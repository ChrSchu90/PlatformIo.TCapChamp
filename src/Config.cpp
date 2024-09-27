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
    // Check limit and round to 0.1
    _manualOutputActive = preferences->getBool(KEY_SETTING_TEMP_MANUAL_OUT_MODE, false);
    _manualOutputTemperature = preferences->getFloat(KEY_SETTING_TEMP_MANUAL_OUT_TEMP, 15.0f);

    _manualInputActive = preferences->getBool(KEY_SETTING_TEMP_MANUAL_IN_MODE, false);
    _manualInputTemperature = preferences->getFloat(KEY_SETTING_TEMP_MANUAL_IN_TEMP, 10.0f);

    for (size_t i = 0; i < TEMP_ADJUST_AMOUNT; i++)
    {
        _adjustments[i] = new TemperatureAdjustment(ADJUST_TEMP_START - i, preferences);
    }
};

bool TemperatureConfig::setManualOutputActive(bool manualMode)
{
    if (_manualOutputActive == manualMode)
    {
        return _manualOutputActive;
    }

    _manualOutputActive = manualMode;
    _preferences->putBool(KEY_SETTING_TEMP_MANUAL_OUT_MODE, _manualOutputActive);
    return _manualOutputActive;
}

bool TemperatureConfig::setManualInputActive(bool manualMode)
{
    if (_manualInputActive == manualMode)
    {
        return _manualInputActive;
    }

    _manualInputActive = manualMode;
    _preferences->putBool(KEY_SETTING_TEMP_MANUAL_IN_MODE, _manualInputActive);
    return _manualInputActive;
}

float TemperatureConfig::setManualOutputTemperature(float manualTemperature)
{
    // Check limit and round to 0.1
    manualTemperature = min(max(manualTemperature, (float)MIN_TEMPERATURE), (float)MAX_TEMPERATURE);
    manualTemperature = roundf(manualTemperature * 10) / 10;
    
    if (_manualOutputTemperature == manualTemperature)
    {
        return _manualOutputTemperature;
    }

    _manualOutputTemperature = manualTemperature;
    _preferences->putFloat(KEY_SETTING_TEMP_MANUAL_OUT_TEMP, _manualOutputTemperature);
    return _manualOutputTemperature;
}

float TemperatureConfig::setManualInputTemperature(float manualTemperature)
{
    // Check limit and round to 0.1
    manualTemperature = min(max(manualTemperature, (float)MIN_TEMPERATURE), (float)MAX_TEMPERATURE);
    manualTemperature = roundf(manualTemperature * 10) / 10;

    if (_manualInputTemperature == manualTemperature)
    {
        return _manualInputTemperature;
    }

    _manualInputTemperature = manualTemperature;
    _preferences->putFloat(KEY_SETTING_TEMP_MANUAL_IN_TEMP, _manualInputTemperature);
    return _manualInputTemperature;
}

float TemperatureConfig::getOutputTemperature(float inputTemp)
{
    if (_manualOutputActive)
    {
        return _manualOutputTemperature;
    }

    if (isnanf(inputTemp))
    {
        return inputTemp;
    }

    if(inputTemp > ADJUST_TEMP_START)
    {
        return ADJUST_TEMP_START;
    }

    if(inputTemp < ADJUST_TEMP_END)
    {
        return ADJUST_TEMP_END;
    }

    int8_t upperIn = ceil(inputTemp); 
    int8_t lowerIn = floor(inputTemp);
    if(upperIn == lowerIn)
    {
        auto adj = getTempAdjustment(upperIn);
        if(adj != nullptr)
        {
            return adj->getTemperatureAdjusted();
        }
    }

    auto adjUpper = getTempAdjustment(upperIn);
    auto adjLower = getTempAdjustment(lowerIn);
    if(adjUpper != nullptr && adjLower != nullptr)
    {
        auto upperOut = adjUpper->getTemperatureAdjusted();
        auto lowerOut = adjLower->getTemperatureAdjusted();

        // Linear interpolation
        return lowerOut + (upperOut - lowerOut) / (upperIn - lowerIn) * (inputTemp - lowerIn);
    }

    return inputTemp;
}

TemperatureAdjustment *TemperatureConfig::getTempAdjustment(int8_t tempReal)
{
    for (size_t i = 0; i < TEMP_ADJUST_AMOUNT; i++)
    {
        auto adjustment = getAdjustment(i);
        if(adjustment->tempReal == tempReal)
        {
            return adjustment;
        }
    }

    return nullptr;
}

/*
##############################################
##          TemperatureAdjustment           ##
##############################################
*/

TemperatureAdjustment::TemperatureAdjustment(int8_t tempReal, Preferences *preferences) : _preferences(preferences), tempReal(tempReal)
{
    _tempAdjusted = preferences->getChar(KEY_SETTING_TEMP_ADJUST_TEMP + tempReal, tempReal);
};

int8_t TemperatureAdjustment::setTemperatureAdjusted(int8_t temperature)
{
    temperature = min(max(temperature, (int8_t)(tempReal - ADJUST_TEMP_MAX_OFSET)), (int8_t)(tempReal + ADJUST_TEMP_MAX_OFSET));
    temperature = min(max(temperature, (int8_t)MIN_TEMPERATURE), (int8_t)MAX_TEMPERATURE);

    if (temperature == _tempAdjusted)
    {
        return _tempAdjusted;
    }

    _tempAdjusted = temperature;
    _preferences->putChar(KEY_SETTING_TEMP_ADJUST_TEMP + tempReal, _tempAdjusted);
    return _tempAdjusted;
};

/*
##############################################
##            PowerConfig                   ##
##############################################
*/

PowerConfig::PowerConfig(Preferences *preferences) : _preferences(preferences)
{
    _manualOutputActive = preferences->getBool(KEY_SETTING_POWER_MANUAL_MODE, false);
    _manualPower = preferences->getUChar(KEY_SETTING_POWER_MANUAL_POWER, 100);

    for (size_t i = 0; i < POWER_AREA_AMOUNT; i++)
    {
        _areas[i] = new PowerArea(i, this, preferences);
    }
};

bool PowerConfig::setManualOutputActive(bool manualMode)
{
    if (_manualOutputActive == manualMode)
    {
        return _manualOutputActive;
    }

    _manualOutputActive = manualMode;
    _preferences->putBool(KEY_SETTING_POWER_MANUAL_MODE, _manualOutputActive);
    return _manualOutputActive;
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
    if (_manualOutputActive)
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
##             PowerArea                    ##
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
    powerLimit = ((uint8_t)round((powerLimit + (uint8_t)(STEP_POWER_LIMIT / 2)) / STEP_POWER_LIMIT)) * STEP_POWER_LIMIT;
    powerLimit = min(max(powerLimit, (uint8_t)MIN_POWER_LIMIT), (uint8_t)MAX_POWER_LIMIT);
    if (powerLimit == _powerLimit)
    {
        return _powerLimit;
    }

    _powerLimit = powerLimit;
    _preferences->putUChar(KEY_SETTING_POWER_AREA_LIMIT + index, _powerLimit);
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