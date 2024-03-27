#include <Arduino.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <ESPUI.h>
#include <arduino-timer.h>
#include <HTTPClient.h>
#include <RunningMedian.h>
#include <SPI.h>
#include <stdlib.h>
#include "Weather\OpenWeatherMap.h"
#include "Thermistor\ThermistorCalc.h"
#include "Tabs\TabSystemInfo.h"
#include "Tabs\TabWifiInfo.h"
#include "Area.h"
#include "Secrets.h"

static const bool DEBUG_LOGGING = true;														// Enable/disable logs to Serial println
static const uint8_t SPI_BUS_THERMISTOR_OUT = VSPI;											// SPI bus used for digital potentiometer for output temperature
static const char *PREF_KEY_NAMESPACE = "HeatPumpChamp";									// Preferences namespance key (limited to 15 chars)
static const char *PREF_KEY_MANUAL_MODE = "ManualMode";										// Preferences key for manual mode (limited to 15 chars)
static const char *PREF_KEY_TEMP_OFFSET = "TempOffet";										// Preferences key for temperature offset (limited to 15 chars)
static const char *PREF_KEY_TEMP_MANUAL = "TempManual";										// Preferences key for manual temperature (limited to 15 chars)
static const uint8_t GPIO_THERMISTOR_IN = 36;												// GPIO used for real input temperature from thermistor
static const uint8_t GPIO_FAILOVER_OUT = 27;												// GPIO used as digital output to signal that the output is now valid (failover via relays or LED)
static const unsigned int WEB_UI_UPDATE_CYCLE = 10000;										// Update time for Web UI in milliseconds
static const float SUPPLY_VOLTAGE = 3.3;													// Maximum Voltage ADC input
static const unsigned int WEATHER_API_UPDATE_CYCLE = 600000;								// Update time of the temperture by the weather API in milliseconds
static const unsigned int TEMP_OUT_UPDATE_CYCLE = 10000;									// Update time of the output temperature in milliseconds
static const unsigned int TEMP_IN_DEVIDER_RESISTANCE = 10000;								// Voltage divider resistor value for input temperature in Ohm
static const unsigned int TEMP_IN_SAMPLE_CYCLE = 100;										// Sample rate to build the median in milliseconds
static const unsigned int TEMP_IN_UPDATE_CYCLE = 10000;										// Update every n ms the input temperature
static const unsigned int TEMP_IN_SAMPLE_CNT = TEMP_IN_UPDATE_CYCLE / TEMP_IN_SAMPLE_CYCLE; // Amount of samples for input thermistor median calculation
static const uint16_t DIGI_POTI_STEPS = 256;												// Maximum amount of steps that the digital potentiometer supports
static const uint16_t DIGI_POTI_STEP_MIN = 0;												// Minimum step of the digital potentiometer to limit maximum current (if no pre-resistor is used)
static const float DIGI_POTI_RESISTANCE = 50000.0f;											// Maximum resistance of the digital potentiometer in Ohm
static const float DIGI_POTI_PRERESISTANCE = 3333.0f;										// Digital potentiometer pre-resistor to limit current and improve precision in Ohm

RunningMedian _thermistorInMedian = RunningMedian(TEMP_IN_SAMPLE_CNT);
ThermistorCalc _thermistorIn(-40, 167820, 25, 6523, 120, 302);	// Input for real temperature (Panasonic PAW-A2W-TSOD)
ThermistorCalc _thermistorOut(-40, 167820, 25, 6523, 120, 302); // Output that simulates a Panasonic PAW-A2W-TSOD for the Panasonic T-Cap 16 KW Kit-WQC16H9E8
Timer<5, millis> _timers;
Preferences _preferences;
WiFiManager _wifiManager;

uint16_t _lblSensorTemp;
uint16_t _lblWeatherTemp;
uint16_t _lblOutputTemp;
uint16_t _tabTemperature;
uint16_t _tabPower;
uint16_t _swManualMode;
uint16_t _sldManualTemp;
uint16_t _sldOffset; // TODO: Remove after moving into area
TabSystemInfo *_tabSystemInfo;
TabWifiInfo *_tabWifiInfo;

SPIClass *_spiDigitalPoti;
String _weatherApiUrl;
bool _manualMode;
int _temperatureOffset;
int _temperatureManual;
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

