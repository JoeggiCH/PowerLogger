#include <Arduino.h>
#include <Wire.h>
#include <INA226_WE.h>
#include <SPI.h>
#include <SD.h>
#include <RtcDS1307.h>

extern RtcDS1307<TwoWire> Rtc;
extern void printDateTime(const RtcDateTime& dt);
extern bool wasError(const char* errorTopic = "");
extern void rtcsetup ();

// for INA226
#define I2C_ADDRESS 0x40
INA226_WE ina226 = INA226_WE(I2C_ADDRESS);

// for SPI bus used by Data Logging Module, i.e. SD Card and RTC
const int chipSelect = 10; // D10 auf Nano Every

const char INIfilename[] = "LOGGER.INI";
File logfile;

// iter is the logfile "generation", a sequence number which 
// is increased with each logfile produced
// logfiles are started when the logging threshold conditions were met for several cycles
// logfiles are closed when logging threshold conditions are no longer met
int iter;

// the logger logs when abs(busVoltage)>busVoltageThreshold AND abs(current) > currentThreshold 
// and the condition is true for more than SwitchTime secs
// setting threshold values to 0.0 means the condition is always met
// values are read from the INI file
float busVoltageThreshold;
float currentThreshold;

// used to count the number of cycles (not) meeeting the threshold conditions
int CyclesCondMet=0, CyclesCondNotMet=0;

// values measured by INA226
float shuntVoltage_mV = 0.0;
float loadVoltage_V = 0.0;
float busVoltage_V = 0.0;
float current_mA = 0.0;
float power_mW = 0.0;

// Other global variables
int SwitchTime=2.0;
int MaxCycles;
bool logging=false;
char status[10];
float freq=1.0;
unsigned long delaytime;
unsigned long StartOfLoopMicros;

void FileReadLn(File &ReadFile, char *buffer, size_t len);
void measure_loop();
void reboot() { asm volatile ("jmp 0"); }

