#include <Arduino.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <arduino-timer.h>
#include <RunningMedian.h>
#include <SPI.h>
#include <Wire.h>
#include <DFRobot_GP8403.h>
#include "OpenWeatherMap.h"
#include "ThermistorCalc.h"
#include "Config.h"
#include "Webinterface.h"
#include "Secrets.h"

static const bool DEBUG_LOGGING = true;														// Enable/disable logs to Serial println
static const uint8_t SPI_BUS_THERMISTOR_OUT = VSPI;											// SPI bus used for digital potentiometer for output temperature
static const uint8_t GPIO_THERMISTOR_IN = 36;												// GPIO used for real input temperature from thermistor
static const uint8_t GPIO_FAILOVER_OUT = 27;												// GPIO used as digital output to signal that the output is now valid (failover via relays or LED)
static const float SUPPLY_VOLTAGE = 3.3;													// Maximum Voltage ADC input
static const unsigned int WEATHER_API_UPDATE_CYCLE = 600000;								// Update time of the temperture by the weather API in milliseconds
static const unsigned int TEMP_OUT_UPDATE_CYCLE = 1000;										// Update time of the output temperature in milliseconds
static const unsigned int TEMP_IN_DEVIDER_RESISTANCE = 10000;								// Voltage divider resistor value for input temperature in Ohm
static const unsigned int TEMP_IN_SAMPLE_CYCLE = 10;										// Sample rate to build the median in milliseconds
static const unsigned int TEMP_IN_UPDATE_CYCLE = 1000;										// Update every n ms the input temperature
static const unsigned int TEMP_IN_SAMPLE_CNT = TEMP_IN_UPDATE_CYCLE / TEMP_IN_SAMPLE_CYCLE; // Amount of samples for input thermistor median calculation
static const uint16_t DIGI_POTI_STEPS = 256;												// Maximum amount of steps that the digital potentiometer supports
static const uint16_t DIGI_POTI_STEP_MIN = 0;												// Minimum step of the digital potentiometer to limit maximum current (if no pre-resistor is used)
static const float DIGI_POTI_RESISTANCE = 50000.0f;											// Maximum resistance of the digital potentiometer in Ohm
static const float DIGI_POTI_PRERESISTANCE = 5000.0f;										// Digital potentiometer pre-resistor to limit current and improve precision in Ohm

RunningMedian _thermistorInMedian(TEMP_IN_SAMPLE_CNT);
ThermistorCalc _thermistorIn(-40, 167820, 25, 6523, 120, 302);	// Input for real temperature (Panasonic PAW-A2W-TSOD)
ThermistorCalc _thermistorOut(-40, 167820, 25, 6523, 120, 302); // Output that simulates a Panasonic PAW-A2W-TSOD for the Panasonic T-Cap
Timer<5, millis> _timers;
WiFiManager _wifiManager;
Config *_config;
OpenWeatherMap *_weatherApi;
Webinterface *_webinterface;
SPIClass *_spiDigitalPoti;
DFRobot_GP8403 *_i2cDac;

float _thermistorInTemperature = NAN;
float _weatherApiTemperature = NAN;
float _outputTemperature = NAN;

/// @brief Logs the given message if debug is active
void DebugLog(String message)
{
	if (DEBUG_LOGGING)
	{
		Serial.println(message);
	}
}

/// @brief Reads the ADC voltage with none linear compensation (maximum reading 3.3V range from 0 to 4095)
float readAdcVoltageCorrected(uint8_t gpioPin)
{
	u_int16_t reading = analogRead(gpioPin);
	if (reading < 1)
	{
		return 0;
	}

	if (reading > 3757)
	{
		// From 3.0284V on we use the normal calculation, since the Polynomial curve will drift away and will never reach 3.3V (max out at 3.14V)
		return SUPPLY_VOLTAGE / 4095.0f * reading;
	}

	// Pre-calculated polynomial curve from https://github.com/G6EJD/ESP32-ADC-Accuracy-Improvement-function
	return -0.000000000000016 * pow(reading, 4) + 0.000000000118171 * pow(reading, 3) - 0.000000301211691 * pow(reading, 2) + 0.001109019271794 * reading + 0.034143524634089;
}

