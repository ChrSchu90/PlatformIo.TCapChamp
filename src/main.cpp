#include <Arduino.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPDash.h>
#include <arduino-timer.h>
#include <HTTPClient.h>
#include <RunningMedian.h>
#include <SPI.h>
#include "ThermistorCalc.h"
#include "Secrets.h"

static const bool DEBUG_LOGGING = true;														// @brief Enable/disable logs to Serial println
static const uint8_t SPI_BUS_THERMISTOR_OUT = VSPI;											// @brief SPI bus used for digital potentiometer for output temperature
static const char *PREF_KEY_MANUAL_MODE = "ManualMode";										// @brief Preferences key for manual mode
static const char *PREF_KEY_TEMP_OFFSET = "TempOffet";										// @brief Preferences key for temperature offset
static const char *PREF_KEY_TEMP_MANUAL = "TempManual";										// @brief Preferences key for manual temperature
static const byte GPIO_THERMISTOR_IN = 36;													// @brief GPIO used for real input temperature from thermistor
static const byte GPIO_FAILOVER_OUT = 27;													// @brief GPIO used for real input temperature from thermistor
static const unsigned int WEB_UI_UPDATE_CYCLE = 10000;										// @brief Update time for Web UI in milliseconds
static const float SUPPLY_VOLTAGE = 3.3;													// @brief Maximum Voltage ADC input
static const unsigned int WEATHER_API_UPDATE_CYCLE = 600000;								// @brief Update time of the temperture by the weather API in milliseconds
static const unsigned int TEMP_OUT_UPDATE_CYCLE = 10000;									// @brief Update time of the output temperature in milliseconds
static const unsigned int TEMP_IN_DEVIDER_RESISTANCE = 10000;								// @brief Voltage divider resistor value for input temperature in Ohm
static const unsigned int TEMP_IN_SAMPLE_CYCLE = 100;										// @brief Sample rate to build the median in milliseconds
static const unsigned int TEMP_IN_UPDATE_CYCLE = 10000;										// @brief Update every n ms the input temperature
static const unsigned int TEMP_IN_SAMPLE_CNT = TEMP_IN_UPDATE_CYCLE / TEMP_IN_SAMPLE_CYCLE; // @brief Amount of samples for input thermistor median calculation
static const unsigned int DIGI_POTI_STEPS = 256;											// @brief Maximum amount of steps that the digital potentiometer supports
static const unsigned int DIGI_POTI_STEPMIN = 9;											// @brief Minimum step of the digital potentiometer to limit maximum current
static const float DIGI_POTI_RESISTANCE = 100000.0f;										// @brief Maximum resistance of the digital potentiometer in Ohm

RunningMedian _thermistorInMedian = RunningMedian(TEMP_IN_SAMPLE_CNT);
ThermistorCalc *_thermistorIn = new ThermistorCalc(-40, 167820, 25, 6523, 120, 302);  // @brief Input for real temperature (Panasonic PAW-A2W-TSOD)
ThermistorCalc *_thermistorOut = new ThermistorCalc(-40, 167820, 25, 6523, 120, 302); // @brief Output that simulates a Panasonic PAW-A2W-TSOD for the Panasonic T-Cap 16 KW Kit-WQC16H9E8
Timer<5, millis> _timers;
Preferences _preferences;
WiFiManager _wifiManager;
AsyncWebServer _serverWebUi(80);
ESPDash _dashboard(&_serverWebUi);
Card _tempInCard(&_dashboard, TEMPERATURE_CARD, "Temperature Sensor", "°C");
Card _tempApiCard(&_dashboard, TEMPERATURE_CARD, "Temperature API", "°C");
Card _tempOutCard(&_dashboard, TEMPERATURE_CARD, "Temperature Output", "°C");
Card _tempOffsetCard(&_dashboard, SLIDER_CARD, "Temperature Offset", "", 0, 15, 1);
Card _manualModeCard(&_dashboard, BUTTON_CARD, "Manual Mode");
Card _manualTempCard(&_dashboard, SLIDER_CARD, "Manual Temperature", "", -18, 40, 1);
SPIClass *_spiDigitalPoti;
String _weatherApiUrl;
bool _manualMode;
int _temperatureOffset;
int _temperatureManual;
float _thermistorInTemperature = NAN;
float _weatherApiInTemperature = NAN;
float _outputTemperature = NAN;
unsigned int _digiPotPosition = DIGI_POTI_STEPS / 2;

/// @brief Logs the given message if debug is active
void DebugLog(String message)
{
	if (DEBUG_LOGGING)
	{
		Serial.println(message);
	}
}

