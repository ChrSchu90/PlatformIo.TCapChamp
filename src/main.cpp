#define LOG_LEVEL NONE

#include <Arduino.h>
#include <WiFiManager.h>
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
#include "SerialLogging.h"

Timer<5, millis> _timers;	 // Timer collection for time based operations
WiFiManager _wifiManager;	 // Access to the WiFi manager TODO: replace by integrated configuration and reconnect logic
Config *_config;			 // Access to the configuration
Webinterface *_webinterface; // Access to the webinterface

#pragma region Input_Temperature_Sensor

static const uint8_t GPIO_THERMISTOR_IN = GPIO_NUM_36;												// GPIO used for real input temperature from thermistor
static const float SUPPLY_VOLTAGE = 3.3;													// Maximum Voltage ADC input
static const unsigned int TEMP_IN_DEVIDER_RESISTANCE = 10000;								// Voltage divider resistor value for input temperature in Ohm
static const unsigned int TEMP_IN_SAMPLE_CYCLE = 10;										// Sample rate to build the median in milliseconds
static const unsigned int TEMP_IN_UPDATE_CYCLE = 1000;										// Update every n ms the input temperature
static const unsigned int TEMP_IN_SAMPLE_CNT = TEMP_IN_UPDATE_CYCLE / TEMP_IN_SAMPLE_CYCLE; // Amount of samples for input thermistor median calculation
RunningMedian _thermistorInMedian(TEMP_IN_SAMPLE_CNT);										// Median average calculation for input temperature sensor
ThermistorCalc _thermistorIn(-40, 167820, 25, 6523, 120, 302);								// Input for real temperature (Panasonic PAW-A2W-TSOD)
float _thermistorInTemperature = NAN;														// Last temperature from input sensor (NAN if not available)

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
#ifdef LOG_ERROR
			LOG_ERROR(F("Main"), F("getCurrentThermistorInTemperature"), F("Voltage unplausible, devider resistor defect? voltage=") + voltage);
#endif
		}

		return NAN;
	}

	float resistance = TEMP_IN_DEVIDER_RESISTANCE * voltage / (SUPPLY_VOLTAGE - voltage);
	if (resistance == infinityf())
	{
		if (logError)
		{
#ifdef LOG_ERROR
			LOG_ERROR(F("Main"), F("getCurrentThermistorInTemperature"), F("No external temperature sensor connected. resistance=") + resistance);
#endif
		}

		return NAN;
	}

	return _thermistorIn.celsiusFromResistance(resistance);
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
#ifdef LOG_INFO
			LOG_INFO(F("Main"), F("updateThermistorInTemperature"), F("Temperature (median) ") + medianTemp);
#endif
			_thermistorInTemperature = medianTemp;
			return true;
		}
	}

	return false;
}

/// @brief Setup reading of input thermistor
void setupThermistorInputReading()
{
#ifdef LOG_DEBUG
	LOG_DEBUG(F("Main"), F("setupThermistorInputReading"), F("Started"));
#endif

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

#ifdef LOG_DEBUG
	LOG_DEBUG(F("Main"), F("setupThermistorInputReading"), F("Completed"));
#endif
}

#pragma endregion Input_Temperature_Sensor

#pragma region Weather_API

static const unsigned int WEATHER_API_UPDATE_CYCLE = 600000;	   // Update time of the temperture by the weather API in milliseconds
static const unsigned int WEATHER_API_UPDATE_CYCLE_FAILED = 10000; // Update time of the temperture by the weather API if the request has failed in milliseconds
OpenWeatherMap *_weatherApi;									   // OpenWeatherMap API access
float _weatherApiTemperature = NAN;								   // Last temperature from weather API (NAN if not available)

/// @brief Update Weather API temperature timer callback
/// NOTE: The timer registration is handled by updateWeatherApiTemperature since
/// based on the API response the timer will be restarted with different ticks
bool updateWeatherApiTemperatureTick(void *opaque);

///	@brief Updates the _weatherApiTemperature via API
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
#ifdef LOG_ERROR
		switch (request.error)
		{
		case Error::WifiNotConnected:
			LOG_ERROR(F("Main"), F("updateWeatherApiTemperature"), F("Error on update temperature by weather API (WiFi not connected)"));
			break;
		case Error::HttpError:
			LOG_ERROR(F("Main"), F("updateWeatherApiTemperature"), F("Error on update temperature by weather API (HTTP error) " + request.httpCode));
			break;
		case Error::DeserializationFailed:
			LOG_ERROR(F("Main"), F("updateWeatherApiTemperature"), F("Error on update temperature by weather API (Deserialization failed)"));
			break;
		}
#endif
		_timers.every(WEATHER_API_UPDATE_CYCLE_FAILED, updateWeatherApiTemperatureTick);
		return false;
	}

	_timers.every(WEATHER_API_UPDATE_CYCLE, updateWeatherApiTemperatureTick);
	bool changed = _weatherApiTemperature != request.temperature;
	_weatherApiTemperature = request.temperature;
	return changed;
}

