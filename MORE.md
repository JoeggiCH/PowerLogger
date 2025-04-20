
## Internal Wiring diagram of "Red" INA226 Shield/Module

![Diagram](/images/INA226%20red%20module%20wiring.png)

## Some Detail "Stuff"

The Arduino Nano Every Pin D13 is typically used for the SPI SCK signal. The same pin connects to a built-in LED. So, when the Arduino reads from or writes to the SD card the built-in LED flashes. When the logger logs data, SD card write operations happen, i.e. the LED flashes. 

I tested the setup with frequencies as low as 0.1 Hz (i.e. one measurement every 10 seconds) and as high as 100 Hz. There are some limitations related to higher frequencies (see also main.cpp comments where findEnumsMaxProductBelowThreshold() in invoked ).

findEnumsMaxProductBelowThreshold() only finds the optimum parameters for the INA226, if we assume that the measured signal does not vary significantly during delaytime. 

findEnumsMaxProductBelowThreshold() was mostly written by gemini.google.com :-)

The SPI terminology can be a bit confusing because the signals were renamed to avoid the master-slave analogy:

| Master/Slave (OLD) |	Controller/Peripheral (NEW)|
|---|---|
|Master In Slave Out (MISO)	| Controller In, Peripheral Out (CIPO)|
|Master Out Slave In (MOSI)	| Controller Out Peripheral In (COPI)|
|Slave Select pin (SS)	| Chip Select Pin (CS)|


### Connecting the Logger to Power

I used the logger with two types of power sources 

1. Independent Power Source: the logger / Arduino is connected to its "own" power source, which is independent of the power source for the load we want to measure. Such an independent power source can be connected to the Arduino USB, VIN/5V (and GND) pins. 

2. Common Power Source: the logger and the load to be measured use a common power source.

There are pros and cons of using an Independent Power Source (1)

   * PRO: power sources with voltages between 0V and 36V can be measured.

   * CON: an USB power supply, a battery pack or similar, more space is needed.

and for a Common Power Source (2)

   * PRO: less space and wiring needed.

   * CON: the supported power source voltages are limited by the Arduino, e.g. in the case of the Arduino Nano Every, the recommended VIN power is between 4.5V and 21V (see MPM3610 specification). 

   * CON: variations of the common power source voltage might impact the measurements.


## Improvement Ideas

- A "log" button could be added, so the user can choose to start and stop logging - this would be a simple workaround to mitigate the risk of rendering the SD card filesystem inconsistent (e.g. when writing data to the SD card at high frequencies) 
- Use a more modern microcontroller, which includes a SD card and a RTC on board
- Connect multiple INA226 devices to allow multiple measurements in parallel
- Replace the INA226 with a chip, which takes less time to measure voltage and current
- Build a logger with a BLE or WLAN interface transmitting measurement values to another microcontroller with a display