/// @brief Gets the current input thermistor temperature
/// @param logError Log detected errors due to e.g. unplausible values
/// @return the temperature of te input thermistor or NAN if there is no sensor conneted or if the value is unplausible
float getCurrentThermistorInTemperature(bool logError)
{
	float voltage = readAdcVoltageCorrected(GPIO_THERMISTOR_IN);
	if (!(voltage > 0))
	{
		if (logError)
		{
			DebugLog("getInputThermistorTemperature: Voltage unplausible, devider resistor defect? voltage=" + String(voltage));
		}

		return NAN;
	}

	float resistance = TEMP_IN_DEVIDER_RESISTANCE * voltage / (SUPPLY_VOLTAGE - voltage);
	if (resistance == infinityf())
	{
		if (logError)
		{
			DebugLog("getInputThermistorTemperature: No external temperature sensor connected. resistance=" + String(resistance));
		}

		return NAN;
	}

	return _thermistorIn.celsiusFromResistance(resistance);
}

///	@brief Updates the _weatherApiTemperature via API from
/// @return If the value has changed
bool updateWeatherApiTemperature()
{
	if (!_weatherApi)
	{
		return false;
	}

	auto request = _weatherApi->request();
	if (!request.successful)
	{
		DebugLog("updateWeatherApiTemperature: Error on update temperature by weather API: " + request.errorMessage);
		return false;
	}

	bool changed = _weatherApiTemperature != request.temperature;
	_weatherApiTemperature = request.temperature;
	return changed;
}

/// @brief Updates the _outputTemperature based on the input sensor, weather API or manual temperature
/// @return If the value has changed
bool updateOutputTemperature()
{
	float inputTemperature = !isnanf(_thermistorInTemperature) ? _thermistorInTemperature : _weatherApiTemperature;
	float targetTemp = _config->temperatureConfig->getOutputTemperature(inputTemperature);

	bool changed = false;
	if (!isnanf(targetTemp))
	{
		float targetResistance = _thermistorOut.resistanceFromCelsius(targetTemp);
		uint16_t posistion = targetResistance >= DIGI_POTI_PRERESISTANCE ? roundf(((targetResistance - DIGI_POTI_PRERESISTANCE) / (DIGI_POTI_RESISTANCE / (DIGI_POTI_STEPS + 1))) - 1) : 0;
		posistion = constrain(posistion, DIGI_POTI_STEP_MIN, DIGI_POTI_STEPS);
		float positionResistance = (DIGI_POTI_RESISTANCE / (DIGI_POTI_STEPS + 1)) + (posistion * (DIGI_POTI_RESISTANCE / (DIGI_POTI_STEPS + 1)));
		float outputResistance = DIGI_POTI_PRERESISTANCE + positionResistance;
		float outputTemperature = _thermistorOut.celsiusFromResistance(outputResistance);
		if (_outputTemperature != outputTemperature)
		{
			DebugLog("updateOutputTemperature: targetTemp=" + String(targetTemp) + " targetResistance=" + String(targetResistance) + " posistion=" + String(posistion) + " positionResistance=" + String(positionResistance) + " outputResistance=" + String(outputResistance) + " outputTemperature=" + String(outputTemperature));
			_spiDigitalPoti->transfer16(posistion);
			_outputTemperature = outputTemperature;
			changed = true;
		}
	}
	else
	{
		changed = !isnanf(_outputTemperature);
		_outputTemperature = targetTemp;
	}

	digitalWrite(GPIO_FAILOVER_OUT, !isnanf(_outputTemperature) ? HIGH : LOW);
	return changed;
}

