/*
This sketch implements a data logger - recording voltage, current and power readings
of an INA226 sensor; logging the readings into CSV files on a SD Card.

POWERING THE LOGGER
-------------------
The logger software supports two types of power sources for the Arduino (incl the power
for the the SD card and the INA226 sensor):

A) INDEPENDENT POWER SOURCE: the logger / Arduino is connected to its own power source, which 
is independent of the power source for the load we want to measure.
An independent power source can be a powered USB port, which the Arduino is connected to.
It is also possible to connect an independent power source to the Arduino pins "5V" or "VIN".

B) COMMON POWER SOURCE: the logger and the load to be measured use a common power source. 
One way of achieving this, is to connec the VIN pin of the Arduino to the V+ pin of the INA226 and
the + pin of the common power source.


There are pros and cons of using an INDEPENDENT POWER SOURCE (A)

   PRO: power sources with voltages between 0V and 36V can be measured.

   PRO: the power source for the load can continue to operate without any (major) impact
   of the sensor and its measurements. The only minor impact is caused by the additional
   "shunt" resistor the INA226 requires for its measurement. Minimal impact is desired, 
   for example, if we want to precisely measure the voltage, currents and power of a 
   charger charging an accumulator battery.

   CON: additional wiring, an USB power supply, a battery pack or similar is needed.

For a COMMON POWER SOURCE (B)

   PRO: less space and wiring needed.

   CON: the supported power source voltages are limited by the Arduino, e.g. in the case of the Arduino Nano 
   Every, the recommended VIN power is between 4.5V and 21V (see MPM3610 specification). 



MEASURING 
---------
1) insert a SD card into the SD card slot. 
2) connect the power source for the Arduino/the logger : the logger will be in standby
3) connect the load to the logger : the logger will start measuring/logging

To stop measuring
1) disconnect the load: the logger goes to standby
2) disconnect the power source

#################################### IMPORTANT ####################################
Always disconnect the load FIRST, BEFORE disconnecting the power source for 
the Arduino! Otherwise the logger might loose its power while writing to the SD card, 
which can cause inconsistent information on the SD card file system. In the worst case, 
the SD card will become unreadable and must be formatted again and your measurement data 
is lost!
See also the section "POWERING THE LOGGER" above!

#################################### IMPORTANT ####################################


WIRING FOR MEASUREMENTS
-----------------------
The sensor needs to be connected to the source and the load, two configurations are possible:
a) is called "HIGH SIDE" configuration; 
b) is called "LOW SIDE" configuration

For a), "HIGH SIDE" the wiring is 
-------------+-----------------------+----------------------+
| sensor pin | source connection/pin |  load connection/pin |
-------------+-----------------------+----------------------+
|   V+       |        +              |                      |
-------------+-----------------------+----------------------+
|   V-       |        - / GND        |         - / GND      |
-------------+-----------------------+----------------------+
|   I+       |        +              |                      |
| Current +  |                       |                      |
-------------+-----------------------+----------------------+
|   I-       |                       |         +            |
| Current -  |                       |                      |
-------------+-----------------------+----------------------+

For b), "LOW SIDE" the wiring is 
-------------+-----------------------+----------------------+
| sensor pin | source connection/pin |  load connection/pin |
-------------+-----------------------+----------------------+
|   V+       |        +              |         +            |
-------------+-----------------------+----------------------+
|   V-       |        - / GND        |                      |
-------------+-----------------------+----------------------+
|   I+       |                       |         - / GND      |
| Current +  |                       |                      |
-------------+-----------------------+----------------------+
|   I-       |        - / GND        |                      |
| Current -  |                       |                      |
-------------+-----------------------+----------------------+

If I+, I- are NOT connected, the logger will only measure/log the source voltage.


LOGGER MODES
------------
The logger can be in one of two modes
- STANDBY - waiting for a load to be connected
- MEASURE - actively measuring and writing to the SD

During STANDBY, the pin 13 LED will flash at a slow rate.
During MEASURE, the pin 13 LED will flash at the rate of the measurement frequency

At each transition from STANDBY to MEASURE a new measurement cycle begins and
a file with the name 'logNNNNN.csv' is created on the SD card; NNNNN indicates 
the number of the measurement cycle; the number is increased for each cycle. 

At each transition from MEASURE to STANDBY, the measurement file is closed and
the measurement cycle finishes.

During STANDBY mode the logger keeps checking the sensor for a load threshold 
voltage, above the load threshold voltage measurements will start, i.e. the mode changes
to "MEASURE" Below the load threshold voltage the logger assumes that the load is not 
connected yet and the STANDBY mode will continue.


CONFIGURATION FILE ON SD CARD
-----------------------------
The logger uses a configuration file called LOGGER.INI, in the root folder of the SD card.
The file is a text file containing one value (a string representing a number) per line.
If there is no LOGGER.INI file on the SD card, the logger will create it - using default values.

First line: next measurement cycle number, an integer. With each transition from "standby" to "measure"
the logger will increase this number. DEFAULT: 1

Second line: measurement frequency, a float. A value of 1.0 means that one measurement is done per second. 
A frequency of 10 means that 10 measurements are made/logged per second. Fractions are supported as well, e.g. 
a value of 0.1 means that every 10 seconds a value is logged. DEFAULT: 1.0

Third line: load threshold voltage, a float. The logger will start measuring/logging data, if the load 
voltage is above this value. See also the section "POWERING THE LOGGER" above.
DEFAULT : 6


*/

