#define LOG_LEVEL NONE

#include "Config.h"
#include "SerialLogging.h"

/*
##############################################
##                  Config                  ##
##############################################
*/

Config::Config() : _preferences(new Preferences())
{
    _preferences->begin(KEY_SETTINGS_NAMESPACE, false);

    temperatureConfig = new TemperatureConfig(_preferences);
    powerConfig = new PowerConfig(_preferences);
};

/*
##############################################
##            TemperatureConfig             ##
##############################################
*/

TemperatureConfig::TemperatureConfig(Preferences *preferences) : _preferences(preferences)
{
    _manualMode = preferences->getBool(KEY_SETTING_TEMP_MANUAL_MODE, false);
    _manualTemperature = preferences->getChar(KEY_SETTING_TEMP_MANUAL_TEMP, 15);

    for (size_t i = 0; i < TEMP_AREA_AMOUNT; i++)
    {
        _areas[i] = new TemperatureArea(i, this, preferences);
    }
};

bool TemperatureConfig::setManualMode(bool manualMode)
{
    if (_manualMode == manualMode)
    {
        return _manualMode;
    }

    _manualMode = manualMode;
    _preferences->putBool(KEY_SETTING_TEMP_MANUAL_MODE, _manualMode);
    return _manualMode;
}

int8_t TemperatureConfig::setManualTemperature(int8_t manualTemperature)
{
    if (_manualTemperature == manualTemperature)
    {
        return _manualTemperature;
    }

    _manualTemperature = manualTemperature;
    _preferences->putChar(KEY_SETTING_TEMP_MANUAL_TEMP, _manualTemperature);
    return _manualTemperature;
}

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

/*
##############################################
##             TemperatureArea              ##
##############################################
*/

TemperatureArea::TemperatureArea(size_t index, TemperatureConfig *config, Preferences *preferences) : index(index), name("Area " + String(index + 1)), _config(config), _preferences(preferences)
{
    _enabled = preferences->getBool(KEY_SETTING_TEMP_AREA_ENABLED + index, false);
    _start = preferences->getChar(KEY_SETTING_TEMP_AREA_START + index, 0);
    _end = preferences->getChar(KEY_SETTING_TEMP_AREA_END + index, 0);
    _offset = preferences->getChar(KEY_SETTING_TEMP_AREA_OFFSET + index, 0);
};

bool TemperatureArea::setEnabled(bool enabled)
{
    if (enabled == _enabled)
    {
        return _enabled;
    }

    _enabled = enabled;
    _preferences->putBool(KEY_SETTING_TEMP_AREA_ENABLED + index, _enabled);
    return _enabled;
}

int8_t TemperatureArea::setStart(int8_t start)
{
    start = min(max(start, (int8_t)MIN_TEMPERATURE), (int8_t)MAX_TEMPERATURE);
    if (start == _start)
    {
        return _start;
    }

    _start = start;
    _preferences->putChar(KEY_SETTING_TEMP_AREA_START + index, _start);
    return _start;
}

int8_t TemperatureArea::setEnd(int8_t end)
{
    end = min(max(end, (int8_t)MIN_TEMPERATURE), (int8_t)MAX_TEMPERATURE);
    if (end == _end)
    {
        return _end;
    }

    _end = end;
    _preferences->putChar(KEY_SETTING_TEMP_AREA_END + index, _end);
    return _end;
}

int8_t TemperatureArea::setOffset(int8_t offset)
{
    offset = min(max(offset, (int8_t)MIN_TEMPERATURE), (int8_t)20);
    if (offset == _offset)
    {
        return _offset;
    }

    _offset = offset;
    _preferences->putChar(KEY_SETTING_TEMP_AREA_OFFSET + index, _offset);
    return _offset;
}

bool TemperatureArea::isResponsable(float temperature)
{
    return !isnanf(temperature) && _enabled && isValid() && temperature >= _start && temperature <= _end;
}

bool TemperatureArea::isValid()
{
    return _start <= _end && _end >= _start;
}

/*
##############################################
##            PowerConfig             ##
##############################################
*/

