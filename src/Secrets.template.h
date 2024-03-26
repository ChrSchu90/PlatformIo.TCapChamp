
/**
 * @brief Contains secrets like weather API key
 */

#pragma once

#include <WString.h>

/// @brief API key for OpenWeatherMap. Can be created for free at
/// https://home.openweathermap.org/users/sign_up (60 calls/minute or 1,000,000 calls/month)
static const String WEATHER_API_KEY = "";

/// @brief City ID for weather API. Can be taken from https://openweathermap.org/ by search from URL (https://openweathermap.org/city/xxxxxxx)
static const unsigned int WEATHER_CITY_ID = 0;

/// @brief City ID is sometimes very hard to find, instead you can use Latitude + Longitude (can be taken from google maps with right-click)
static const double WEATHER_LATITUDE = 0;

/// @brief City ID is sometimes very hard to find, instead you can use Latitude + Longitude (can be taken from google maps with right-click)
static const double WEATHER_LONGITUDE = 0;

/// @brief Initial password for WiFi configuration AP 
static const String WIFI_CONFIG_PASSWORD = "123456789";