/// @brief Gets the temperature from OpenWeatherMap see: https://openweathermap.org/current (60 calls/minute or 1,000,000 calls/month)
/// @return The temperature from the API response or NAN if request has failed
float getWeatherApiTemperature()
{
	if (_weatherApiUrl.length() < 1)
	{
		return NAN;
	}

	if (WiFi.status() != WL_CONNECTED)
	{
		DebugLog("getWeatherApiTemperature: WiFi not connected");
		return NAN;
	}

	HTTPClient http;
	http.begin(_weatherApiUrl);
	int httpCode = http.GET();
	if (httpCode != 200)
	{
		DebugLog("getWeatherApiTemperature: HTTP Error " + String(httpCode));
		JsonDocument doc;
		DeserializationError error = deserializeJson(doc, http.getString());
		if (error || !doc.containsKey("message"))
		{
			http.end();
			return NAN;
		}

		String errMessage = doc["message"];
		DebugLog("getWeatherApiTemperature: Message=" + errMessage);
		http.end();
		return NAN;
	}

	String response = http.getString();
	http.end();
	DebugLog("getWeatherApiTemperature: API response=" + String(response));

	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, response);
	if (error)
	{
		DebugLog("getWeatherApiTemperature: DeserializationError=" + String(error.c_str()));
		doc.clear();
		return NAN;
	}

	float temperature = doc["main"]["temp"];
	doc.clear();
	DebugLog("getWeatherApiTemperature: Temperature=" + String(temperature));

	return temperature;
}

///	@brief Updates the _weatherApiTemperature via API from
/// @return If the value has changed
bool updateWeatherApiTemperature()
{
	float temperature = getWeatherApiTemperature();
	bool changed = _weatherApiTemperature != temperature;
	_weatherApiTemperature = temperature;
	return changed;
}

