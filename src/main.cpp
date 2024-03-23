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

static const bool DEBUG_LOGGING = true;											 // @brief Enable/disable logs to Serial println
static const uint8_t SPI_BUS_THERMISTOR_OUT = VSPI;								 // @brief SPI bus used for digital potentiometer for output temperature
static const char *PREF_KEY_MANUAL_MODE = "ManualMode";							 // @brief Preferences key for manual mode
static const char *PREF_KEY_TEMP_OFFSET = "TempOffet";							 // @brief Preferences key for temperature offset
static const char *PREF_KEY_TEMP_MANUAL = "TempManual";							 // @brief Preferences key for manual temperature
static const byte GPIO_THERMISTOR_IN = 36;										 // @brief GPIO used for real input temperature from thermistor
static const byte GPIO_FAILOVER_OUT = 27;										 // @brief GPIO used for real input temperature from thermistor
static const float NO_TEMPERATURE = -273.15;									 // @brief Replacement value for none valid temperature
static const int WEB_UI_UPDATE_CYCLE = 10000;									 // @brief Update time for Web UI in milliseconds
static const int DEVIDER_RESISTANCE_TEMP_IN = 10000;							 // @brief Voltage divider resistor value for input temperature in Ohm
static const float SUPPLY_VOLTAGE = 3.3;										 // @brief Maximum Voltage ADC input
static const int SAMPLE_TIME_TEMP_IN = 100;										 // @brief Sample rate to build the median in milliseconds
static const int UPDATE_TIME_TEMP_IN = 10000;									 // @brief Update every n ms the input temperature
static const int SAMPLE_CNT_TEMP_IN = UPDATE_TIME_TEMP_IN / SAMPLE_TIME_TEMP_IN; // @brief Amount of samples for input thermistor median calculation

RunningMedian thermistorInSamples = RunningMedian(SAMPLE_CNT_TEMP_IN);
ThermistorCalc *thermistorIn = new ThermistorCalc(-40, 167820, 25, 6523, 120, 302);	 // @brief Input for real temperature (Panasonic PAW-A2W-TSOD)
ThermistorCalc *thermistorOut = new ThermistorCalc(-40, 167820, 25, 6523, 120, 302); // @brief Output that simulates a Panasonic PAW-A2W-TSOD for the Panasonic T-Cap 16 KW Kit-WQC16H9E8
Timer<5, millis> _timers;
Preferences preferences;
WiFiManager wifiManager;
AsyncWebServer server(80);
ESPDash dashboard(&server);
Card tempInCard(&dashboard, TEMPERATURE_CARD, "Temperature Sensor", "°C");
Card tempApiCard(&dashboard, TEMPERATURE_CARD, "Temperature API", "°C");
Card tempOutCard(&dashboard, TEMPERATURE_CARD, "Temperature Output", "°C");
Card tempOffsetCard(&dashboard, SLIDER_CARD, "Temperature Offset", "", 0, 15, 1);
Card manualModeCard(&dashboard, BUTTON_CARD, "Manual Mode");
Card manualTempCard(&dashboard, SLIDER_CARD, "Manual Temperature", "", -18, 40, 1);
SPIClass *vspi;

String weatherApiUrl;
bool manualMode;
int tempOffset;
int tempManual;
float thermistorInTemperature = NAN;
float weatherApiInTemperature = NAN;
float outputTemperature = NAN;

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