PowerConfig::PowerConfig(Preferences *preferences) : _preferences(preferences)
{
    _manualMode = preferences->getBool(KEY_SETTING_POWER_MANUAL_MODE, false);
    _manualPower = preferences->getUChar(KEY_SETTING_POWER_MANUAL_POWER, 100);

    for (size_t i = 0; i < POWER_AREA_AMOUNT; i++)
    {
        _areas[i] = new PowerArea(i, this, preferences);
    }
};

bool PowerConfig::setManualMode(bool manualMode)
{
    if (_manualMode == manualMode)
    {
        return _manualMode;
    }

    _manualMode = manualMode;
    _preferences->putBool(KEY_SETTING_POWER_MANUAL_MODE, _manualMode);
    return _manualMode;
}

uint8_t PowerConfig::setManualPower(uint8_t manualPower)
{
    manualPower = min(max(manualPower, (uint8_t)MIN_POWER_LIMIT), (uint8_t)MAX_POWER_LIMIT);
    if (_manualPower == manualPower)
    {
        return _manualPower;
    }

    _manualPower = manualPower;
    _preferences->putUChar(KEY_SETTING_POWER_MANUAL_POWER, _manualPower);
    return _manualPower;
}

uint8_t PowerConfig::getOutputPowerLimit(float inputTemp)
{
    if (_manualMode)
    {
        return _manualPower;
    }

    if (isnanf(inputTemp))
    {
        return MIN_POWER_LIMIT;
    }

    auto area = getArea(inputTemp);
    if (area != nullptr)
    {
        return area->getPowerLimit();
    }

    return MIN_POWER_LIMIT;
}

PowerArea *PowerConfig::getArea(float temperature)
{
    if (isnanf(temperature))
    {
        return nullptr;
    }

    for (size_t i = 0; i < POWER_AREA_AMOUNT; i++)
    {
        auto area = getArea(i);
        if (area->isResponsable(temperature))
        {
            return area;
        }
    }

    return nullptr;
}

/*
##############################################
##             PowerArea              ##
##############################################
*/

PowerArea::PowerArea(size_t index, PowerConfig *config, Preferences *preferences) : index(index), name("Area " + String(index + 1)), _config(config), _preferences(preferences)
{
    _enabled = preferences->getBool(KEY_SETTING_POWER_AREA_ENABLED + index, false);
    _start = preferences->getChar(KEY_SETTING_POWER_AREA_START + index, 0);
    _end = preferences->getChar(KEY_SETTING_POWER_AREA_END + index, 0);
    _powerLimit = preferences->getUChar(KEY_SETTING_POWER_AREA_LIMIT + index, MAX_POWER_LIMIT);
};

bool PowerArea::setEnabled(bool enabled)
{
    if (enabled == _enabled)
    {
        return _enabled;
    }

    _enabled = enabled;
    _preferences->putBool(KEY_SETTING_POWER_AREA_ENABLED + index, _enabled);
    return _enabled;
}

int8_t PowerArea::setStart(int8_t start)
{
    start = min(max(start, (int8_t)MIN_TEMPERATURE), (int8_t)MAX_TEMPERATURE);
    if (start == _start)
    {
        return _start;
    }

    _start = start;
    _preferences->putChar(KEY_SETTING_POWER_AREA_START + index, _start);
    return _start;
}

int8_t PowerArea::setEnd(int8_t end)
{
    end = min(max(end, (int8_t)MIN_TEMPERATURE), (int8_t)MAX_TEMPERATURE);
    if (end == _end)
    {
        return _end;
    }

    _end = end;
    _preferences->putChar(KEY_SETTING_POWER_AREA_END + index, _end);
    return _end;
}

uint8_t PowerArea::setPowerLimit(uint8_t powerLimit)
{
    powerLimit = min(max(powerLimit, (uint8_t)MIN_POWER_LIMIT), (uint8_t)MAX_POWER_LIMIT);
    if (powerLimit == _powerLimit)
    {
        return _powerLimit;
    }

    _powerLimit = powerLimit;
    _preferences->putChar(KEY_SETTING_POWER_AREA_LIMIT + index, _powerLimit);
    return _powerLimit;
}

bool PowerArea::isResponsable(float temperature)
{
    return !isnanf(temperature) && _enabled && isValid() && temperature >= _start && temperature <= _end;
}

bool PowerArea::isValid()
{
    return _start <= _end && _end >= _start;
}