/// @brief Updates the _outputTemperature based on the input sensor, weather API or manual temperature
/// @return If the value has changed
bool updateOutputTemperature()
{
	float targetTemp = NAN;
	if (_manualMode)
	{
		targetTemp = _temperatureManual;
	}
	else
	{
		float inTemp = _thermistorInTemperature;
		if (!isnanf(inTemp))
		{
			targetTemp = inTemp + _temperatureOffset;
		}
		else
		{
			float apiTemp = _weatherApiTemperature;
			if (!isnanf(apiTemp))
			{
				targetTemp = apiTemp + _temperatureOffset;
			}
			else
			{
				// TODO: Try getting anouther fallback like the last known temperature saved in preferences?
				targetTemp = _temperatureManual;
			}
		}
	}

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
		if (isnanf(_thermistorInTemperature) || abs(medianTemp - _thermistorInTemperature) > 0.01)
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

/// @brief Configure Preferences for storing data
void setupPreferences()
{
	// Note: Namespace name is limited to 15 chars.
	_preferences.begin(PREF_KEY_NAMESPACE, false);

	// Note: Key name is limited to 15 chars.
	_manualMode = _preferences.getBool(PREF_KEY_MANUAL_MODE, false);
	_temperatureOffset = _preferences.getInt(PREF_KEY_TEMP_OFFSET, 0); // TODO: Move into area
	_temperatureManual = _preferences.getInt(PREF_KEY_TEMP_MANUAL, 15);
}

/// @brief Setup for Web UI (called by setupWifiManager after auto connect)
void setupWebUi()
{
	// Create global labels
	_lblSensorTemp = ESPUI.addControl(Label, "Sensor", String(_thermistorInTemperature) + " °C", None);
	_lblWeatherTemp = ESPUI.addControl(Label, "Weather", String(_weatherApiTemperature) + " °C", None);
	_lblOutputTemp = ESPUI.addControl(Label, "Output", String(_outputTemperature) + " °C", None);

	// Generate tabs
	_tabTemperature = ESPUI.addControl(Tab, "Temperature", "Temperature");
	_tabPower = ESPUI.addControl(Tab, "Power", "Power");
	_tabSystemInfo = new TabSystemInfo();
	_tabWifiInfo = new TabWifiInfo(&_wifiManager);

	_swManualMode = ESPUI.addControl(
		Switcher, "Manual Temperature", String(_manualMode ? 1 : 0), None, _tabTemperature,
		[](Control *sender, int type)
		{
			bool value = sender->value.toInt() > 0;
			if (_manualMode != value)
			{
				_manualMode = value;
				_preferences.putBool(PREF_KEY_MANUAL_MODE, _manualMode);
				ESPUI.updateSlider(sender->id, value);
			}
		});

	_sldManualTemp = ESPUI.addControl(
		Slider, "Temperature", String(_temperatureManual), None, _swManualMode,
		[](Control *sender, int type)
		{
			int value = sender->value.toInt();
			if (_temperatureManual != value)
			{
				_temperatureManual = value;
				_preferences.putInt(PREF_KEY_TEMP_MANUAL, _temperatureManual);
				ESPUI.updateSlider(sender->id, value);
			}
		});
	ESPUI.addControl(Min, "", "-25", None, _sldManualTemp);
	ESPUI.addControl(Max, "", "25", None, _sldManualTemp);
	ESPUI.addControl(Step, "", "1", None, _sldManualTemp);

	// TODO: Offset should be moved into the area
	_sldOffset = ESPUI.addControl(
		Slider, "Offset", String(_temperatureOffset), None, _tabTemperature,
		[](Control *sender, int type)
		{
			int value = sender->value.toInt();
			if (_temperatureOffset != value)
			{
				_temperatureOffset = value;
				_preferences.putInt(PREF_KEY_TEMP_OFFSET, _temperatureOffset);
				ESPUI.updateSlider(sender->id, value);
			}
		});
	ESPUI.addControl(Min, "", "-15", None, _sldOffset);
	ESPUI.addControl(Max, "", "15", None, _sldOffset);
	ESPUI.addControl(Step, "", "1", None, _sldOffset);
	
	// Start ESP UI https://github.com/s00500/ESPUI
	ESPUI.begin("Heat Pump Champ");
}

/// @brief Setup for Weather API
void setupWeatherApi()
{
	if (WEATHER_API_KEY.length() < 1)
	{
		DebugLog("setupWeatherApi: API key missing, weather API can't be used!");
		return;
	}

	if (WEATHER_CITY_ID > 0)
	{
		DebugLog("setupWeatherApi: Using City ID");
		_weatherApiUrl = "https://api.openweathermap.org/data/2.5/weather?id=" + String(WEATHER_CITY_ID) + "&lang=en" + "&units=METRIC" + "&appid=" + WEATHER_API_KEY;
	}
	else if (WEATHER_LATITUDE != 0 && WEATHER_LONGITUDE != 0)
	{
		DebugLog("setupWeatherApi: Using Latitude and Longitude");
		_weatherApiUrl = "https://api.openweathermap.org/data/2.5/weather?lat=" + String(WEATHER_LATITUDE, 6) + "&lon=" + String(WEATHER_LONGITUDE, 6) + "&lang=en" + "&units=METRIC" + "&appid=" + WEATHER_API_KEY;
	}
	else
	{
		DebugLog("setupWeatherApi: Location missing, please define CityID or Latitude + Longitude!");
		return;
	}

	DebugLog("setupWeatherApi: _weatherApiUrl=" + _weatherApiUrl);
	_weatherApiTemperature = getWeatherApiTemperature(); // initial weather update
	DebugLog("setupWeatherApi: initial _weatherApiTemperature=" + String(_weatherApiTemperature));

	_timers.every(
		WEATHER_API_UPDATE_CYCLE,
		[](void *opaque) -> bool
		{
			if (updateWeatherApiTemperature())
			{
				ESPUI.updateLabel(_lblWeatherTemp, String(_weatherApiTemperature) + " °C");
			}

			return true; // Keep timer running
		});
}

/// @brief Setup for WiFiManager as blocking implementation if not configured.
void setupWifiManager()
{
	// explicitly set mode, esp defaults to STA+AP
	WiFi.mode(WIFI_STA);

	// Start config portal only if there is no WiFi configuration
	// And enable portal blocking for configuration
	bool configurued = _wifiManager.getWiFiIsSaved();
	_wifiManager.setEnableConfigPortal(!configurued);
	_wifiManager.setConfigPortalBlocking(!configurued);
	_wifiManager.setConfigPortalTimeout(86400); // 24h timeout

	// automatically connect using saved credentials if they exist
	// If connection fails it starts an access point with the specified name
	bool connected = _wifiManager.autoConnect("", "123456789");
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
	// initial reading without building a median
	_thermistorInTemperature = getCurrentThermistorInTemperature(true);
	DebugLog("setupThermistorInputReading: initial _thermistorInTemperature=" + String(_thermistorInTemperature));

	_timers.every(
		TEMP_IN_SAMPLE_CYCLE,
		[](void *opaque) -> bool
		{
			if (updateThermistorInTemperature())
			{
				ESPUI.updateLabel(_lblSensorTemp, String(_thermistorInTemperature) + " °C");
			}

			return true; // Keep timer running
		});
}

/// @brief Setup of the power limiter via 0-10V analog output
void setupPowerLimit()
{
	// TODO: implement 0-10V analog output
}

/// @brief Setup digital potentiometer for output temperature
void setupOutputTemperature()
{
	_spiDigitalPoti = new SPIClass(SPI_BUS_THERMISTOR_OUT);
	_spiDigitalPoti->begin();
	pinMode(_spiDigitalPoti->pinSS(), OUTPUT);
	pinMode(GPIO_FAILOVER_OUT, OUTPUT);

	updateOutputTemperature();
	_timers.every(
		TEMP_OUT_UPDATE_CYCLE,
		[](void *opaque) -> bool
		{
			if (updateOutputTemperature())
			{
				ESPUI.updateLabel(_lblOutputTemp, String(_outputTemperature) + " °C");
			}

			return true;
		});
}

/// @brief Put your setup code here, to run once:
void setup()
{
	if (DEBUG_LOGGING)
	{
		Serial.begin(115200);
		delay(2000); // Delay for serial monitor attach
	}

	setupPreferences();
	setupThermistorInputReading();
	setupWifiManager();
	setupWeatherApi();
	setupPowerLimit();
	setupOutputTemperature();
	setupWebUi();
}