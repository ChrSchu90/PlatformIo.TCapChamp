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
#include "ThermistorCalc.h"
#include "Secrets.h"

#define KEY_MANUAL_MODE "ManualMode"
#define KEY_TEMP_OFFSET "TempOffet"
#define KEY_TEMP_MANUAL "TempManual"

static const bool debug = true;
static const byte gpioTermistorIn = 36;
static const int webUiUpdateTime = 10000;		  // Update time for Web UI in milliseconds
static const int deviderResistanceTempIn = 10000; // Voltage divider resistor value for input temperature in Ohm
static const float supplyVoltage = 3.3;			  // Maximum Voltage ADC input
static const int sampleTimeTempIn = 100;		  // Sample rate to build the median in milliseconds
static const int updateTimeTempIn = 10000;		  // Update every n ms the input temperature
static const int sampleCntTempIn = updateTimeTempIn / sampleTimeTempIn;
RunningMedian thermistorInSamples = RunningMedian(sampleCntTempIn);
ThermistorCalc *thermistorIn = new ThermistorCalc(-40, 167820, 25, 6523, 120, 302);	 // Values taken from Panasonic PAW-A2W-TSOD
ThermistorCalc *thermistorOut = new ThermistorCalc(-40, 167820, 25, 6523, 120, 302); // Values taken from Panasonic PAW-A2W-TSOD for Panasonic T-Cap 16 KW Kit-WQC16H9E8
Timer<> webUiUpdateTimer = timer_create_default();
Timer<> weatherApiUpdateTimer = timer_create_default();
Timer<> thermistorInUpdateTimer = timer_create_default();
Preferences preferences;
WiFiManager wifiManager;
AsyncWebServer server(80);
ESPDash dashboard(&server);
Card tempInCard(&dashboard, TEMPERATURE_CARD, "Temperature In", "°C");
Card tempOutCard(&dashboard, TEMPERATURE_CARD, "Temperature Out", "°C");
Card tempOffsetCard(&dashboard, SLIDER_CARD, "Temperature Offset", "", 0, 15, 1);
Card manualModeCard(&dashboard, BUTTON_CARD, "Manual Mode");
Card manualTempCard(&dashboard, SLIDER_CARD, "Manual Temperature", "", -18, 40, 1);

String weatherApiUrl;
bool inputThermistorConnected;
bool manualMode;
int tempOffset;
int tempManual;
float thermistorInTemperature;

/// @brief Logs the given message if debug is active
void DebugLog(String message)
{
	if (debug)
	{
		Serial.println(message);
	}
}

/// @brief Reads the ADC voltage with none linear compensation (maximum reading 3.3V range from 0 to 4095)
float readAdcVoltageCorrected(byte gpio)
{
	u_int16_t reading = analogRead(gpio);
	if (reading < 1 || reading > 4095)
	{
		return 0;
	}

	// Pre-calculated for 3.3V from 0 - 4095
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
			http.end();
			return;
		}

		String message = doc["message"];
		DebugLog("updateWeatherApi: Message = " + message);
		http.end();
		return;
	}

	String response = http.getString();
	DebugLog("updateWeatherApi: API response = " + String(response));

	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, response);
	if (error)
	{
		http.end();
		return;
	}

	float temp = doc["main"]["temp"];
	DebugLog("updateWeatherApi: Temperature = " + String(temp));
	http.end();
}

/// @brief Put your main code here, to run repeatedly:
void loop()
{
	// TODO: use timer pool because it is easier?!?
	webUiUpdateTimer.tick();
	weatherApiUpdateTimer.tick();
	thermistorInUpdateTimer.tick();
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
	manualMode = preferences.getBool(KEY_MANUAL_MODE, false);
	tempOffset = preferences.getInt(KEY_TEMP_OFFSET, false);
	tempManual = preferences.getInt(KEY_TEMP_MANUAL, false);

	// Store the counter to the Preferences
	// preferences.putUInt("counter", counter);
}

/// @brief Setup for Web UI (called by setupWifiManager after auto connect)
void setupWebUi()
{
	// Call Web UI update every N ms without blocking
	webUiUpdateTimer.every(
		webUiUpdateTime,
		[](void *opaque) -> bool
		{
			tempInCard.update(thermistorInTemperature);
			tempOutCard.update((int)random(-200, 1000) * 0.1f);
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
				preferences.putInt(KEY_TEMP_OFFSET, tempOffset);
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
				preferences.putInt(KEY_TEMP_MANUAL, tempManual);
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
				preferences.putBool(KEY_MANUAL_MODE, manualMode);
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
	weatherApiUpdateTimer.every(
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
	thermistorInUpdateTimer.every(
		sampleTimeTempIn,
		[](void *opaque) -> bool
		{
			double voltageIn = readAdcVoltageCorrected(gpioTermistorIn);
			inputThermistorConnected = voltageIn > 0;
			if (!inputThermistorConnected)
			{
				if (thermistorInSamples.getCount() > 0)
				{
					thermistorInSamples.clear();
					thermistorInTemperature = -273.15;
				}

				return true; // Keep timer running
			}

			double thermistorResistance = deviderResistanceTempIn * voltageIn / (supplyVoltage - voltageIn);
			double tempIn = thermistorIn->celsiusFromResistance(thermistorResistance);
			thermistorInSamples.add(tempIn);
			if ((thermistorInSamples.getCount() % sampleCntTempIn) == 0)
			{
				thermistorInTemperature = thermistorInSamples.getAverage(sampleCntTempIn);
				DebugLog("thermistorInUpdateTimer: TemperatureIn = " + String(thermistorInTemperature));
				thermistorInSamples.clear();
			}

			return true; // Keep timer running
		});
}

/// @brief Put your setup code here, to run once:
void setup()
{
	if (debug)
	{
		Serial.begin(115200);
		delay(2000); // Delay for serial monitor attach
	}

	setupPreferences();
	setupWifiManager();
	setupWeatherApi();
	setupThermistorInputReading();
}