/// @brief Update Weather API temperature timer callback
/// NOTE: The timer registration is handled by updateWeatherApiTemperature since
/// based on the API response the timer will be restarted with different ticks
bool updateWeatherApiTemperatureTick(void *opaque)
{
	if (updateWeatherApiTemperature() && _webinterface)
	{
		_webinterface->setWeatherTemp(_weatherApiTemperature);
	}

	// Stop current timer, a new one is created by updateWeatherApiTemperature depending if failed or successful
	return false;
}

/// @brief Setup for Weather API
void setupWeatherApi()
{
#ifdef LOG_DEBUG
	LOG_DEBUG(F("Main"), F("setupWeatherApi"), F("Started"));
#endif

	if (WEATHER_API_KEY.length() < 30)
	{
#ifdef LOG_ERROR
		LOG_ERROR(F("Main"), F("setupWeatherApi"), F("API key missing or invalid, weather API can't be used!"));
#endif
		return;
	}

	if (WEATHER_CITY_ID > 0)
	{
#ifdef LOG_INFO
		LOG_INFO(F("Main"), F("setupWeatherApi"), F("Using City ID ") + WEATHER_CITY_ID);
#endif
		_weatherApi = new OpenWeatherMap(WEATHER_API_KEY, WEATHER_CITY_ID);
	}
	else if (abs(WEATHER_LATITUDE) > 0 && abs(WEATHER_LONGITUDE) > 0)
	{
#ifdef LOG_INFO
		LOG_INFO(F("Main"), F("setupWeatherApi"), F("Using Latitude ") + WEATHER_LATITUDE + F(" and Longitude ") + WEATHER_LONGITUDE);
#endif
		_weatherApi = new OpenWeatherMap(WEATHER_API_KEY, WEATHER_LATITUDE, WEATHER_LONGITUDE);
	}
	else
	{
#ifdef LOG_ERROR
		LOG_ERROR(F("Main"), F("setupWeatherApi"), F("Location missing, please define CityID or Latitude + Longitude!"));
#endif
		return;
	}
#ifdef LOG_DEBUG
	LOG_DEBUG(F("Main"), F("setupWeatherApi"), F("API URL ") + _weatherApi->apiUrl);
#endif
	if (updateWeatherApiTemperature())
	{
#ifdef LOG_INFO
		LOG_INFO(F("Main"), F("setupWeatherApi"), F("initial temperature ") + _weatherApiTemperature);
#endif
	}
}

#pragma endregion Weather_API

#pragma region Output_Temperature_Sensor

static const uint8_t SPI_BUS_THERMISTOR_OUT = VSPI;				// SPI bus used for digital potentiometer for output temperature
static const uint8_t GPIO_FAILOVER_OUT = GPIO_NUM_27;			// GPIO used as digital output to signal that the output is now valid (failover via relays or LED)
static const unsigned int TEMP_OUT_UPDATE_CYCLE = 1000;			// Update time of the output temperature in milliseconds
static const uint16_t DIGI_POTI_STEPS = 256;					// Maximum amount of steps that the digital potentiometer supports
static const uint16_t DIGI_POTI_STEP_MIN = 0;					// Minimum step of the digital potentiometer to limit maximum current (if no pre-resistor is used)
static const float DIGI_POTI_RESISTANCE = 50000.0f;				// Maximum resistance of the digital potentiometer in Ohm
static const float DIGI_POTI_PRERESISTANCE = 5000.0f;			// Digital potentiometer pre-resistor to limit current and improve precision in Ohm
ThermistorCalc _thermistorOut(-40, 167820, 25, 6523, 120, 302); // Output that simulates a Panasonic PAW-A2W-TSOD for the Panasonic T-Cap
SPIClass *_spiDigitalPoti;										// Digital potentiometer SPI interface
float _outputTemperature = NAN;									// Last output temperature (NAN if no temperature could be calculated)

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
#ifdef LOG_DEBUG
			LOG_DEBUG(F("Main"), F("updateOutputTemperature"), F("targetTemp=") + String(targetTemp) + F(" targetResistance=") + String(targetResistance) + F(" posistion=") + String(posistion) + F(" positionResistance=") + String(positionResistance) + F(" outputResistance=") + String(outputResistance) + F(" outputTemperature=") + String(outputTemperature));
#endif
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

/// @brief Setup digital potentiometer for output temperature
void setupOutputTemperature()
{
#ifdef LOG_DEBUG
	LOG_DEBUG(F("Main"), F("setupOutputTemperature"), F("Started"));
#endif

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
#ifdef LOG_DEBUG
	LOG_DEBUG(F("Main"), F("setupOutputTemperature"), F("Completed"));
#endif
}