///	@brief Updates the temperature via API from OpenWeatherMap
void updateWeatherApi()
{
	if (weatherApiUrl.length() < 1)
	{
		return;
	}

	if (WiFi.status() != WL_CONNECTED)
	{
		weatherApiInTemperature = NAN;
		DebugLog("updateWeatherApi: WiFi not connected");
		return;
	}

	HTTPClient http;
	http.begin(weatherApiUrl);
	int httpCode = http.GET();
	if (httpCode != 200)
	{
		DebugLog("updateWeatherApi: HTTP Error = " + String(httpCode));
		JsonDocument doc;
		DeserializationError error = deserializeJson(doc, http.getString());
		if (error)
		{
			weatherApiInTemperature = NAN;
			http.end();
			return;
		}

		String errMessage = doc["message"];
		DebugLog("updateWeatherApi: Message = " + errMessage);
		weatherApiInTemperature = NAN;
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
		weatherApiInTemperature = NAN;
		http.end();
		return;
	}

	float temperature = doc["main"]["temp"];
	DebugLog("updateWeatherApi: Temperature = " + String(temperature));
	weatherApiInTemperature = temperature;
	http.end();
}

/// @brief Put your main code here, to run repeatedly:
void loop()
{
	_timers.tick();
	wifiManager.process();
}

/// @brief Configure Preferences for storing data
void setupPreferences()
{
	// Open Preferences with my-app namespace. Each application module, library, etc
	// has to use a namespace name to prevent key name collisions. We will open storage in
	// RW-mode (second parameter has to be false).
	// Note: Namespace name is limited to 15 chars.
	preferences.begin("HeatPumpChamp", false);

	// TODO: Add into WebUI
	// Remove all preferences under the opened namespace
	// preferences.clear();
	// Or remove the counter key only
	// preferences.remove("counter");

	// Get the counter value, if the key does not exist, return a default value of 0
	// Note: Key name is limited to 15 chars.
	manualMode = preferences.getBool(PREF_KEY_MANUAL_MODE, false);
	tempOffset = preferences.getInt(PREF_KEY_TEMP_OFFSET, 0);
	tempManual = preferences.getInt(PREF_KEY_TEMP_MANUAL, 15);

	// Store the counter to the Preferences
	// preferences.putUInt("counter", counter);
}

/// @brief Setup for Web UI (called by setupWifiManager after auto connect)
void setupWebUi()
{
	tempInCard.update(thermistorInTemperature);
	tempApiCard.update(weatherApiInTemperature);
	tempOutCard.update(outputTemperature);
	dashboard.sendUpdates();

	// Call Web UI update every N ms without blocking
	_timers.every(
		WEB_UI_UPDATE_CYCLE,
		[](void *opaque) -> bool
		{
			tempInCard.update(thermistorInTemperature);
			tempApiCard.update(weatherApiInTemperature);
			tempOutCard.update(outputTemperature);
			dashboard.sendUpdates();
			return true; // Keep timer running
		});

	tempOffsetCard.update(tempOffset); // Set value from stored config
	tempOffsetCard.attachCallback(
		[&](int value)
		{
			DebugLog("tempOffsetCard: Slider Callback Triggered: " + String(value));
			if (tempOffset != value)
			{
				tempOffset = value;
				tempOffsetCard.update(tempOffset);
				dashboard.sendUpdates();
				preferences.putInt(PREF_KEY_TEMP_OFFSET, tempOffset);
			}
		});

	manualTempCard.update(tempManual); // Set value from stored config
	manualTempCard.attachCallback(
		[&](int value)
		{
			DebugLog("manualTempCard: Slider Callback Triggered: " + String(value));
			if (tempManual != value)
			{
				tempManual = value;
				manualTempCard.update(tempManual);
				dashboard.sendUpdates();
				preferences.putInt(PREF_KEY_TEMP_MANUAL, tempManual);
			}
		});

	manualModeCard.update(manualMode ? 1 : 0); // Set value from stored config
	manualModeCard.attachCallback(
		[&](int value)
		{
			DebugLog("manualModeCard: Button Callback Triggered: " + String((value == 1) ? "true" : "false"));
			if (manualMode != (value == 1))
			{
				manualModeCard.update(value);
				dashboard.sendUpdates();
				manualMode = value == 1;
				preferences.putBool(PREF_KEY_MANUAL_MODE, manualMode);
			}
		});

	server.begin();
}

