# PowerLogger

This project implements a data logger for electrical voltage, current and power readings. 

The logger consists of
- a TI INA226 chip to measure the electrical values,
- an Arduino Nano Every as micro controller,
- a SD holder/card to which the measured values are written in CSV format,
- an ADI DS1307 Real-Time-Clock to supply date/time information.

## Measuring

1) insert a SD card into the SD card slot. 
2) connect the power source for the Arduino/the logger : the logger will be in standby
3) connect the load to the logger : the logger will start measuring/logging after "SwitchTime" seconds

To stop measuring
1) disconnect the load: the logger continues measuring/logging for "SwitchTime" seconds and then stops
2) disconnect the power source

### IMPORTANT
Always disconnect the load FIRST, BEFORE disconnecting the power source for 
the Arduino! Otherwise the logger might loose its power while writing to the SD card, 
which can cause inconsistent information on the SD card file system. In the worst case, 
the SD card will become unreadable, must be formatted and the SD card data 
is lost!

## Wiring for Measurements

The INA226 sensor needs to be connected to the source and the load, two configurations are possible:

<img src=./images/HiLo%20Wiring.png width="960">


If Current+, Current- are NOT (both) connected, the logger will only measure/log the source voltage.

![Diagram](/images/FullDiagram.png)

## Logger Modes

The logger can be in one of two modes
- STANDBY - waiting for a load to be connected
- LOGGING - measuring and writing to the SD

During STANDBY, the pin 13 LED will not flash.
During LOGGING, the pin 13 LED will flash at the rate of the measurement frequency

At each transition from STANDBY to LOGGING a new measurement cycle begins and
a file with the name 'logNNNNN.csv' is created on the SD card; NNNNN indicates 
the number of the measurement cycle; the number is increased for each cycle. 

At each transition from LOGGING to STANDBY, the measurement file is closed and
the measurement cycle finishes.

During STANDBY mode the logger keeps checking the sensor for a load threshold 
voltage, above the load threshold voltage measurements will start, i.e. the mode changes
to "LOGGING" Below the load threshold voltage the logger assumes that the load is not 
connected yet and the STANDBY mode will continue.


# Configuration File on SD Card

The logger uses a configuration file called LOGGER.INI, in the root folder of the SD card.
The file is a text file containing one value (a string representing a number) per line.
If there is no LOGGER.INI file on the SD card, the logger will create it - using default values.

* First line: next measurement cycle number, an integer. With each transition from "standby" to "measure"
the logger will increase this number.  DEFAULT: 1
* Second line: measurement frequency, a float. A value of 1.0 means that one measurement is done per second. 
A frequency of 10 means that 10 measurements are made/logged per second. Fractions are supported as well, e.g. 
a value of 0.1 means that every 10 seconds a value is logged. DEFAULT: 1.0
* Third line: load threshold voltage, a float. The logger will start measuring/logging data, if the load 
voltage is above this value. See also the section "POWERING THE LOGGER" above. DEFAULT : 6

## Connecting the Logger to Power

The logger software supports two types of power sources 

1. Independent Power Source: the logger / Arduino is connected to its own power source, which 
is independent of the power source for the load we want to measure.
An independent power source can be a powered USB port, which the Arduino is connected to.
It is also possible to connect an independent power source to the Arduino pins "5V" or "VIN".

2. Common Power Source: the logger and the load to be measured use a common power source. 
One way of achieving this, is to connec the VIN pin of the Arduino to the V+ pin of the INA226 and
the + pin of the common power source.


There are pros and cons of using an Independent Power Source (1)

   * PRO: power sources with voltages between 0V and 36V can be measured.

   * PRO: the power source for the load can continue to operate without any (major) impact
   of the sensor and its measurements. The only minor impact is caused by the additional
   "shunt" resistor the INA226 requires for its measurement. Minimal impact is desired, 
   for example, if we want to precisely measure the voltage, currents and power of a 
   charger charging an accumulator battery.

   * CON: additional wiring, an USB power supply, a battery pack or similar is needed.

For a Common Power Source (2)

   * PRO: less space and wiring needed.

   * CON: the supported power source voltages are limited by the Arduino, e.g. in the case of the Arduino Nano 
   Every, the recommended VIN power is between 4.5V and 21V (see MPM3610 specification). 


# Hardware Specs

* [Arduino Nano Every](https://docs.arduino.cc/resources/datasheets/ABX00028-datasheet.pdf)
* [Arduino Nano Every Schema](https://content.arduino.cc/assets/NANOEveryV3.0_sch.pdf)
* [AVR ATMega 4809 on Nano Every](https://www.microchip.com/en-us/product/atmega4809)
* [Serial USB Bridge on Nano Every](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-42363-SAM-D11_Datasheet.pdf)
* [Step Down Converter MPM3610 on Nano Every](https://www.monolithicpower.com/en/documentview/productdocument/index/version/2/document_type/datasheet/lang/en/sku/MPM3610GQV-Z/document_id/2090)
* [AP2112 voltage regulator on Nano Every](https://www.diodes.com/assets/Datasheets/AP2112.pdf)
* [36V, 16-bit, ultra-precise i2c output current/voltage/power monitor w/alert](https://www.ti.com/product/INA226) 
* [INA226 Module on amazon](https://www.amazon.de/dp/B0DGXPWDMP)
* [AD DS1307 RTC](https://www.analog.com/media/en/technical-documentation/data-sheets/ds1307.pdf)
* [SD Card Holder / RTC Module on bastelgarage.ch](https://www.bastelgarage.ch/micro-sd-data-logger-module-with-rtc)
* [SD Card Holder / RTC Module on aliexpress.com](https://www.aliexpress.com/item/1005006248586820.html)

# Internal Wiring diagram of "Red" INA226 Module

![Diagram](/images/INA226%20red%20module%20wiring.png)

