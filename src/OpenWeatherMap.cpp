#define LOG_LEVEL NONE

#include <ArduinoJson.h>
#include <WiFi.h>
#include "OpenWeatherMap.h"
#include "SerialLogging.h"

OpenWeatherMap::OpenWeatherMap(String apiKey, unsigned int cityId) : apiUrl(String("https://api.openweathermap.org/data/2.5/weather?id=") + cityId + "&lang=en&units=METRIC&appid=" + apiKey)
{
    _temperature = NAN;
}

OpenWeatherMap::OpenWeatherMap(String apiKey, double latitude, double longitude) : apiUrl(String("https://api.openweathermap.org/data/2.5/weather?lat=") + String(latitude, 6) + "&lon=" + String(longitude, 6) + "&lang=en&units=METRIC&appid=" + apiKey)
{
    _temperature = NAN;
}

ApiResponse OpenWeatherMap::request()
{
    if (WiFi.status() != WL_CONNECTED)
    {
#ifdef LOG_WARNING
        LOG_WARNING(F("OpenWeatherMap"), F("request"), F("WiFi is not connected"));
#endif
        return ApiResponse(WifiNotConnected, HTTP_CODE_REQUEST_TIMEOUT);
    }

    HTTPClient client;
    client.setReuse(false);
    client.begin(apiUrl);
    int httpCode = client.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        client.end();
#ifdef LOG_ERROR
        LOG_ERROR(F("OpenWeatherMap"), F("request"), F("HTTP Error code = ") + httpCode);
#endif
        return ApiResponse(HttpError, httpCode);
    }

    auto response = client.getString();
    client.end();
#ifdef LOG_INFO
    LOG_INFO(F("OpenWeatherMap"), F("request"), F("response = ") + response);
#endif

    JsonDocument doc;
    auto error = deserializeJson(doc, response);
    if (error != DeserializationError::Ok)
    {
#ifdef LOG_ERROR
        LOG_ERROR(F("OpenWeatherMap"), F("request"), F("DeserializationError = ") + error.c_str());
#endif
        doc.clear();
        return ApiResponse(DeserializationFailed, httpCode);
    }

    _temperature = doc["main"]["temp"];
#ifdef LOG_ERROR
    LOG_ERROR(F("OpenWeatherMap"), F("request"), F("_temperature = ") + _temperature);
#endif
    doc.clear();
    return ApiResponse(_temperature);
}

/*
Example OpenWeather API response (https://api.openweathermap.org/data/2.5/weather?id=YourCityId&lang=en&units=METRIC&appid=YourApiKey):
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