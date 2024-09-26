# PlatformIo.TCapChamp
[![Build](https://github.com/ChrSchu90/PlatformIo.TCapChamp/actions/workflows/build.yml/badge.svg)](https://github.com/ChrSchu90/PlatformIo.TCapChamp/actions/workflows/build.yml)

## Description
`ESP32` project that manipulates the outside temperature to adjust the cycle rate of the heat pump to increase the efficiency of a overdimensioned heat pump.
The manipulated temperature output needs to be connected to the external temperature sensor of the T-Cap.
If you already use an external temperature sensor you can connect it as input sensor for the ESP, 
in case you don't have a temperature sensor you can use the free [OpenWeather API](https://api.openweathermap.org) to receive actual temperatures. 

Via an optional 0-10V DAC it is also possible to limit the power consumption via `Demand Control`.

[Service Manual T-Cap (PDF)](https://www.kaelte-bast.de/dateien_neu13/Panasonic/01-produktunterlagen/aquarea/produkte/waermepumpen/t-cap/sqc/handbuch_englisch/sm_wh-sqc09h3e8%2Cwh-sqc12.16h9e8_%28papamy1704053ce%29.pdf)

## Bill of Material & Tools
| Name                                                       | Required | Description                                                                   |
| ---------------------------------------------------------- | -------- | ----------------------------------------------------------------------------- |
| [ESP32 NodeMCU-32S](https://www.amazon.de/dp/B0CQSWC67G)   | [x]      | Microcontroller                                                               |
| [MCP4151-503E/P](https://agelektronik.de/digital-potentiometer/6459-mcp4151-503ep-digital-potentiometer50k256-steps1-kanalspidip-8.html)   | [x]      | Digital Potentiometer 50K, 256 steps, SPI   |
| [Electrical wire](https://www.amazon.de/dp/B08BZKR22W)     | [x]      |                                                                               |
| [Prototype Board](https://www.amazon.de/dp/B08F2TS7ZC)     | [x]      | For final installation                                                        |
| 10k resistor                                               | [x]      | Voltage devider for input sensor                                              |
| 5k resistor                                                | [x]      | Pre-Resistor for Digital potentiometer (defines the possible output temperature, in my case +30°C to -20°C) |
| [DFRobot Gravity I2C DAC Module](https://www.dfrobot.com/product-2613.html)      | [ ]      | Optional I2C DAC 0-10V for Demand Control                                                   |
| [Breadboard Kit](https://www.amazon.de/dp/B0B18G3V5T)      | [ ]      | Prototype Board for testing                                                   |
| [Connectors](https://www.amazon.de/dp/B087RN8FDZ)          | [ ]      | Connector for wires                                                           |
| [Socket for ESP](https://www.amazon.de/dp/B07DBY753C)      | [ ]      | Socket for ESP                                                                |
| [IC Sockets](https://www.amazon.de/dp/B01GOLSUAU)          | [ ]      | IC Socket for MCP4151                                                         |


### Tools
- Soldering iron, Solder...
- Multimeter

## Electrical Drawings
You can find the the electrical drawings and parts inside `Documentation\Fritzing` as [Fritzing](https://fritzing.org/) (freeware) project.

### Schematic Drawing
<img src="Documentation/Screenshots/Schematic Drawing.jpg" alt="drawing" width="250" />

### Breadboard Drawing
<img src="Documentation/Screenshots/Breadboard Drawing.jpg" alt="drawing" width="250" />

## Final board (example)
<img src="Documentation/Images/Image1.jpg" alt="drawing" width="250" />
<img src="Documentation/Images/Image2.jpg" alt="drawing" width="250" />

## Webinterface
<img src="Documentation/Screenshots/Webinterface_Adjustment.jpg" alt="drawing" width="300" />

<img src="Documentation/Screenshots/Webinterface_SystemInfo.jpg" alt="drawing" width="300" />

<img src="Documentation/Screenshots/Webinterface_WiFiInfo.jpg" alt="drawing" width="300" />

## License
This project is licensed under [MIT license](LICENSE).