/// @brief Setup for WiFiManager as none blocking implementation
void setupWifiManager()
{
	// explicitly set mode, esp defaults to STA+AP
	WiFi.mode(WIFI_STA);

	// TODO: Add into WebUI
	// reset settings - wipe credentials for testing
	// wm.resetSettings();

	wifiManager.setConfigPortalBlocking(false);
	wifiManager.setConfigPortalTimeout(600);

	// automatically connect using saved credentials if they exist
	// If connection fails it starts an access point with the specified name
	if (wifiManager.autoConnect(wifiManager.getDefaultAPName().c_str(), "123456789"))
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
		weatherApiUrl = "https://api.openweathermap.org/data/2.5/weather?id=" + String(WEATHER_CITY_ID) + "&lang=en" + "&units=METRIC" + "&appid=" + WEATHER_API_KEY;
	}
	else if (WEATHER_LATITUDE != 0 && WEATHER_LONGITUDE != 0)
	{
		DebugLog("setupWeatherApi: Using Latitude and Longitude");
		weatherApiUrl = "https://api.openweathermap.org/data/2.5/weather?lat=" + String(WEATHER_LATITUDE, 6) + "&lon=" + String(WEATHER_LONGITUDE, 6) + "&lang=en" + "&units=METRIC" + "&appid=" + WEATHER_API_KEY;
	}
	else
	{
		DebugLog("setupWeatherApi: Location missing, please define CityID or Latitude and Longitude!");
		return;
	}

	DebugLog("setupWeatherApi: weatherApiUrl = " + weatherApiUrl);
	updateWeatherApi(); // initial weather update
	_timers.every(
		600000, // Every 10 minutes
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
		SAMPLE_TIME_TEMP_IN,
		[](void *opaque) -> bool
		{
			float voltageIn = readAdcVoltageCorrected(GPIO_THERMISTOR_IN);
			if (!(voltageIn > 0))
			{
				if (thermistorInTemperature != NAN)
				{
					thermistorInSamples.clear();
					thermistorInTemperature = NAN;
					DebugLog("thermistorInUpdateTimer: No thermistor connected.");
				}

				return true; // Keep timer running
			}

			float thermistorResistance = DEVIDER_RESISTANCE_TEMP_IN * voltageIn / (SUPPLY_VOLTAGE - voltageIn);
			if (thermistorResistance == infinityf())
			{
				if (thermistorInTemperature != NAN)
				{
					thermistorInSamples.clear();
					thermistorInTemperature = NAN;
					DebugLog("thermistorInUpdateTimer: No thermistor resistance unplausible or devider resistor defect.");
				}

				return true; // Keep timer running
			}

			float tempIn = thermistorIn->celsiusFromResistance(thermistorResistance);
			thermistorInSamples.add(tempIn);
			if ((thermistorInSamples.getCount() % SAMPLE_CNT_TEMP_IN) == 0)
			{
				thermistorInTemperature = thermistorInSamples.getMedianAverage(SAMPLE_CNT_TEMP_IN);
				DebugLog("thermistorInUpdateTimer: thermistorInTemperature = " + String(thermistorInTemperature));
				thermistorInSamples.clear();
			}

			return true; // Keep timer running
		});
}

/// @brief Setup digital potentiometer for output temperature
void setupOutputTemp()
{
	// TODO: Disable failover on other location
	// pinMode(GPIO_FAILOVER_OUT, OUTPUT);
	// digitalWrite(GPIO_FAILOVER_OUT, ledOn ? LOW : HIGH);

	vspi = new SPIClass(SPI_BUS_THERMISTOR_OUT);
	vspi->begin();
	pinMode(vspi->pinSS(), OUTPUT);

	// auto result = vspi->transfer16(0);
	// DebugLog("setupOutputTemp: set 0 result = " + String(result));
	// delay(5000);
	// result = vspi->transfer16(256);
	// DebugLog("setupOutputTemp: set 100 result = " + String(result));
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
	setupOutputTemp();
	setupWifiManager();
	setupWeatherApi();
}