/// @brief Update the _thermistorInTemperature by reading the input thermistor temperature to build a medain average
/// @return If the value has changed
bool updateThermistorInTemperature()
{
	float tempIn = getCurrentThermistorInTemperature(!isnanf(_thermistorInTemperature));
	if (isnanf(tempIn))
	{
		if (!isnanf(_thermistorInTemperature) || _thermistorInMedian.getCount() > 0)
		{
			_thermistorInMedian.clear();
			_thermistorInTemperature = NAN;
			return true;
		}

		return false;
	}

	_thermistorInMedian.add(tempIn);
	if ((_thermistorInMedian.getCount() % TEMP_IN_SAMPLE_CNT) == 0)
	{
		float medianTemp = _thermistorInMedian.getMedianAverage(TEMP_IN_SAMPLE_CNT);
		_thermistorInMedian.clear();
		if (isnanf(_thermistorInTemperature) || abs(medianTemp - _thermistorInTemperature) > 0.06)
		{
			DebugLog("updateThermistorInTemperature: (median) Temperature=" + String(medianTemp));
			_thermistorInTemperature = medianTemp;
			return true;
		}
	}

	return false;
}

/// @brief Put your main code here, to run repeatedly:
void loop()
{
	_timers.tick();
	_wifiManager.process();
}

/// @brief Setup for the configuration for loading and storing none volatile data
void setupConfiguration()
{
	DebugLog("setupConfiguration: started");
	_config = new Config();
	DebugLog("setupConfiguration: completed");
}

/// @brief Setup for Web UI (called by setupWifiManager after auto connect)
void setupWebinterface()
{
	DebugLog("setupWebinterface: started");

	// Creaate webinterface
	_webinterface = new Webinterface(8080, _config, &_wifiManager);

	// Set initial values
	_webinterface->setSensorTemp(_thermistorInTemperature);
	_webinterface->setWeatherTemp(_weatherApiTemperature);
	_webinterface->setOutputTemp(_outputTemperature);

	DebugLog("setupWebinterface: completed");
}

/// @brief Setup for Weather API
void setupWeatherApi()
{
	DebugLog("setupWeatherApi: started");

	if (WEATHER_API_KEY.length() < 1)
	{
		DebugLog("setupWeatherApi: API key missing, weather API can't be used!");
		return;
	}

	if (WEATHER_CITY_ID > 0)
	{
		DebugLog("setupWeatherApi: Using City ID");
		_weatherApi = new OpenWeatherMap(WEATHER_API_KEY, WEATHER_CITY_ID);
		//_weatherApiUrl = "https://api.openweathermap.org/data/2.5/weather?id=" + String(WEATHER_CITY_ID) + "&lang=en" + "&units=METRIC" + "&appid=" + WEATHER_API_KEY;
	}
	else if (abs(WEATHER_LATITUDE) > 0 && abs(WEATHER_LONGITUDE) > 0)
	{
		DebugLog("setupWeatherApi: Using Latitude and Longitude");
		_weatherApi = new OpenWeatherMap(WEATHER_API_KEY, WEATHER_LATITUDE, WEATHER_LONGITUDE);
		//_weatherApiUrl = "https://api.openweathermap.org/data/2.5/weather?lat=" + String(WEATHER_LATITUDE, 6) + "&lon=" + String(WEATHER_LONGITUDE, 6) + "&lang=en" + "&units=METRIC" + "&appid=" + WEATHER_API_KEY;
	}
	else
	{
		DebugLog("setupWeatherApi: Location missing, please define CityID or Latitude + Longitude!");
		return;
	}

	DebugLog("setupWeatherApi: _weatherApiUrl=" + _weatherApi->apiUrl);
	if (updateWeatherApiTemperature())
	{
		DebugLog("setupWeatherApi: initial _weatherApiTemperature=" + String(_weatherApiTemperature));
	}

	_timers.every(
		WEATHER_API_UPDATE_CYCLE,
		[](void *opaque) -> bool
		{
			if (updateWeatherApiTemperature() && _webinterface)
			{
				_webinterface->setWeatherTemp(_weatherApiTemperature);
			}

			return true; // Keep timer running
		});
}

