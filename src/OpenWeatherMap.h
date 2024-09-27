#pragma once

#include <HTTPClient.h>

/// @brief API request error type
enum Error 
{
    /// @brief No error / request successful
    None = 0,
    /// @brief  WiFi is not connected
    WifiNotConnected = 1,
    /// @brief HTTP error
    HttpError = 2,
    /// @brief Error on json deserialization
    DeserializationFailed = 3,
};

/// @brief Represents the API response with temperature and error information if request failed
struct ApiResponse
{
    /// @brief Creates a new instance of an successful API request
    ApiResponse(float temperature) : successful(true), error(None), httpCode(HTTP_CODE_OK), temperature(temperature){};

    /// @brief Creates a new instance of an failed API request
    ApiResponse(Error error, int httpCode) : successful(false), error(error), httpCode(httpCode), temperature(NAN){};

    /// @brief Gets if the request was successful
    const bool successful;

    /// @brief Error
    const Error error;

    /// @brief HTTP code RFC7231
    const int httpCode;

    /// @brief Gets the termperature or NAN if request failed
    const float temperature;
};

/// @brief Implements the OpenWeatherMap API https://openweathermap.org/current
/// NOTE: Free API is limited to 60 calls/minute or 1,000,000 calls/month
/// A API key can be created for free at https://home.openweathermap.org/users/sign_up
class OpenWeatherMap
{
private:
    float _temperature;

protected:

public:

    /// @brief URL that is used for the API requests
    const String apiUrl;

    /// @brief Creates an instance of the OpenWeatherMap API
    /// @param apiKey Your API key
    /// @param cityId The city ID can be taken from https://openweathermap.org/ by search from URL (https://openweathermap.org/city/xxxxxxx)
    OpenWeatherMap(String apiKey, unsigned int cityId);

    /// @brief Creates an instance of the OpenWeatherMap API
    /// @param apiKey Your API key
    /// @param latitude Location latitude (can be taken from google maps with right-click)
    /// @param longitude Location longitude (can be taken from google maps with right-click)
    OpenWeatherMap(String apiKey, double latitude, double longitude);

    /// @brief Gets the last known valid temperature received by the API
    /// @return a valid temperature or NAN
    float getLastValidTemperature() { return _temperature; }

    /// @brief Sends a API request (may take a while and will block)
    /// @return the API response
    ApiResponse request();
};