/// @brief Reads the ADC voltage with none linear compensation (maximum reading 3.3V range from 0 to 4095)
float readAdcVoltageCorrected(byte gpio)
{
	u_int16_t reading = analogRead(gpio);
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

///	@brief Updates the temperature via API from OpenWeatherMap see https://openweathermap.org/current
void updateWeatherApi()
{
	if (_weatherApiUrl.length() < 1)
	{
		return;
	}

	if (WiFi.status() != WL_CONNECTED)
	{
		_weatherApiInTemperature = NAN;
		DebugLog("updateWeatherApi: WiFi not connected");
		return;
	}

	HTTPClient http;
	http.begin(_weatherApiUrl);
	int httpCode = http.GET();
	if (httpCode != 200)
	{
		DebugLog("updateWeatherApi: HTTP Error = " + String(httpCode));
		JsonDocument doc;
		DeserializationError error = deserializeJson(doc, http.getString());
		if (error)
		{
			_weatherApiInTemperature = NAN;
			http.end();
			return;
		}

		String errMessage = doc["message"];
		DebugLog("updateWeatherApi: Message = " + errMessage);
		_weatherApiInTemperature = NAN;
		http.end();
		return;
	}

	String response = http.getString();
	DebugLog("updateWeatherApi: API response = " + String(response));

	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, response);
	if (error)
	{
		DebugLog("updateWeatherApi: DeserializationError = " + String(error.c_str()));
		_weatherApiInTemperature = NAN;
		http.end();
		return;
	}

	float temperature = doc["main"]["temp"];
	DebugLog("updateWeatherApi: Temperature = " + String(temperature));
	_weatherApiInTemperature = temperature;
	http.end();
}

// @brief Updates the output temperature based on the input sensor, weather API or manual temperature
void updateOutputTemperature()
{
	float outTemp;
	if (_manualMode)
	{
		outTemp = _temperatureManual;
	}
	else
	{
		if (!isnanf(_thermistorInTemperature))
		{
			outTemp = _thermistorInTemperature + _temperatureOffset;
		}
		else
		{
			if (!isnanf(_weatherApiInTemperature))
			{
				outTemp = _weatherApiInTemperature + _temperatureOffset;
			}
			else
			{
				// TODO: Try getting anouther fallback like the last known temperature saved in preferences?
				outTemp = _temperatureManual;
			}
		}
	}

	_outputTemperature = outTemp;

	float resistance = _thermistorOut->resistanceFromCelsius(outTemp);
	unsigned int pos = roundf(resistance / (DIGI_POTI_RESISTANCE / DIGI_POTI_STEPS));
	DebugLog("updateOutputTemperature: outTemp=" + String(outTemp) + " resistance=" + String(resistance) + " pos=" + String(pos));
	pos = constrain(pos, DIGI_POTI_STEPMIN, DIGI_POTI_STEPS - 1);
	if (_digiPotPosition != pos)
	{
		_spiDigitalPoti->transfer16(pos);
		_digiPotPosition = pos;
	}

	digitalWrite(GPIO_FAILOVER_OUT, HIGH);
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
	// Open Preferences with my-app namespace. Each application module, library, etc
	// has to use a namespace name to prevent key name collisions. We will open storage in
	// RW-mode (second parameter has to be false).
	// Note: Namespace name is limited to 15 chars.
	_preferences.begin("HeatPumpChamp", false);

	// TODO: Add into WebUI
	// Remove all _preferences under the opened namespace
	// _preferences.clear();
	// Or remove the counter key only
	// _preferences.remove("counter");

	// Get the counter value, if the key does not exist, return a default value of 0
	// Note: Key name is limited to 15 chars.
	_manualMode = _preferences.getBool(PREF_KEY_MANUAL_MODE, false);
	_temperatureOffset = _preferences.getInt(PREF_KEY_TEMP_OFFSET, 0);
	_temperatureManual = _preferences.getInt(PREF_KEY_TEMP_MANUAL, 15);

	// Store the counter to the Preferences
	// _preferences.putUInt("counter", counter);
}

