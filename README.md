# PowerLogger

This project implements a data logger for electrical voltage (up to 36V), current (up to 20A) and power readings. 

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

Note that it is better to disconnect the load before disconnecting the power source for the Arduino. Otherwise the logger might loose its power while writing to the SD card, which can cause inconsistent information on the SD card file system. In the worst case, the SD card will become unreadable, must be formatted and the SD card data is lost!

## Wiring for Measurements

The INA226 sensor needs to be connected to the source and the load, two configurations are possible:

<img src=./images/HiLo%20Wiring.png width="960">


If either Current+ or Current- are not connected, the logger will only measure/log the source voltage.

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

During STANDBY mode the logger keeps measuring and if the measured values of either 
the voltage or current are higher than the respective thresholds, the mode changes
to "LOGGING" - and vice versa.

## Configuration File on SD Card

The logger uses a configuration file called LOGGER.INI, in the root folder of the SD card. This text file contains one value (a string representing a number) per line. If there is no LOGGER.INI file on the SD card, the logger will create it - using default values. File Format:
* First line: next measurement cycle number, an integer. With each transition from "standby" to "measure"
the logger will increase this number.  DEFAULT: 1
* Second line: measurement frequency, a float. A value of 1.0 means that one measurement is done per second. 
A frequency of 10 means that 10 measurements are made/logged per second. Fractions are supported as well, e.g. 
a value of 0.1 means that every 10 seconds a value is logged. DEFAULT: 1.0
* Third line: Bus voltage threshold, a float. DEFAULT : 0.0
* Fourth line: load current threshold in mA, a float. DEFAULT : 20.0. 

The logger will log to the SD card, if the Bus voltage and the current are both above thresholds. Setting the current threshold to 0.0 means the logger will log irrespective of the current measured; same for the bus voltage threshold. Setting both threshholds to 0.0 means the logger will start logging after a short delay (see SwitchTime in the code; typically 2 secs) and it will continue until the Arduino is disconnected from power. As mentioned above, the risk of doing this is that the SD card filesystem becomes inconsistent, i.e. unreadable. 

## Hardware Specs

* [Arduino Nano Every](https://docs.arduino.cc/resources/datasheets/ABX00028-datasheet.pdf)
* [Arduino Nano Every Schema](https://content.arduino.cc/assets/NANOEveryV3.0_sch.pdf)
* [AVR ATMega 4809](https://www.microchip.com/en-us/product/atmega4809) on Nano Every
* [Serial USB Bridge](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-42363-SAM-D11_Datasheet.pdf) on Nano Every
* [Step Down Converter MPM3610](https://www.monolithicpower.com/en/documentview/productdocument/index/version/2/document_type/datasheet/lang/en/sku/MPM3610GQV-Z/document_id/2090) on Nano Every
* [AP2112 voltage regulator](https://www.diodes.com/assets/Datasheets/AP2112.pdf) on Nano Every
* [Ultra low capacitance double rail-to-rail ESD protection diode](https://www.nexperia.com/product/PRTR5V0U2X) on Nano Every
* [INA226 36V, 16-bit, ultra-precise i2c output current/voltage/power monitor w/alert](https://www.ti.com/product/INA226) 
* [INA226 Module on amazon](https://www.amazon.de/dp/B0DGXPWDMP)
* [AD DS1307 RTC](https://www.analog.com/media/en/technical-documentation/data-sheets/ds1307.pdf)
* [SD Card Holder / RTC Module on bastelgarage.ch](https://www.bastelgarage.ch/micro-sd-data-logger-module-with-rtc)
* [SD Card Holder / RTC Module on aliexpress.com](https://www.aliexpress.com/item/1005006248586820.html)

## Libraries
* [wollewald / INA226_WE](https://github.com/wollewald/INA226_WE) on github and [INA226 Strom- und Leistungssensor](https://wolles-elektronikkiste.de/ina226)
* [Makuna / Rtc](https://github.com/Makuna/Rtc/wiki) on github

## Wiring Diagram
![Diagram](/images/FullDiagram.png)

* The INA226 and the DS1307 connect via I2C to the Arduino.
* The SD card is connected via SPI.

[More details](MORE.md)