/// @brief Setup for WiFiManager as blocking implementation if not configured.
void setupWifiManager()
{
	DebugLog("setupWifiManager: started");

	// explicitly set mode, esp defaults to STA+AP
	WiFi.mode(WIFI_STA);

	// Start config portal only if there is no WiFi configuration
	// And enable portal blocking for configuration
	bool configurued = _wifiManager.getWiFiIsSaved();
	_wifiManager.setEnableConfigPortal(!configurued);
	_wifiManager.setConfigPortalBlocking(!configurued);
	_wifiManager.setConfigPortalTimeout(86400); // 24h timeout
	//_wifiManager.setHttpPort(8080);

	// automatically connect using saved credentials if they exist
	// If connection fails it starts an access point with the specified name
	bool connected = _wifiManager.autoConnect("", WIFI_CONFIG_PASSWORD.c_str());
	if (!connected)
	{
		if (!configurued)
		{
			DebugLog("setupWifiManager: Config portal failed, reboot");
			ESP.restart();
			delay(10000);
		}
		else
		{
			DebugLog("setupWifiManager: WiFi is configured, but connecting to saved WiFi failed");
		}
	}
	else
	{
		if (!configurued)
		{
			DebugLog("setupWifiManager: Start reboot after WiFi configuration");
			ESP.restart();
			delay(10000);
		}

		DebugLog("setupWifiManager: Successfuly connected to WiFi");
	}
}

/// @brief Setup reading of input thermistor
void setupThermistorInputReading()
{
	DebugLog("setupThermistorInputReading: started");

	// initial reading without building a median
	_thermistorInTemperature = getCurrentThermistorInTemperature(true);
	_timers.every(
		TEMP_IN_SAMPLE_CYCLE,
		[](void *opaque) -> bool
		{
			if (updateThermistorInTemperature() && _webinterface)
			{
				_webinterface->setSensorTemp(_thermistorInTemperature);
			}

			return true; // Keep timer running
		});

	DebugLog("setupThermistorInputReading: completed");
}

/// @brief Setup of the power limiter via 0-10V analog output
void setupPowerLimit()
{
	DebugLog("setupPowerLimit: started");

	DFRobot_GP8403 *dac = new DFRobot_GP8403(&Wire, 0x5F);
	if (dac->begin() != 0)
	{
		DebugLog("setupPowerLimit: Error on init 0-10V output via I2C!");
	}
	else
	{
		_i2cDac = dac;
		_i2cDac->setDACOutRange(dac->eOutputRange10V);
		_i2cDac->setDACOutVoltage(0000, 0); // mV / Channel
		DebugLog("setupPowerLimit: Successful init 0-10V output via I2C");
		// delay(1000); // wait before store, due to exceptions
		// dac.store();
	}
}

/// @brief Setup digital potentiometer for output temperature
void setupOutputTemperature()
{
	DebugLog("setupOutputTemperature: started");

	_spiDigitalPoti = new SPIClass(SPI_BUS_THERMISTOR_OUT);
	_spiDigitalPoti->begin();
	pinMode(_spiDigitalPoti->pinSS(), OUTPUT);
	pinMode(GPIO_FAILOVER_OUT, OUTPUT);

	updateOutputTemperature();
	_timers.every(
		TEMP_OUT_UPDATE_CYCLE,
		[](void *opaque) -> bool
		{
			if (updateOutputTemperature() && _webinterface)
			{
				_webinterface->setOutputTemp(_outputTemperature);
			}

			return true;
		});

	DebugLog("setupOutputTemperature: completed");
}

/// @brief Put your setup code here, to run once:
void setup()
{
	if (DEBUG_LOGGING)
	{
		Serial.begin(115200);
		delay(2000); // Delay for serial monitor attach
	}

	DebugLog("setup: started");

	setupConfiguration();
	setupThermistorInputReading();
	setupWifiManager();
	setupWeatherApi();
	setupPowerLimit();
	setupOutputTemperature();
	setupWebinterface();

	DebugLog("setup: completed");
}