void setup() {
  
  Serial.begin(460800);

  Serial.println();Serial.println();
  Serial.println(F("Initializing INA226 ..."));
  Wire.begin();
  ina226.init();
  // the "red" module/shield uses a 0.002 Ohm shunt and supports measurements up to 20A
  ina226.setResistorRange(0.002, 20.0);
  ina226.setCorrectionFactor(0.947818013);
  ina226.readAndClearFlags();
  ina226.waitUntilConversionCompleted();

  Serial.println(F("Initializing DS1307 ..."));
  rtcsetup();

  Serial.print(F("Initializing SD card..."));
  if (!SD.begin(chipSelect)) {
    Serial.println(F("failed"));
    delay(10000);
    reboot();
    }
  else {
    Serial.println(F("ok"));
    }

  // INI File Handling : read current values, ensure integrity, check file available
  File INIFile;

  if (SD.exists(INIfilename)){
    INIFile = SD.open(INIfilename, FILE_READ);

    if (INIFile.size()<=500) {
      char buffer[16];
      // reading the last logfile generation number used

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
      // the current threshold
      FileReadLn(INIFile,buffer,sizeof(buffer));
      if (atof(buffer)>0.0) {
        currentThreshold=atof(buffer);
      }

      INIFile.close();
      Serial.print(F("Read "));
      Serial.print(INIfilename);
      Serial.print(F(" with iter="));
      Serial.print(iter);
      Serial.print(F(", freq="));
      Serial.print(freq, 10);
      Serial.print(F(", voltage threshold="));
      Serial.print(busVoltageThreshold, 10);
      Serial.print(F(", current threshold="));
      Serial.println(currentThreshold, 10);

      iter++;
      }
    else {    
      Serial.println(F("Ini file too big; will write a new INI file"));
      iter=1;
      freq=1.0;
      busVoltageThreshold=0.0;
      currentThreshold=20.0;    
      INIFile.close();
      }
    }
  else {
    Serial.println(F("No INI file on SD Card!"));
    iter=1;
    freq=1.0;
    }
  
  // TEMP
  // temporary definitions, so I don't have to use the INI file for configuring it
  freq=0.2;
  busVoltageThreshold=5.0;
  currentThreshold=10.0;
  // end of temp section
  // TEMP
  
  // initialize global values
  delaytime= 1000000/freq;
  MaxCycles=max(1,trunc(SwitchTime*freq));

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
  StartOfLoopMicros=micros();
  
  busVoltage_V = ina226.getBusVoltage_V();
  current_mA = -ina226.getCurrent_mA();

  if ((abs(busVoltage_V)>=busVoltageThreshold && (abs(current_mA))>=currentThreshold) || (ina226.overflow)) {
    // condition met, so CyclesCondMet is increased up to a maximum of MaxCycles+1
    if (CyclesCondMet<MaxCycles+1) {
      CyclesCondMet++;
    }
    CyclesCondNotMet=0;
    //Serial.print("X");
    }
  else {
    // threshold conditions not met
    // CyclesCondNotMet increased up to a maximum of MaxCycles+1
    if (CyclesCondNotMet<MaxCycles+1) {
      CyclesCondNotMet++;
      }
    CyclesCondMet=0;
    //Serial.print("_");
    } 

  if (!logging && (CyclesCondMet==MaxCycles)) {
    // We weren't logging so far, but for MaxCycles the logging conditions were met
    // so we transition to logging 

    logging=true;
    CyclesCondMet=MaxCycles+1;

    // handle invalid RTC info
    if (!Rtc.IsDateTimeValid()) 
    {
        if (!wasError("IsDateTimeValid in loop()"))
        {
            // Common Causes:
            //    1) the battery on the device is low or even missing and the power line was disconnected
            Serial.println(F("Lost confidence in RTC DateTime!"));
        }
    }

    // prep datestring for logging
    char datestring[21];
    RtcDateTime now = Rtc.GetDateTime();
    if (!wasError("GetDateTime in loop")) {

      snprintf_P(datestring, 
              countof(datestring),
              PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
              now.Day(),
              now.Month(),
              now.Year(),
              now.Hour(),
              now.Minute(),
              now.Second() );
    }

    // open new logfile
    char logfn[20];
    snprintf(logfn, sizeof(logfn), "log%05d.csv", iter);

    logfile=SD.open(logfn,FILE_WRITE);
    if (logfile){
      Serial.print(F("\nWriting to "));
      Serial.println(logfn);  
      Serial.println(datestring);
      logfile.print(F("Data measured from, "));
      logfile.println(datestring);
      logfile.println(F("millis,micros,status,Load_Voltage,Current_mA, load_Power_mW"));
      }
      else{
        Serial.println(F("issue writing logfile to SD Card"));
        // wait 10s; usually the SD card is missing, let's wait 10 secs 
        // and then re-start like after a hardware reset
        delay(10000);
        reboot();
      } 

    // write updated INI File; 
    // include updated iter value - so, logfile names remain unique
    SD.remove(INIfilename);
    File INIFile;
    INIFile = SD.open(INIfilename, FILE_WRITE);
    if (INIFile){
      Serial.print(F("Writing inifile "));
      Serial.print(INIfilename);
      Serial.print(F(" with iter="));
      Serial.print(iter);
      Serial.print(F(", freq="));
      Serial.print(freq, 10);
      Serial.print(F(", busVoltageThreshold="));
      Serial.print(busVoltageThreshold, 10);
      Serial.print(F(", currentThreshold="));
      Serial.println(currentThreshold, 10);
      INIFile.println(String(iter));
      INIFile.println(String(freq,10));
      INIFile.println(String(busVoltageThreshold,10));
      INIFile.println(String(currentThreshold,10));
      INIFile.close();
      }
    else{
      Serial.println(F("issue writing inifile to SD Card"));
      delay(10000);
      reboot();
    }

  }

  if (logging && (CyclesCondNotMet==MaxCycles)) {
    // We are currently logging, however, for MaxCycles the logging conditions were not met
    // so we transition to the non-logging state, close the logfile, increase logfile generation number iter
    // and set CyclesCondNotMet is set to MaxCycles+1
    Serial.println(F("\nclosing logfile"));
    logfile.close();
    iter++;
    CyclesCondNotMet=MaxCycles+1;
    logging=false;
  }


// after all this management we do some real work :-)
if (logging) {
  measure_loop();
  }

// All kinds of serial output to support troubleshooting
Serial.print(" logging ");
Serial.print(logging);
Serial.print(" logfile # ");
Serial.print(iter);
Serial.print(F(" CyclesCondMet: ")); Serial.print(String(CyclesCondMet));
Serial.print(F(" CyclesCondNotMet: ")); Serial.print(String(CyclesCondNotMet));
Serial.print(F(" Bus[V]: ")); Serial.print(String(busVoltage_V,5));
Serial.print(F(" Current[mA]: ")); Serial.print(current_mA);
Serial.println();

// Calculate the elapsed time since the start of the loop
// and delay the rest of the loop time
// to ensure the loop time is equal to delaytime  
unsigned long NowMicros=micros();
unsigned long MicrosElapsed;
if (NowMicros>StartOfLoopMicros) {
  MicrosElapsed=NowMicros-StartOfLoopMicros;
  }
else {
  // we have a rollover of the micros() counter
  MicrosElapsed=4294967295-StartOfLoopMicros+NowMicros;
  }

  if (MicrosElapsed<delaytime) {
  unsigned long RemainingDelay=delaytime-MicrosElapsed;
  if (RemainingDelay>16383) {
    // delayMicroseconds() only works well up to 16383 microseconds
    // so we have to split the delay into two parts
    delay(RemainingDelay/1000);
    RemainingDelay=RemainingDelay%1000;
    delayMicroseconds(RemainingDelay);
    }
  else {
    delayMicroseconds(delaytime-MicrosElapsed);
    }
  }
}
  

void measure_loop() {

// Bus Voltage is measured between GND and V+ (of the module, VBUS of the INA226 chip)
// Shunt Voltage is measured between Current- and Current+

  //ina226.readAndClearFlags();
  shuntVoltage_mV = ina226.getShuntVoltage_mV();
  busVoltage_V = ina226.getBusVoltage_V();
  current_mA = -ina226.getCurrent_mA();
  power_mW = ina226.getBusPower();

  // "loadVoltage" is the Bus Voltage minus the Shunt Voltage
  
  loadVoltage_V  = busVoltage_V - (shuntVoltage_mV/1000);
    

  if(!ina226.overflow){
      strcpy(status,"ok");  
      }
  else{
    strcpy(status,"overflow");  
    }
  
  logfile.print(millis());                logfile.print(",");
  logfile.print(micros());                logfile.print(",");
  logfile.print(status);                  logfile.print(",");
  logfile.print(String(loadVoltage_V,5)); logfile.print(",");
  logfile.print(String(current_mA,5));    logfile.print(",");
  logfile.print(String(power_mW,5));      logfile.println();

  logfile.flush();
}
