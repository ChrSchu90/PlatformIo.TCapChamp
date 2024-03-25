/**
 * @file OpenWeatherMapApi.h
 * @author ChrSchu
 * @brief Implements the OpenWeatherMap API https://openweathermap.org/current
 * @date 2024-03-24
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <WString.h>
#include <math.h>

/**
 * @brief Implements the OpenWeatherMap API https://openweathermap.org/current
 * NOTE: Free API is limited to 60 calls/minute or 1,000,000 calls/month
 * A API key can be created for free at https://home.openweathermap.org/users/sign_up
 */

/// @brief Represents the API response with temperature and error information if request failed
struct ApiResponse
{
    /// @brief Creates a new insatnce of an successful API request
    /// @param temperature the successful received temperature
    ApiResponse(float temperature) : successful(true), errorMessage(""), temperature(temperature){};

    /// @brief Creates a new insatnce of an unsuccessful API request
    /// @param errorMessage The error message
    ApiResponse(String errorMessage) : successful(false), errorMessage(errorMessage), temperature(NAN){};

    /// @brief Gets if the request was successful
    const bool successful;

    /// @brief Gets the termperature or NAN if request failed
    const float temperature;

    /// @brief Gets the error message if the request has failed
    const String errorMessage;
};

class OpenWeatherMapApi
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
    OpenWeatherMapApi(String apiKey, unsigned int cityId);

    /// @brief Creates an instance of the OpenWeatherMap API
    /// @param apiKey Your API key
    /// @param latitude Location latitude (can be taken from google maps with right-click)
    /// @param longitude Location longitude (can be taken from google maps with right-click)
    OpenWeatherMapApi(String apiKey, double latitude, double longitude);

    /// @brief Gets the last known valid temperature received by the API
    /// @return a valid temperature or NAN
    float getLastValidTemperature() { return _temperature; }

    /// @brief Sends a API request (may take a while and will block)
    /// @return the API response
    ApiResponse requestTemperature();
};