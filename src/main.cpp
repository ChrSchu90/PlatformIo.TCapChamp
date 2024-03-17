#include <Arduino.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPDash.h>
#include "ThermistorCalc.h"

WiFiManager wfm;
ThermistorCalc *thermistor;

AsyncWebServer server(80);
ESPDash dashboard(&server);
Card temperatureInCard(&dashboard, TEMPERATURE_CARD, "Temperature In", "°C");
Card temperatureOutCard(&dashboard, TEMPERATURE_CARD, "Temperature Out", "°C");
Card offsetCard(&dashboard, SLIDER_CARD, "Temperature Offset", "", 0, 15, 0);

/// @brief Setup for Web UI
void setupWebUi()
{
	offsetCard.attachCallback([&](float value)
	{
  		Serial.println("[offsetCard] Slider Callback Triggered: "+String(value));
  		offsetCard.update(value);
  		dashboard.sendUpdates();
	});

	server.begin();
}

/// @brief Setup for WiFiManager as none blocking implementation
void setupWifiManager()
{
	delay(5000);

	// explicitly set mode, esp defaults to STA+AP
	WiFi.mode(WIFI_STA);

	// reset settings - wipe credentials for testing
	// wm.resetSettings();

	wfm.setConfigPortalBlocking(false);
	wfm.setConfigPortalTimeout(600);

	// automatically connect using saved credentials if they exist
	// If connection fails it starts an access point with the specified name
	if (wfm.autoConnect(wfm.getDefaultAPName().c_str(), "123456789"))
	{
		Serial.println("connected...yeey :)");
		setupWebUi();
	}
	else
	{
		Serial.println("Configportal running");
	}
}

/// @brief Put your setup code here, to run once:
void setup()
{
	Serial.begin(115200);
	setupWifiManager();
	thermistor = new ThermistorCalc(-40, 167820, 25, 6523, 120, 302);
}

/// @brief Put your main code here, to run repeatedly:
void loop()
{
	wfm.process();

	temperatureInCard.update((int)random(0, 50));
	temperatureOutCard.update((int)random(0, 100));
	dashboard.sendUpdates();

	// double c = thermistor->celsiusFromResistance(7701);
	// double r = thermistor->resistanceFromCelsius(21);
	delay(2000);
}