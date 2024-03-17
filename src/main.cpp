#include <Arduino.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPDash.h>
#include <arduino-timer.h>
#include "ThermistorCalc.h"

#define KEY_MANUAL_MODE "ManualMode"
#define KEY_TEMP_OFFSET "TempOffet"
#define KEY_TEMP_MANUAL "TempManual"

Timer<> webUiUpdateTimer = timer_create_default();
Preferences preferences;
WiFiManager wfm;
ThermistorCalc *thermistor;
AsyncWebServer server(80);
ESPDash dashboard(&server);
Card tempInCard(&dashboard, TEMPERATURE_CARD, "Temperature In", "°C");
Card tempOutCard(&dashboard, TEMPERATURE_CARD, "Temperature Out", "°C");
Card tempOffsetCard(&dashboard, SLIDER_CARD, "Temperature Offset", "", 0, 15, 1);
Card manualModeCard(&dashboard, BUTTON_CARD, "Manual Mode");
Card manualTempCard(&dashboard, SLIDER_CARD, "Manual Temperature", "", -18, 40, 1);

bool manualMode;
int tempOffset;
int tempManual;

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
	webUiUpdateTimer.every(5000,
						   [](void *opaque) -> bool
						   {
							   tempInCard.update((int)random(-200, 500) * 0.1f);
							   tempOutCard.update((int)random(-200, 1000) * 0.1f);
							   dashboard.sendUpdates();
							   return true;
						   });

	tempOffsetCard.update(tempOffset); // Set value from stored config
	tempOffsetCard.attachCallback(
		[&](int value)
		{
			Serial.println("[tempOffsetCard] Slider Callback Triggered: " + String(value)); // TODO: Remove
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
			Serial.println("[manualTempCard] Slider Callback Triggered: " + String(value)); // TODO: Remove
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
			Serial.println("[manualModeCard] Button Callback Triggered: " + String((value == 1) ? "true" : "false")); // TODO: Remove
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

	wfm.setConfigPortalBlocking(false);
	wfm.setConfigPortalTimeout(600);

	// automatically connect using saved credentials if they exist
	// If connection fails it starts an access point with the specified name
	if (wfm.autoConnect(wfm.getDefaultAPName().c_str(), "123456789"))
	{
		Serial.println("connected...yeey :)"); // TODO: Remove
		setupWebUi();
	}
	else
	{
		Serial.println("Configportal running"); // TODO: Remove
	}
}

/// @brief Put your setup code here, to run once:
void setup()
{
	Serial.begin(115200);
	delay(2000); // Delay for serial monitor attach
	setupPreferences();
	setupWifiManager();
	thermistor = new ThermistorCalc(-40, 167820, 25, 6523, 120, 302);
}

/// @brief Put your main code here, to run repeatedly:
void loop()
{
	webUiUpdateTimer.tick();
	wfm.process();

	// double c = thermistor->celsiusFromResistance(7701);
	// double r = thermistor->resistanceFromCelsius(21);
	// delay(2000); // TODO: remove after update rates are defined by
}