#include <Arduino.h>
#include <Wire.h>
#include <INA226_WE.h>
#define I2C_ADDRESS 0x40
INA226_WE ina226 = INA226_WE(I2C_ADDRESS);

#include <SPI.h>
#include <SD.h>

const int chipSelect = 10; // D10 auf Nano Every

const String INIfilename="LOGGER.INI";
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

String FileReadLn(File &ReadFile);

void setup() {
  File INIFile;
  Serial.begin(460800);
  
  Serial.println("Initializing INA226 ...");
  Wire.begin();
  ina226.init();
  // joerg's board uses a 0.002 Ohm shunt and supports measurements up to 20A
  ina226.setResistorRange(0.002, 20.0);

  Serial.println("Initializing SD card...");

  if (!SD.begin(chipSelect)) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("1. is a card inserted?");
    Serial.println("2. is your wiring correct?");
    Serial.println("3. did you change the chipSelect pin to match your shield or module?");
    Serial.println("Note: press reset button on the board and reopen this Serial Monitor after fixing your issue!");
    //while (true);
  }

  if (SD.exists(INIfilename)){
    INIFile = SD.open(INIfilename, FILE_READ);
    if (INIFile.size()>500){
      Serial.println("Ini file too big; writing a new one");
      iter=1;
      freq=1.0;
      INIFile.close();
      SD.remove(INIfilename);
      }
    else {
      // reading the current next measurement cycle number to use
      iter=FileReadLn(INIFile).toInt();
      // the next measurement frequency, e.g. 1.0 -> 1 measurement per second
      freq=FileReadLn(INIFile).toFloat();
      INIFile.close();
      Serial.println("Read inifile with iter="+String(iter)+", freq="+String(freq,10));
      iter++;
      SD.remove(INIfilename);
      }
    }
  else {
    Serial.println("INI file not found on SD Card!");
    Serial.println("Creating new file called logger.ini with two lines"); 
    Serial.println("- the first line containing the measurement file number to use next");
    Serial.println("- the second line containing the number of measurements per second (floats <1 are ok!)");
    iter=1;
    freq=1.0;
    }

  Serial.println("Writing inifile with iter="+String(iter)+", freq="+String(freq,10));

  freq=100.0;
  delaytime= 1000/freq;
  Serial.println("Current freq="+String(freq,10)+", delaytime="+String(delaytime));
  
  INIFile = SD.open(INIfilename, FILE_WRITE);
  if (INIFile){
    INIFile.println(String(iter));
    INIFile.println(String(freq,10));
    INIFile.close();
    }
  else{
    Serial.println("issue writing inifile to SD Card");
    while(true);
  }

  char buf[40]; sprintf(buf,"%05d",iter);
  char logfn[20]="log"; strcat(logfn,buf); strcat(logfn,".csv");
  strcpy(buf,"Writing to "); strcat(buf,logfn);
  Serial.println(buf);
  String S2=String(logfn);
  logfile=SD.open(S2,FILE_WRITE);
  if (logfile){
    logfile.println("millis,micros,status,Load_Voltage,Current_mA, load_Power_mW");
  }
  else{
    Serial.println("issue writing logfile to SD Card");
    while(true);
  }

  ina226.waitUntilConversionCompleted();
  Serial.println("initialization done.");   
  Serial.println("INA226 Current Sensor - Continuous Measurements & Logging");
}

String FileReadLn(File &ReadFile){
    String line="";

    while (ReadFile.available()) {
      char c=ReadFile.read();
      if (c==0x0a){
        return line;
      }
      else {
        line=line+c;
      }
    }
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
    //Serial.println("Values OK - no overflow");
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