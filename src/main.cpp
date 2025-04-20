#include <Arduino.h>
#include <Wire.h>
#include <INA226_WE.h>
#include <SPI.h>
#include <SD.h>
#include <RtcDS1307.h>
#include <avr/pgmspace.h>

extern RtcDS1307<TwoWire> Rtc;
extern void printDateTime(const RtcDateTime& dt);
extern bool wasError(const char* errorTopic = "");
extern void rtcsetup (char const *compile_date, char const *compile_time); 


// for INA226
#define I2C_ADDRESS 0x40
INA226_WE ina226 = INA226_WE(I2C_ADDRESS);

// for SPI bus used by Data Logging Module, i.e. SD Card and RTC
const int chipSelect = 10; // D10; seems to correspond to D13 on Nano Every

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

// values measured by INA226
float shuntVoltage_mV = 0.0;
float loadVoltage_V = 0.0;
float busVoltage_V = 0.0;
float current_mA = 0.0;
float power_mW = 0.0;

// frequency of the measurements
float freq=1.0;

//delaytime is the time between two measurements, i.e. is calculated as 1 / freq (in microseconds). 
unsigned long delaytime;

// Other global variables
int SwitchTime=2.0;
int MaxCycles;
bool logging=false;
char status[10];
unsigned long StartOfLoopMicros;
// used to count the number of cycles (not) meeeting the threshold conditions
int CyclesCondMet=0, CyclesCondNotMet=0;


/* 
The code below, up to and including findEnumsMaxProductBelowThreshold(), is used to set 
the average mode and conversion time of the INA226 chip, based on a given delaytime. 

The idea is to run the INA226 in "TRIGGERED" mode, and to optimally use the available delaytime
to collect as many samples as possible. The INA226 averages all samples and these averages are 
logged to the SD card.

Interestingly averaging in the INA226 happens on two levels - on the level of the Delta Sigma ADC, 
which operates at a frequency of 500kHz, collects multiple samples during a "conversion" and 
averages them electronically.The time allowed for the conversion is the conversionTime, CT.
The results of each conversion are then averaged on a second level using a digital calculation 
function. The number of second level averages is controlled by the averageMode, AVG.

The function findEnumsMaxProductBelowThreshold()searches for the maximum product of the 
AVG and CT array components, which is below the delaytime. The results are then mapped to the 
corresponding enum values used in the INA226_WE library.
*/
const byte VECTOR_SIZE = 8;
const byte MAX_IDX = VECTOR_SIZE - 1;

const unsigned int AVG_VALUES[VECTOR_SIZE] PROGMEM = {
    1, 4, 16, 64, 128, 256, 512, 1024
};
const unsigned int CT_VALUES[VECTOR_SIZE] PROGMEM = {
    140, 204, 332, 588, 1100, 2116, 4156, 8244
};

const unsigned int AVG_ENUMS[VECTOR_SIZE] PROGMEM = {
    AVERAGE_1, AVERAGE_4, AVERAGE_16, AVERAGE_64,
    AVERAGE_128, AVERAGE_256, AVERAGE_512, AVERAGE_1024
};
const unsigned int CT_ENUMS[VECTOR_SIZE] PROGMEM = {
    CONV_TIME_140, CONV_TIME_204, CONV_TIME_332, CONV_TIME_588,
    CONV_TIME_1100, CONV_TIME_2116, CONV_TIME_4156, CONV_TIME_8244
};

const long MIN_PRODUCT_VAL = 140L;       // Calculated from 1 * 140
const long MAX_PRODUCT_VAL = 8441856L;   // Calculated from 1024 * 8244

void findEnumsMaxProductBelowThreshold(long threshold, averageMode* result_avg_enum, convTime* result_ct_enum) {

    threshold = threshold / 2.0;

    byte best_i = 0;
    byte best_j = 0;

    if (threshold <= MIN_PRODUCT_VAL) {
        // Indices remain 0, 0
    } else if (threshold >= MAX_PRODUCT_VAL) {
        best_i = MAX_IDX;
        best_j = MAX_IDX;
    } else {
        long maxProductBelowThreshold = MIN_PRODUCT_VAL;

        for (byte i = 0; i < VECTOR_SIZE; ++i) {
            unsigned int avg_val = pgm_read_word_near(AVG_VALUES + i);

            for (byte j = 0; j < VECTOR_SIZE; ++j) {
                unsigned int ct_val = pgm_read_word_near(CT_VALUES + j);
                long currentProduct = (long)avg_val * (long)ct_val;

                if (currentProduct < threshold && currentProduct > maxProductBelowThreshold) {
                    maxProductBelowThreshold = currentProduct;
                    best_i = i;
                    best_j = j;
                }
            }
        }
    }

    *result_avg_enum = (averageMode)pgm_read_word_near(AVG_ENUMS + best_i);
    *result_ct_enum = (convTime)pgm_read_word_near(CT_ENUMS + best_j);
}

// used for INIFile ingestion
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

// this works on the Arduino Nano Every; compatibility with other boards is not guaranteed
void reboot() { asm volatile ("jmp 0"); }

