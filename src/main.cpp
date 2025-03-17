#include <Arduino.h>
#include <Wire.h>
#include <INA226_WE.h>
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>

#define I2C_ADDRESS 0x40
INA226_WE ina226 = INA226_WE(I2C_ADDRESS);

const int chipSelect = 10; // D10 auf Nano Every

const char INIfilename[] PROGMEM = "LOGGER.INI";
File logfile;
int iter;
float freq;
int delaytime;

float shuntVoltage_mV = 0.0;
float loadVoltage_V = 0.0;
float busVoltage_V = 0.0;
float current_mA = 0.0;
float power_mW = 0.0;

int mnum=1;
char status[10];

void FileReadLn(File &ReadFile, char *buffer, size_t len);

void setup() {
  File INIFile;
  Serial.begin(460800);
  
  Serial.println(F("Initializing INA226 ..."));
  Wire.begin();
  ina226.init();
  // joerg's board uses a 0.002 Ohm shunt and supports measurements up to 20A
  ina226.setResistorRange(0.002, 20.0);

  Serial.println(F("Initializing SD card..."));

  if (!SD.begin(chipSelect)) {
    Serial.println(F("initialization failed"));
    while (true);
  }

  if (SD.exists(INIfilename)){
    INIFile = SD.open(INIfilename, FILE_READ);
    if (INIFile.size()>500){
      Serial.println(F("Ini file too big; writing a new one"));
      iter=1;
      freq=1.0;
      INIFile.close();
      SD.remove(INIfilename);
      }
    else {
      // reading the current next measurement cycle number to use
      char buffer[16];
      FileReadLn(INIFile, buffer, sizeof(buffer));
      iter=atoi(buffer);
      // the next measurement frequency, e.g. 1.0 -> 1 measurement per second
      FileReadLn(INIFile,buffer,sizeof(buffer));
      freq=atof(buffer);
      INIFile.close();
      Serial.print(F("Read inifile with iter="));
      Serial.print(iter);
      Serial.print(F(", freq="));
      Serial.println(freq, 10);
      iter++;
      SD.remove(INIfilename);
      }
    }
  else {
    Serial.println(F("INI file not found on SD Card!"));
    iter=1;
    freq=1.0;
    }

  Serial.print(F("Writing inifile with iter="));
  Serial.print(iter);
  Serial.print(F(", freq="));
  Serial.println(freq, 10);

  freq=2.0;
  delaytime= 1000/freq;
  
  Serial.print(F("Current freq="));
  Serial.print(freq, 10);
  Serial.print(F(", delaytime="));
  Serial.println(delaytime);
  
  INIFile = SD.open(INIfilename, FILE_WRITE);
  if (INIFile){
    INIFile.println(String(iter));
    INIFile.println(String(freq,10));
    INIFile.close();
    }
  else{
    Serial.println(F("issue writing inifile to SD Card"));
    while(true);
  }

  char logfn[20];
  snprintf(logfn, sizeof(logfn), "log%05d.csv", iter);
  
  Serial.print(F("Writing to "));
  Serial.println(logfn);

  logfile=SD.open(logfn,FILE_WRITE);
  if (logfile){
    logfile.println(F("millis,micros,status,Load_Voltage,Current_mA, load_Power_mW"));
  }
  else{
    Serial.println(F("issue writing logfile to SD Card"));
    while(true);
  }

  ina226.waitUntilConversionCompleted();
  Serial.println(F("initialization done."));   
  Serial.println(F("INA226 Current Sensor - Continuous Measurements & Logging"));
}

void FileReadLn(File &ReadFile, char *buffer, size_t len) {
  size_t index = 0;
  while (ReadFile.available() && index < len - 1) {
      char c = ReadFile.read();
      if (c == '\n') {
          break;
      }
      buffer[index++] = c;
  }
  buffer[index] = '\0';
}


void loop() {

// Joerg: Bus Voltage, in High Side configuration, is the same as the voltage over
// the power consumer we want to measure; the total voltage supplied equals the shunt voltage 
// plus the bus Voltage. 
// 
// In Low Side configuration, Bus voltage equals the total voltage supplied.


  ina226.readAndClearFlags();
  shuntVoltage_mV = ina226.getShuntVoltage_mV();
  busVoltage_V = ina226.getBusVoltage_V();
  current_mA = ina226.getCurrent_mA();
  power_mW = ina226.getBusPower();

  // Joerg: "loadVoltage" confused me, because, to me, the "load" is the power consumer, not 
  // the power source.
  // Using the example values from Figure 8-1, section 6.5.1 of the data sheet: 
  // Total voltage supplied is 12V (=power source), voltage across the load consuming 10A 
  // is 11.98V, the shunt voltage is 0.02V (2 mOhm Shunt).
  loadVoltage_V  = busVoltage_V + (shuntVoltage_mV/1000);
  
  /**
  Serial.print("Shunt Voltage [mV]: "); Serial.println(shuntVoltage_mV);
  Serial.print("Bus Voltage [V]: "); Serial.println(String(busVoltage_V,5));
  Serial.print("Load Voltage [V]: "); Serial.println(String(loadVoltage_V,5));
  Serial.print("Current[mA]: "); Serial.println(current_mA);
  Serial.print("Bus Power [mW]: "); Serial.println(String(power_mW,5));
  */

  if(!ina226.overflow){
    //Serial.println(F("Values OK - no overflow"));
      strcpy(status,"ok");  
      }
  else{
    //Serial.println("Overflow! Choose higher current range");
    strcpy(status,"overflow");  
    }
  //Serial.println();

  logfile.print(millis());                logfile.print(",");
  logfile.print(micros());                logfile.print(",");
  logfile.print(status);                  logfile.print(",");
  logfile.print(String(loadVoltage_V,5)); logfile.print(",");
  logfile.print(String(current_mA,5));    logfile.print(",");
  logfile.print(String(power_mW,5));      logfile.println();

  logfile.flush();
  delay(delaytime);
}