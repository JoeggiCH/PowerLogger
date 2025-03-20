#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

const int chipSelect = 10; // D10 auf Nano Every

void setup2() {
    Serial.begin(460800);
    
    // wait for Serial Monitor to connect. Needed for native USB port boards only:
    while (!Serial);

    pinMode(chipSelect, OUTPUT); // change this to 53 on a mega  // don't follow this!!
    digitalWrite(chipSelect, HIGH); // Add this line

    Serial.print(F("SD card initialization..."));
    if (!SD.begin(chipSelect)) {
      Serial.println(F(" failed"));
    } 
    else{
      Serial.println(F(" ok"));
    }

}  

void loop2() {
  delay(1000);
}