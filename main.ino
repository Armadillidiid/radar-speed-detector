#include <LiquidCrystal_I2C.h>

#define bufferSize 100
#define bufferValidSize 5
#define bufferFilterFactor 50.f
#define resultsBufferSize 10

volatile unsigned long lastPulse = 0;
volatile unsigned long buffer[bufferSize] = {0};
volatile unsigned short bufferIndex = 0;

unsigned int results[resultsBufferSize] = {0};
unsigned short resultsIndex = 0;
unsigned short resultsCount = 0;
unsigned short resultsLastTime = 0;

LiquidCrystal_I2C lcd(0x27, 16, 2);
bool lcdScreenUpdateState = false;
unsigned long lcdScreenUpdateTime = 0;
float lcdScreenUpdateSpeedAve = 0;
float lcdScreenUpdateSpeedMax = 0;


void isr() {
  if(bufferIndex < bufferSize) {
    buffer[bufferIndex] = 1000000.0 / (micros() - lastPulse);
    bufferIndex++;
    lastPulse = micros();
  }
}

void updateScreenValues(float max, float ave){
  lcd.setCursor(5,0);
  lcd.print("           ");
  if(max > 0){
    lcd.setCursor(5,0);
    lcd.print(String(max, 1) + " km/h");
  }
  lcd.setCursor(5,1);
  lcd.print("           ");
  if(ave > 0){
    lcd.setCursor(5,1);
    lcd.print(String(ave, 1) + " km/h");
  }
}

void setup() {
  Serial.begin(9600);

  lcd.init();
  lcd.clear();        
  lcd.backlight(); 

  lcd.setCursor(0, 0);
  lcd.print("SPEED");
  lcd.setCursor(0, 1);
  lcd.print("DETECTOR");
  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Max:");
  lcd.setCursor(0, 1);
  lcd.print("Ave:");

  lastPulse = micros();
  attachInterrupt(digitalPinToInterrupt(3), isr, FALLING);
}

void loop() {
  if(bufferIndex > bufferValidSize){
    unsigned long sum = buffer[0];
    unsigned short startIndex = 0;
    bool refreshBuffer = false;

    for(int i = 1; i < bufferIndex && !refreshBuffer; i++){
      sum += buffer[i];

      long diff = abs(buffer[i] - buffer[i - 1]);
      long maxDiff = buffer[i - 1] * (buffer[i - 1] / bufferFilterFactor);
      if(abs(diff) > maxDiff){
        sum = buffer[i];
        startIndex = i;
        if(i <= bufferIndex - bufferValidSize)
          refreshBuffer = true;
      }

      if(i - startIndex + 1 >= bufferValidSize){
        float frequency = (1.0 * sum / (i - startIndex + 1));

        if(millis() - resultsLastTime > 1500){
          resultsCount = 0;
          resultsIndex = 0;
        }
        resultsLastTime = millis();
        results[resultsIndex] = frequency;
        resultsIndex = (resultsIndex + 1) % resultsBufferSize;
        resultsCount = min(resultsCount + 1, resultsBufferSize);

        lcdScreenUpdateSpeedAve = 0.f;
        for(short i = 0; i < resultsCount; i++){
          float speed = results[i] / 19.49; //19.49 from datasheet for km/hr;

          lcdScreenUpdateSpeedAve += speed;
          if(speed > lcdScreenUpdateSpeedMax)
            lcdScreenUpdateSpeedMax = speed;
        }
        lcdScreenUpdateSpeedAve /= resultsCount;
        lcdScreenUpdateState = true;
        
        Serial.println(String("speed(ave): ") + lcdScreenUpdateSpeedAve + " km/h");
        Serial.println(String("speed(max): ") + lcdScreenUpdateSpeedMax + " km/h");
        Serial.println("-----------------------");
        refreshBuffer = true;
      }

      if(i == bufferSize - 1)
        refreshBuffer = true;
    }

    if(refreshBuffer)
      bufferIndex = 0;
  }

  if(lcdScreenUpdateState && millis() - lcdScreenUpdateTime > 500){
    lcdScreenUpdateState = false;
    updateScreenValues(lcdScreenUpdateSpeedMax, lcdScreenUpdateSpeedAve);
    lcdScreenUpdateTime = millis();
  }

  if(lcdScreenUpdateTime > 0 && millis() - lcdScreenUpdateTime > 6000){
    lcdScreenUpdateTime = lcdScreenUpdateSpeedMax = lcdScreenUpdateSpeedAve = 0;
    lcdScreenUpdateState = false;
    updateScreenValues(0, 0);
  }
}
