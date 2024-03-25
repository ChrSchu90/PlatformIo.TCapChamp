/**
 * @file OpenWeatherMapApi.cpp
 * @author ChrSchu
 * @brief Implements the OpenWeatherMap API https://openweathermap.org/current
 * @date 2024-03-24
 *
 * @copyright Copyright (c) 2024
 *
 */

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include "OpenWeatherMapApi.h"

/// @brief Creates an instance of the OpenWeatherMap API
/// @param apiKey Your API key
/// @param cityId The city ID can be taken from https://openweathermap.org/ by search from URL (https://openweathermap.org/city/xxxxxxx)
OpenWeatherMapApi::OpenWeatherMapApi(String apiKey, unsigned int cityId) : apiUrl("https://api.openweathermap.org/data/2.5/weather?id=" + String(cityId) + "&lang=en" + "&units=METRIC" + "&appid=" + apiKey)
{
}

/// @brief Creates an instance of the OpenWeatherMap API
/// @param apiKey Your API key
/// @param latitude Location latitude (can be taken from google maps with right-click)
/// @param longitude Location longitude (can be taken from google maps with right-click)
OpenWeatherMapApi::OpenWeatherMapApi(String apiKey, double latitude, double longitude) : apiUrl("https://api.openweathermap.org/data/2.5/weather?lat=" + String(latitude, 6) + "&lon=" + String(longitude, 6) + "&lang=en" + "&units=METRIC" + "&appid=" + apiKey)
{
}

/// @brief Sends a API request (may take a while and will block)
/// @return the API response
ApiResponse OpenWeatherMapApi::requestTemperature()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return ApiResponse("WiFi not connected");
    }

    HTTPClient client;
    client.setReuse(false);
    client.begin(apiUrl);
    int httpCode = client.GET();
    if (httpCode != 200)
    {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, client.getString());
        if (error || !doc.containsKey("message"))
        {
            client.end();
            doc.clear();
            return ApiResponse("HTTP Error " + String(httpCode));
        }

        String errMessage = doc["message"];
        client.end();
        doc.clear();
        return ApiResponse("HTTP Error=" + String(httpCode) + " Message=" + errMessage);
    }

    String response = client.getString();
    client.end();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    if (error)
    {
        doc.clear();
        return ApiResponse("DeserializationError=" + String(error.c_str()));
    }

    _temperature = doc["main"]["temp"];
    doc.clear();
    return ApiResponse(_temperature);
}

/*
Example API response data/2.5/weather:
{
    "base": "stations",
    "clouds": {
        "all": 20
    },
    "cod": 200,
    "coord": {
        "lat": 12.3456,
        "lon": 12.3456
    },
    "dt": 1711359903,
    "id": 1234567,
    "main": {
        "feels_like": 7.81,
        "humidity": 66,
        "pressure": 1008,
        "temp": 7.81,
        "temp_max": 9.01,
        "temp_min": 6.46
    },
    "name": "TownName",
    "sys": {
        "country": "DE",
        "id": 1234,
        "sunrise": 1711344083,
        "sunset": 1711389082,
        "type": 1
    },
    "timezone": 3600,
    "visibility": 10000,
    "weather": [
        {
            "description": "few clouds",
            "icon": "02d",
            "id": 801,
            "main": "Clouds"
        }
    ],
    "wind": {
        "deg": 0,
        "speed": 1.03
    }
}
*/