/// @brief Setup for Web UI (called by setupWifiManager after auto connect)
void setupWebUi()
{
	_tempInCard.update(_thermistorInTemperature);
	_tempApiCard.update(_weatherApiInTemperature);
	_tempOutCard.update(_outputTemperature);
	_dashboard.sendUpdates();

	// Call Web UI update every N ms without blocking
	_timers.every(
		WEB_UI_UPDATE_CYCLE,
		[](void *opaque) -> bool
		{
			_tempInCard.update(_thermistorInTemperature);
			_tempApiCard.update(_weatherApiInTemperature);
			_tempOutCard.update(_outputTemperature);
			_dashboard.sendUpdates();
			return true; // Keep timer running
		});

	_tempOffsetCard.update(_temperatureOffset); // Set value from stored config
	_tempOffsetCard.attachCallback(
		[&](int value)
		{
			DebugLog("_tempOffsetCard: Slider Callback Triggered: " + String(value));
			if (_temperatureOffset != value)
			{
				_temperatureOffset = value;
				_tempOffsetCard.update(_temperatureOffset);
				_dashboard.sendUpdates();
				_preferences.putInt(PREF_KEY_TEMP_OFFSET, _temperatureOffset);
			}
		});

	_manualTempCard.update(_temperatureManual); // Set value from stored config
	_manualTempCard.attachCallback(
		[&](int value)
		{
			DebugLog("_manualTempCard: Slider Callback Triggered: " + String(value));
			if (_temperatureManual != value)
			{
				_temperatureManual = value;
				_manualTempCard.update(_temperatureManual);
				_dashboard.sendUpdates();
				_preferences.putInt(PREF_KEY_TEMP_MANUAL, _temperatureManual);
			}
		});

	_manualModeCard.update(_manualMode ? 1 : 0); // Set value from stored config
	_manualModeCard.attachCallback(
		[&](int value)
		{
			DebugLog("_manualModeCard: Button Callback Triggered: " + String((value == 1) ? "true" : "false"));
			if (_manualMode != (value == 1))
			{
				_manualModeCard.update(value);
				_dashboard.sendUpdates();
				_manualMode = value == 1;
				_preferences.putBool(PREF_KEY_MANUAL_MODE, _manualMode);
			}
		});

	_serverWebUi.begin();
}

/// @brief Setup for WiFiManager as none blocking implementation
void setupWifiManager()
{
	// explicitly set mode, esp defaults to STA+AP
	WiFi.mode(WIFI_STA);

	// TODO: Add into WebUI
	// reset settings - wipe credentials for testing
	// wm.resetSettings();

	_wifiManager.setConfigPortalBlocking(false);
	_wifiManager.setConfigPortalTimeout(600);

	// automatically connect using saved credentials if they exist
	// If connection fails it starts an access point with the specified name
	if (_wifiManager.autoConnect(_wifiManager.getDefaultAPName().c_str(), "123456789"))
	{
		DebugLog("setupWifiManager: Connected!");
		setupWebUi();
	}
	else
	{
		DebugLog("setupWifiManager: Configportal running...");
	}
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
		DebugLog("setupWeatherApi: Location missing, please define CityID or Latitude and Longitude!");
		return;
	}

	DebugLog("setupWeatherApi: _weatherApiUrl = " + _weatherApiUrl);
	updateWeatherApi(); // initial weather update
	_timers.every(
		WEATHER_API_UPDATE_CYCLE,
		[](void *opaque) -> bool
		{
			updateWeatherApi();
			return true;
		});
}

/// @brief Setup reading of input thermistor
void setupThermistorInputReading()
{
	_timers.every(
		TEMP_IN_SAMPLE_CYCLE,
		[](void *opaque) -> bool
		{
			float voltageIn = readAdcVoltageCorrected(GPIO_THERMISTOR_IN);
			if (!(voltageIn > 0))
			{
				if (!isnanf(_thermistorInTemperature))
				{
					_thermistorInMedian.clear();
					_thermistorInTemperature = NAN;
					DebugLog("thermistorInUpdateTimer: Voltage unplausible or devider resistor defect. voltageIn = " + String(voltageIn));
				}

				return true; // Keep timer running
			}

			float thermistorResistance = TEMP_IN_DEVIDER_RESISTANCE * voltageIn / (SUPPLY_VOLTAGE - voltageIn);
			if (thermistorResistance == infinityf())
			{
				if (!isnanf(_thermistorInTemperature))
				{
					_thermistorInMedian.clear();
					_thermistorInTemperature = NAN;
					DebugLog("thermistorInUpdateTimer: No external temperature sensor connected. thermistorResistance = " + String(thermistorResistance));
				}

				return true; // Keep timer running
			}

			float tempIn = _thermistorIn->celsiusFromResistance(thermistorResistance);
			_thermistorInMedian.add(tempIn);
			if ((_thermistorInMedian.getCount() % TEMP_IN_SAMPLE_CNT) == 0)
			{
				_thermistorInTemperature = _thermistorInMedian.getMedianAverage(TEMP_IN_SAMPLE_CNT);
				DebugLog("thermistorInUpdateTimer: _thermistorInTemperature = " + String(_thermistorInTemperature));
				_thermistorInMedian.clear();
			}

			return true; // Keep timer running
		});
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
			updateOutputTemperature();
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
	setupOutputTemperature();
	setupWifiManager();
	setupWeatherApi();
}