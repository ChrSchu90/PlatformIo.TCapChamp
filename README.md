# PlatformIo.TCapChamp
[![Build](https://github.com/ChrSchu90/PlatformIo.TCapChamp/actions/workflows/build.yml/badge.svg)](https://github.com/ChrSchu90/PlatformIo.TCapChamp/actions/workflows/build.yml)

## Description
`ESP32` project that manipulates the outside temperature measurement to adjust the heat pump cycle rate. In case the heat pump is overdimensioned it can increase its efficiency dramatically since you can adjust the cycle rate specifically on your environment and needs.
The manipulated temperature output needs to be connected to the external temperature sensor of the T-Cap.

If you already use an external temperature sensor you can connect it as input sensor for the ESP, in case you don't have a temperature sensor you can use the free [OpenWeather API](https://api.openweathermap.org) to receive actual temperatures. 

Via an optional 0-10V DAC it is also possible to limit the power consumption via `Demand Control` of the T-Cap.

> **Info**
> You can find the measurements, charts and calcumations inside the [TestsAndCharts.xlsx](Documentation/TestsAndCharts.xlsx)
> Device specific information can be taken from the [T-Cap Service Manual (PDF)](https://www.kaelte-bast.de/dateien_neu13/Panasonic/01-produktunterlagen/aquarea/produkte/waermepumpen/t-cap/sqc/handbuch_englisch/sm_wh-sqc09h3e8%2Cwh-sqc12.16h9e8_%28papamy1704053ce%29.pdf)

## Features
| Feature                                          | Completed |
| ------------------------------------------------ | --------- |
| Automatic switch between WiFi AP and Client mode | &#10003;  |
| Hardware sensor for input temperature            | &#10003;  |
| Weather API for input temperature (OpenWeather)  | &#10003;  |
| 0V-10V Demand control                            | &#10003;  |
| OTA Updates                                      | &#10003;  |
| Captive Portal for initial WiFi configuration    | &#10003;  |
| Custom webinterface for better accessibility     | &#10007;  |


## Bill of Material & Tools
| Name                                                       | Required | Description                                                                   |
| ---------------------------------------------------------- | -------- | ----------------------------------------------------------------------------- |
| [ESP32 NodeMCU-32S](https://www.amazon.de/dp/B0CQSWC67G)   | &#10003; | Microcontroller                                                               |
| [MCP4151-503E/P](https://agelektronik.de/digital-potentiometer/6459-mcp4151-503ep-digital-potentiometer50k256-steps1-kanalspidip-8.html)   | &#10003; | Digital Potentiometer 50K, 256 steps, SPI   |
| [Electrical wire](https://www.amazon.de/dp/B08BZKR22W)     | &#10003; |                                                                               |
| [Prototype Board](https://www.amazon.de/dp/B08F2TS7ZC)     | &#10003; | For final board                                                               |
| 10k resistor                                               | &#10003; | Voltage devider for input sensor                                              |
| 5k resistor                                                | &#10003; | Pre-Resistor for Digital potentiometer (defines the possible output temperature, in my case +30°C to -20°C) |
| [DFRobot Gravity I2C DAC Module](https://www.dfrobot.com/product-2613.html)      | &#10007; | Optional I2C DAC 0-10V for Demand Control                                                   |
| [Breadboard Kit](https://www.amazon.de/dp/B0B18G3V5T)      | &#10007; | Prototype Board for testing                                                   |
| [Connectors](https://www.amazon.de/dp/B087RN8FDZ)          | &#10007; | Connector for wires                                                           |
| [Socket for ESP](https://www.amazon.de/dp/B07DBY753C)      | &#10007; | Socket for ESP                                                                |
| [IC Sockets](https://www.amazon.de/dp/B01GOLSUAU)          | &#10007; | IC Socket for MCP4151                                                         |


### Tools
- Soldering iron, Solder...
- Multimeter

## Electrical Drawings
You can find the the electrical drawings and parts inside `Documentation\Fritzing` as [Fritzing](https://fritzing.org/) (freeware) project.

<img src="Documentation/Screenshots/Breadboard Drawing.jpg" alt="drawing" width="250" /><img src="Documentation/Screenshots/Schematic Drawing.jpg" alt="drawing" width="320" />


## Final board (example)
<img src="Documentation/Images/Image1.jpg" alt="drawing" width="283" /><img src="Documentation/Images/Image2.jpg" alt="drawing" width="250" />

## Webinterface
<img src="Documentation/Screenshots/Webinterface_Adjustment.jpg" alt="drawing" width="300" />

<img src="Documentation/Screenshots/Webinterface_SystemInfo.jpg" alt="drawing" width="300" /><img src="Documentation/Screenshots/Webinterface_WiFiInfo.jpg" alt="drawing" width="300" />

## Temperature Adjustment

### The cheap solution
A common way to manipulate the temperature is to connect a parallel resistor (e.g. 75k Ω) to the external sensor. The problem with this approach is that a thermal resistor isn't a linear resistor and because of that the temperature offset increases as the real temperature goes down:

<img src="Documentation/Screenshots/ParallelResistorChart.jpg" alt="drawing" width="450" />

### The superior solution
Due to the problems with a parallel resistor this project has been started to provide a more granular and comfortable way, without changing hardware if another offset is required. 

It also makes it a lot easier at the beginning, since first you need to figure out what offset is the best based on your environment and preferences. To do so you can control input and output temperatures manually until you know where your sweetspot is. Afterwards the controller manipulates the output temperature based on a input temperature fully automated.

<img src="Documentation/Screenshots/Example1Configuration.jpg" alt="drawing" width="85" /><img src="Documentation/Screenshots/Example1Chart.jpg" alt="drawing" width="375" />

> **Note**
> Keep in mind that the components are not perfect and have tolerances!
> The output values are calculated and may not match the temperature that
> the T-Cap will read. You can use the T-Cap sensor calibration for compensation.

<img src="Documentation/Screenshots/DeltaTargetToOutputChart.jpg" alt="drawing" width="465" />

## Demand Control (optional)
The T-Cap provides a 0-10V analog input to limit the power consumption. It can be a handy tool to optimize the heat pump even more as what is possible only by temperature adjustments.

To do so you can define temperature areas and its power limit inside the UI.

> **Note**
> The `Input Temperature` will be used to determine the area, this ensures that 
> temperature adjustments will not influence the power limits.

The first matching area will be uses (top to buttom). In case no responsable area will be found the output will be set to 0V (not active).
If you like to define a default limit place the value inside the last area with start 30°C and end -20°C and define specific areas above. To deactivate areas set start and end temperature to the same value.

<img src="Documentation/Screenshots/ExamplePowerLimit.jpg" alt="drawing" width="300" />

<img src="Documentation/T-Cap Manual/DemandControl.jpg" alt="drawing" width="465" />

## 3rd Party libraries
The following libraries are used by this project (Thank you very much!)
- [ESPUI](https://github.com/s00500/ESPUI) for webinterface
- [RunningMedian](https://github.com/RobTillaart/RunningMedian) calculation for input temperature sensor
- [arduino-timer](https://github.com/contrem/arduino-timer) for time based functions
- [MycilaESPConnect](https://github.com/mathieucarbou/MycilaESPConnect) inspiration for WiFi AP/Client switching
- [ADC-Accuracy](https://github.com/G6EJD/ESP32-ADC-Accuracy-Improvement-function) ADC accuracy improvement
- [DFRobot_GP8403](https://github.com/DFRobot/DFRobot_GP8403) I2C DAC Module

## License
This project is licensed under [MIT license](LICENSE).