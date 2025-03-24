#include <Arduino.h>
#include <Wire.h>
#include <INA226_WE.h>
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>

#define I2C_ADDRESS 0x40
INA226_WE ina226 = INA226_WE(I2C_ADDRESS);

const int chipSelect = 10; // D10 auf Nano Every

// const char INIfilename[] PROGMEM = "LOGGER.INI";

const char INIfilename[] = "LOGGER.INI";
File logfile;
int iter=1;

float freq=1.0;
int delaytime;

// the logger measures when abs(busVoltage)>busVoltageThreshold for more than SwitchTime secs
// the logger stops measuring when abs(busVoltage) is lower for more than SwitchTime secs
float busVoltageThreshold = 6.0;
int SwitchTime=2.0;
int MaxCycles; // calculated below
bool logging=false;
int MeasureCycle=0, NoMeasureCycle=0;

float shuntVoltage_mV = 0.0;
float loadVoltage_V = 0.0;
float busVoltage_V = 0.0;
float current_mA = 0.0;
float power_mW = 0.0;

char status[10];

void FileReadLn(File &ReadFile, char *buffer, size_t len);
void measure_loop();

void setup() {
  File INIFile;

  Serial.begin(460800);
  Serial.println(F("Initializing INA226 ..."));

  Wire.begin();
  ina226.init();
  // the "red" module/shield uses a 0.002 Ohm shunt and supports measurements up to 20A
  ina226.setResistorRange(0.002, 20.0);

  Serial.print(F("Initializing SD card..."));

  if (!SD.begin(chipSelect)) {
    Serial.println(F("failed"));
    while (true);
    }
  else {
    Serial.println(F("ok"));
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
      if (atoi(buffer)>0) {
        iter=atoi(buffer);
      }
      // measurement frequency, e.g. 1.0 -> 1 measurement per second
      FileReadLn(INIFile,buffer,sizeof(buffer));
      if (atof(buffer)>0.0){
        freq=atof(buffer);
      }
      // the voltage threshold
      FileReadLn(INIFile,buffer,sizeof(buffer));
      if (atof(buffer)>0.0) {
        busVoltageThreshold=atof(buffer);
      }

      INIFile.close();
      Serial.print(F("Read inifile with iter="));
      Serial.print(iter);
      Serial.print(F(", freq="));
      Serial.print(freq, 10);
      Serial.print(F(", voltage threshold="));
      Serial.println(busVoltageThreshold, 10);
      iter++;
      SD.remove(INIfilename);
      }
    }
  else {
    Serial.println(F("INI file not found on SD Card!"));
    iter=1;
    freq=1.0;
    }

  freq=10;
  delaytime= 1000/freq;
  MaxCycles=trunc(SwitchTime*1000/delaytime);
  
  Serial.print(F("Writing inifile "));
  Serial.print(INIfilename);
  Serial.print(F(" with iter="));
  Serial.print(iter);
  Serial.print(F(", freq="));
  Serial.print(freq, 10);
  Serial.print(F(", delaytime="));
  Serial.print(delaytime);
  Serial.print(F(", busVoltageThreshold="));
  Serial.println(busVoltageThreshold, 10);
  
  INIFile = SD.open(INIfilename, FILE_WRITE);
  if (INIFile){
    INIFile.println(String(iter));
    INIFile.println(String(freq,10));
    INIFile.println(String(busVoltageThreshold,10));
    INIFile.close();
    }
  else{
    Serial.println(F("issue writing inifile to SD Card"));
    while(true);
  }

  ina226.waitUntilConversionCompleted();
  Serial.println(F("initialization done."));   
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
      
  ina226.readAndClearFlags();
  // is the following necessary ????
  ina226.waitUntilConversionCompleted();

  busVoltage_V = ina226.getBusVoltage_V();

  delay(delaytime);

  Serial.print(F("Bus Voltage [V]: ")); Serial.print(String(busVoltage_V,5));
  Serial.print(F(" - MeasureCycle: ")); Serial.print(String(MeasureCycle));
  Serial.print(F(" - NoMeasureCycle: ")); Serial.println(String(NoMeasureCycle));
  

  if (abs(busVoltage_V)>=busVoltageThreshold || (ina226.overflow)) {
    if (MeasureCycle<MaxCycles+1) {
      MeasureCycle++;
    }
    NoMeasureCycle=0;
  }
  else {
    if (NoMeasureCycle<MaxCycles+1) {
      NoMeasureCycle++;
    }
    MeasureCycle=0;
  }


  if ((MeasureCycle>MaxCycles)) {
    measure_loop();
  }

  if (logging && (NoMeasureCycle>0)&&(NoMeasureCycle<MaxCycles)) {
    measure_loop();
    }

  if (MeasureCycle==MaxCycles) {

    Serial.print(F("starting new logfile iter="));Serial.println(iter);
    logging=true;

    // open new logfile
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
        // wait 10s
        delay(10000);
      } 
  }

  if (logging && (NoMeasureCycle==MaxCycles)) {
    logfile.close();
    iter++;
    logging=false;
    Serial.print(F("closing logfile iter="));Serial.println(iter);
  }
}
  

void measure_loop() {

// Bus Voltage is measured between GND and V+ (of the module, VBUS of the INA226 chip)
// Shunt Voltage is measured between Current- and Current+

  //ina226.readAndClearFlags();
  shuntVoltage_mV = ina226.getShuntVoltage_mV();
  busVoltage_V = ina226.getBusVoltage_V();
  current_mA = ina226.getCurrent_mA();
  power_mW = ina226.getBusPower();

  // "loadVoltage" is the Bus Voltage minus the Shunt Voltage
  
  loadVoltage_V  = busVoltage_V - (shuntVoltage_mV/1000);
    

  if(!ina226.overflow){
      strcpy(status,"ok");  
      }
  else{
    strcpy(status,"overflow");  
    }
  

  //Serial.print(F("status: ")); Serial.println(status);
  //Serial.print(F("Shunt Voltage [mV]: ")); Serial.println(shuntVoltage_mV);
  //Serial.print(F("Bus Voltage [V]: ")); Serial.print(String(busVoltage_V,5));
  //Serial.print(F(" - MeasureCycle: ")); Serial.print(String(MeasureCycle));
  //Serial.print(F(" - NoMeasureCycle: ")); Serial.println(String(NoMeasureCycle));
  //Serial.print(F("Load Voltage [V]: ")); Serial.println(String(loadVoltage_V,5));
  //Serial.print(F("Current[mA]: ")); Serial.println(current_mA);
  //Serial.print(F("Bus Power [mW]: ")); Serial.println(String(power_mW,5));
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