void setup() {
  
  Serial.begin(460800);
  while (!Serial);
  Serial.println();Serial.println();

  Serial.print(F("Initializing DS1307 ..."));
  rtcsetup(__DATE__,__TIME__);

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
      // the Bus voltage threshold
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
      Serial.println(F("INI file too big; will write a new INI file"));
      iter=1;
      freq=1.0;
      busVoltageThreshold=0.0;
      currentThreshold=10.0;    
      INIFile.close();
      }
    }
  else {
    Serial.println(F("No INI file on SD Card!"));
    iter=1;
    freq=1.0;
    }
  
  // Temporary definitions, so I don't have to use the INI file during debugging
  // freq=50.0;
  // busVoltageThreshold=3.0;
  // currentThreshold=0.0;
  // end of TEMP section
  
  // initialize global values
  delaytime= 1000000/freq;
  MaxCycles=max(1,trunc(SwitchTime*freq));
  Serial.print(F("delaytime: "));
  Serial.print(delaytime);
  Serial.println(F(" microseconds"));

  Serial.print(F("Initializing INA226 ..."));
  Wire.begin();
  ina226.init();
  // the "red" module/shield uses a 0.002 Ohm shunt and supports measurements up to 20A
  ina226.setResistorRange(0.002, 20.0);
  // correction factor for my "red" module/shield 
  ina226.setCorrectionFactor(0.947818013);
  ina226.readAndClearFlags();
  ina226.waitUntilConversionCompleted();

  // find the longest product of AVG and CT which is below delaytime
  averageMode avgResult;
  convTime ctResult;
  // The value 7500 is specific for my hardware!
  // During a measurement cycle the INA226 measurements and the SD write operation take most of the time.
  // My hardware needs around 7500 OR 14000 microseconds to write to the SD library. My understanding is that the lower value
  // is when the SD library buffers the data and the higher value is when the SD library actually writes to the SD card.
  // So, using 7500 microseconds here means that every few measurements - when actual SD card writes happen - the delaytime can not be met.
  findEnumsMaxProductBelowThreshold(delaytime-7500, &avgResult, &ctResult);
  ina226.setAverage(avgResult);
  ina226.setConversionTime(ctResult);
  Serial.print("  AVG (HEX): 0x");
  Serial.print(avgResult, HEX);
  Serial.print("  CT (HEX): 0x");
  Serial.print(ctResult, HEX);
  ina226.setMeasureMode(TRIGGERED);
  Serial.println(F(" - ok"));   
  Serial.println(F("\nStarting Measurements..."));
}

void loop() {
  StartOfLoopMicros=micros();

  // Trigger measurement and fetch results
    ina226.startSingleMeasurement();
    ina226.readAndClearFlags();

    // Bus Voltage is measured between GND and V+ (of the module, VBUS of the INA226 chip)
    // Shunt Voltage is measured between Current- and Current+
    busVoltage_V = ina226.getBusVoltage_V();
    current_mA = -ina226.getCurrent_mA();
    shuntVoltage_mV = ina226.getShuntVoltage_mV();
    power_mW = ina226.getBusPower();

    // "loadVoltage" is the Bus Voltage minus the Shunt Voltage
    loadVoltage_V  = busVoltage_V - (shuntVoltage_mV/1000);

    if(!ina226.overflow){
        strcpy(status,"ok");  
        }
    else{
      strcpy(status,"overflow");  
      }
  
  // determine when to start/ stop logging; manage transitions
    if ((abs(busVoltage_V)>=busVoltageThreshold && (abs(current_mA))>=currentThreshold) || (ina226.overflow)) {
      // condition met, so CyclesCondMet is increased up to a maximum of MaxCycles+1
      if (CyclesCondMet<MaxCycles+1) {
        CyclesCondMet++;
      }
      CyclesCondNotMet=0;
      }
    else {
      // threshold conditions not met
      // CyclesCondNotMet increased up to a maximum of MaxCycles+1
      if (CyclesCondNotMet<MaxCycles+1) {
        CyclesCondNotMet++;
        }
      CyclesCondMet=0;
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

      // prep datestring for the logfile header
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
        // ensure at least the header is written to the SD card
        logfile.flush();
        }
        else{
          Serial.println(F("issue writing logfile to SD Card"));
          // usually the SD card is missing, let's wait 10 secs 
          // and then reboot
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
      logfile.print(millis());                logfile.print(",");
      logfile.print(micros());                logfile.print(",");
      logfile.print(status);                  logfile.print(",");
      logfile.print(String(loadVoltage_V,5)); logfile.print(",");
      logfile.print(String(current_mA,5));    logfile.print(",");
      logfile.print(String(power_mW,5));      logfile.println();

      // flushing takes around 3..7ms, so we rely on the SD library to flush when 
      // the respective buffer ( a sector ?) is full - unless we have a delaytime > 500ms
      if (delaytime>500000) {
        logfile.flush();
        }
      }

  // if we have enough time, we print measurements to the serial monitor as well
    if (delaytime>=500000){
      Serial.print(" logging ");
      Serial.print(logging);
      Serial.print(" logfile # ");
      Serial.print(iter);
      Serial.print(F(" CyclesCondMet: ")); Serial.print(String(CyclesCondMet));
      Serial.print(F(" CyclesCondNotMet: ")); Serial.print(String(CyclesCondNotMet));
      Serial.print(F(" Bus[V]: ")); Serial.print(String(busVoltage_V,5));
      Serial.print(F(" Current[mA]: ")); Serial.print(current_mA);
      Serial.println();
      }

  // determine how much time we have left in the loop and how long to wait
  // time up to this point can vary quite a bit, depending on the INA226 settings, SD card write operations etc

    // Calculate the elapsed time since the start of the loop
    // and delay the rest of the loop time to ensure the loop time is equal to delaytime  
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
      // we have time left in the loop!
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
        //Serial.print("!");
        //Serial.println(MicrosElapsed);
        //Serial.print("!");
      }
    else {
      // delaytime was too short
      Serial.print("X");
      // Serial.println(MicrosElapsed);
      }
  } // end of loop()
  