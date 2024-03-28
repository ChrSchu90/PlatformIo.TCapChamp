#include "TemperatureConfig.h"

/*
##############################################
##             TemperatureArea              ##
##############################################
*/

/// @brief Creates a new instance of an TemperatureArea
/// @param index unique area index
/// @param config the parent configuration
/// @param preferences the app preferences to stroe the configuration
TemperatureArea::TemperatureArea(
    size_t index, TemperatureConfig *config, Preferences *preferences) : index(index), name("Area " + String(index + 1)), _config(config), _preferences(preferences)
{
    _enabled = preferences->getBool(String(KEY_SETING_TEMP_AREA_ENABLED + index).c_str(), false);
    _start = preferences->getInt(String(KEY_SETING_TEMP_AREA_START + index).c_str(), 0);
    _end = preferences->getInt(String(KEY_SETING_TEMP_AREA_END + index).c_str(), 0);
    _offset = preferences->getInt(String(KEY_SETING_TEMP_AREA_OFFSET + index).c_str(), 0);
};

/// @brief Enables/disables the area
/// @param enabled defines if the area is enabled
void TemperatureArea::setEnabled(bool enabled)
{
    if (enabled == _enabled)
    {
        return;
    }

    _enabled = enabled;
    _preferences->putBool(String(KEY_SETING_TEMP_AREA_ENABLED + index).c_str(), _enabled);
}

/// @brief Sets the start temperature of the area
/// @param start defines the start temperature of the area
void TemperatureArea::setStart(int start)
{
    if (start == _start)
    {
        return;
    }

    _start = start;
    _preferences->putInt(String(KEY_SETING_TEMP_AREA_START + index).c_str(), _start);
}

/// @brief Sets the start temperature of the area
/// @param end defines the end temperature of the area
void TemperatureArea::setEnd(int end)
{
    if (end == _end)
    {
        return;
    }

    _end = end;
    _preferences->putInt(String(KEY_SETING_TEMP_AREA_END + index).c_str(), _end);
}

/// @brief Sets the temperature offset of the area
/// @param end defines the temperature offset of the area
void TemperatureArea::setOffset(int offset)
{
    if (offset == _offset)
    {
        return;
    }

    _offset = offset;
    _preferences->putInt(String(KEY_SETING_TEMP_AREA_OFFSET + index).c_str(), _offset);
}

/// @brief Gets if the area is responsable for the given temperature
/// @param temperature the temperature
/// @return if is responsable
bool TemperatureArea::isResponsable(float temperature)
{
    return !isnanf(temperature) && _enabled && isValid() && temperature >= _start && temperature <= _end;
}

/// @brief Gets if the area configuiration is valid
bool TemperatureArea::isValid()
{
    return _start <= _end && _end >= _start;
}




/*
##############################################
##            TemperatureConfig             ##
##############################################
*/

/// @brief Creates a new instance of an TemperatureConfig
/// @param preferences the app preferences to stroe the configuration
TemperatureConfig::TemperatureConfig(Preferences *preferences) : _preferences(preferences)
{
    _manualMode = preferences->getBool(KEY_SETING_MANUAL_MODE, false);
    _manualTemperature = preferences->getInt(KEY_SETING_MANUAL_TEMP, 15);

    for (size_t i = 0; i < TEMP_AREA_AMOUNT; i++)
    {
        _areas[i] = new TemperatureArea(i, this, preferences);
    }
};

/// @brief Sets if the manual mode is active
/// @param manualMode manual mode enabled/disabled
void TemperatureConfig::setManualMode(bool manualMode)
{
    if (_manualMode == manualMode)
    {
        return;
    }

    _manualMode = manualMode;
    _preferences->putBool(KEY_SETING_MANUAL_MODE, _manualMode);
}

/// @brief Sets if the manual temperature
/// @param manualTemperature manual temperature
void TemperatureConfig::setManualTemperature(int manualTemperature)
{
    if (_manualTemperature == manualTemperature)
    {
        return;
    }

    _manualTemperature = manualTemperature;
    _preferences->putInt(KEY_SETING_MANUAL_TEMP, _manualTemperature);
}

/// @brief Get the output temperature based on the input and configuration
/// @param inputTemp the raw temperature that should be modified based on the configuration
/// @return the output temperature or NAN if it couldn't be calculated
float TemperatureConfig::getOutputTemperature(float inputTemp)
{
    if (_manualMode)
    {
        return _manualTemperature;
    }

    if (isnanf(inputTemp))
    {
        return inputTemp;
    }

    auto area = getArea(inputTemp);
    if (area != nullptr)
    {
        return inputTemp + area->getOffset();
    }

    return inputTemp;
}

/// @brief Gets the area that is responsable for the given temperature
/// @param temperature the temperature
/// @return the responsable area or nullptr if none could be found
TemperatureArea *TemperatureConfig::getArea(float temperature)
{
    if (isnanf(temperature))
    {
        return nullptr;
    }

    for (size_t i = 0; i < TEMP_AREA_AMOUNT; i++)
    {
        auto area = getArea(i);
        if (area->isResponsable(temperature))
        {
            return area;
        }
    }

    return nullptr;
}