#pragma endregion Output_Temperature_Sensor

#pragma region Output_Power_Limit

DFRobot_GP8403 *_i2cDac;

/// @brief Setup of the power limiter via 0-10V analog output
void setupPowerLimit()
{
#ifdef LOG_DEBUG
	LOG_DEBUG(F("Main"), F("setupPowerLimit"), F("Started"));
#endif

	DFRobot_GP8403 *dac = new DFRobot_GP8403(&Wire, 0x5F);
	if (dac->begin() != 0)
	{
#ifdef LOG_ERROR
		LOG_ERROR(F("Main"), F("setupPowerLimit"), F("Error on init 0-10V output via I2C!"));
#endif
	}
	else
	{
		_i2cDac = dac;
		_i2cDac->setDACOutRange(dac->eOutputRange10V);
		_i2cDac->setDACOutVoltage(0000, 0); // mV / Channel
#ifdef LOG_DEBUG
		LOG_DEBUG(F("Main"), F("setupPowerLimit"), F("Successful init 0-10V output via I2C"));
#endif
		// delay(1000); // wait before store, due to exceptions
		// dac.store();
	}
}

#pragma endregion Output_Power_Limit

/// @brief Put your main code here, to run repeatedly:
void loop()
{
	_timers.tick();

	// if(_wifiManager.getWiFiIsSaved())
	//{
	_wifiManager.process();
	//}
}

/// @brief Setup for the configuration for loading and storing none volatile data
void setupConfiguration()
{
#ifdef LOG_DEBUG
	LOG_DEBUG(F("Main"), F("setupConfiguration"), F("Started"));
#endif

	_config = new Config();

#ifdef LOG_DEBUG
	LOG_DEBUG(F("Main"), F("setupConfiguration"), F("Completed"));
#endif
}

/// @brief Setup for Web UI (called by setupWifiManager after auto connect)
void setupWebinterface()
{
#ifdef LOG_DEBUG
	LOG_DEBUG(F("Main"), F("setupWebinterface"), F("Started"));
#endif

	// Creaate webinterface
	_webinterface = new Webinterface(8080, _config, &_wifiManager);

	// Set initial values
	_webinterface->setSensorTemp(_thermistorInTemperature);
	_webinterface->setWeatherTemp(_weatherApiTemperature);
	_webinterface->setOutputTemp(_outputTemperature);

#ifdef LOG_DEBUG
	LOG_DEBUG(F("Main"), F("setupWebinterface"), F("Completed"));
#endif
}

/// @brief Setup for WiFiManager as blocking implementation if not configured.
void setupWifiManager()
{
#ifdef LOG_DEBUG
	LOG_DEBUG(F("Main"), F("setupWifiManager"), F("Started"));
#endif

	// explicitly set mode, esp defaults to STA+AP
	WiFi.mode(WIFI_STA);

	// Start config portal only if there is no WiFi configuration
	// And enable portal blocking for configuration
	bool configurued = _wifiManager.getWiFiIsSaved();
	//_wifiManager.setEnableConfigPortal(!configurued);
	_wifiManager.setConfigPortalBlocking(false);
	_wifiManager.setConfigPortalTimeout(86400); // 24h timeout
	//_wifiManager.setHttpPort(8080);

	// automatically connect using saved credentials if they exist
	// If connection fails it starts an access point with the specified name
	bool connected = _wifiManager.autoConnect("", WIFI_CONFIG_PASSWORD.c_str());
	if (!connected)
	{
		if (!configurued)
		{
#ifdef LOG_WARNING
			LOG_WARNING(F("Main"), F("setupWifiManager"), F("Config portal failed, reboot!"));
#endif
			ESP.restart();
			delay(10000);
		}
		else
		{
#ifdef LOG_WARNING
			LOG_WARNING(F("Main"), F("setupWifiManager"), F("WiFi is configured, but connecting to saved WiFi failed"));
#endif
		}
	}
	else
	{
		if (!configurued)
		{
#ifdef LOG_INFO
			LOG_INFO(F("Main"), F("setupWifiManager"), F("Start reboot after WiFi configuration"));
#endif
			ESP.restart();
			delay(10000);
		}
#ifdef LOG_DEBUG
		LOG_DEBUG(F("Main"), F("setupWifiManager"), F("Successfuly connected to WiFi"));
#endif
	}
}

/// @brief Put your setup code here, to run once:
void setup()
{
	Serial.begin(115200);

#ifdef LOG_DEBUG
	LOG_DEBUG(F("Main"), F("setup"), F("Started"));
#endif

	setupConfiguration();
	setupThermistorInputReading();
	setupWifiManager();
	setupWeatherApi();
	setupPowerLimit();
	setupOutputTemperature();
	setupWebinterface();

#ifdef LOG_DEBUG
	LOG_DEBUG(F("Main"), F("setup"), F("Completed"));
#endif
}