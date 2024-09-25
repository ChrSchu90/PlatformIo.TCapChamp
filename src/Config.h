#pragma once

#include <Preferences.h>

#define MIN_TEMPERATURE -20                         // Minimum supported temperature in °C
#define MAX_TEMPERATURE 30                          // Maximum supported temperature in °C
#define ADJUST_TEMP_START 21                        // Start of adjustment temperatures in °C
#define ADJUST_TEMP_END -15                         // End of adjustment temperatures in °C
#define ADJUST_TEMP_MAX_OFSET 10                    // Maximum offset adjustment temperature in °C
#define KEY_SETTINGS_NAMESPACE "TCapChamp"          // Preferences namespance key (limited to 15 chars)
#define KEY_SETTING_TEMP_MANUAL_MODE "TempManual"   // Preferences key for manual temperature mode (limited to 15 chars)
#define KEY_SETTING_TEMP_MANUAL_TEMP "ManualTemp"   // Preferences key for manual temperature (limited to 15 chars)
#define KEY_SETTING_TEMP_ADJUST_TEMP "AdjustTemp"   // Preferences key for manual temperature NOTE: WITHOUT INDEX (limited to 15 chars)

#define MIN_POWER_LIMIT 0                             // Minimum supported power limit in %
#define MAX_POWER_LIMIT 100                           // Maximum supported power limit in %
#define STEP_POWER_LIMIT 5                            // Setps supported by the power limit in %
#define KEY_SETTING_POWER_MANUAL_MODE "PowerManual"   // Preferences key for manual power mode (limited to 15 chars)
#define KEY_SETTING_POWER_MANUAL_POWER "ManualPower"  // Preferences key for manual power (limited to 15 chars)
#define KEY_SETTING_POWER_AREA_ENABLED "PowerEnabled" // Preferences key area enabled/disabled NOTE: WITHOUT INDEX
#define KEY_SETTING_POWER_AREA_START "PowerStart"     // Preferences key area start temperature NOTE: WITHOUT INDEX
#define KEY_SETTING_POWER_AREA_END "PowerEnd"         // Preferences key area end temperature NOTE: WITHOUT INDEX
#define KEY_SETTING_POWER_AREA_LIMIT "PowerLimit"     // Preferences key area power limit NOTE: WITHOUT INDEX

static const size_t TEMP_ADJUST_AMOUNT = abs(ADJUST_TEMP_END - ADJUST_TEMP_START) + 1; // The amount of temperature adjustments based on ADJUST_TEMP_START and ADJUST_TEMP_END
static const size_t POWER_AREA_AMOUNT = 6;          // Amount of power areas

class TemperatureArea;
class PowerArea;

/// @brief Defines an adjustment for a specific temperature
class TemperatureAdjustment
{
private:
    Preferences *_preferences;    
    int8_t _tempAdjusted;       // Adjusted temperature

protected:
public:
    const int8_t tempReal;      // Real measured temperature

    /// @brief Creates a new instance of an TemperatureConfig
    /// @param preferences the app preferences to stroe the configuration
    TemperatureAdjustment(int8_t tempReal, Preferences *preferences);

    /// @brief Gets the adjusted temperature
    /// @return the saved adjusted temperature
    int8_t getTemperatureAdjusted() { return _tempAdjusted; }

    /// @brief Sets the adjusted temperature
    /// @param temperature the new adjusted temperature
    /// @return the saved adjusted temperature after limit checks
    int8_t setTemperatureAdjusted(int8_t temperature);
};

/// @brief Holds the temperature configuration
class TemperatureConfig
{
private:
    TemperatureAdjustment *_adjustments[TEMP_ADJUST_AMOUNT];
    Preferences *_preferences;
    bool _manualMode;
    int8_t _manualTemperature;

protected:
public:
    /// @brief Creates a new instance of an TemperatureConfig
    /// @param preferences the app preferences to stroe the configuration
    TemperatureConfig(Preferences *preferences);

    /// @brief Gets if the manual mode is active
    bool isManualMode() { return _manualMode; };

    /// @brief Sets if the manual mode is active
    /// @param manualMode manual mode enabled/disabled
    /// @return the new value after limit check
    bool setManualMode(bool manualMode);

    /// @brief Gets the manual temperature
    int8_t getManualTemperature() { return _manualTemperature; };

    /// @brief Sets if the manual temperature
    /// @param manualTemperature manual temperature
    /// @return the new value after limit check
    int8_t setManualTemperature(int8_t manualTemperature);

    /// @brief Gets the temperature adjustment by the index
    /// @param index adjustment index or nullptr if out of index
    TemperatureAdjustment *getAdjustment(size_t index) { return _adjustments[index]; };

    /// @brief Gets the temperature adjustment by the real temperature
    /// @param tempReal the real temperature or nullptr if out of index
    TemperatureAdjustment *getTempAdjustment(int8_t tempReal);

    /// @brief Get the output temperature based on the input and configuration
    /// @param inputTemp the raw temperature that should be modified based on the configuration
    /// @return the output temperature or NAN if it couldn't be calculated
    float getOutputTemperature(float inputTemp);
};

/// @brief Holds the power configuration
class PowerConfig
{
private:
    PowerArea *_areas[POWER_AREA_AMOUNT];
    Preferences *_preferences;
    bool _manualMode;
    uint8_t _manualPower;

protected:
public:
    /// @brief Creates a new instance of an PowerConfig
    /// @param preferences the app preferences to stroe the configuration
    PowerConfig(Preferences *preferences);

    /// @brief Gets if the manual mode is active
    bool isManualMode() { return _manualMode; };

    /// @brief Sets if the manual mode is active
    /// @param manualMode manual mode enabled/disabled
    /// @return the new value after limit check
    bool setManualMode(bool manualMode);

    /// @brief Gets the manual power [%]
    uint8_t getManualPower() { return _manualPower; };

    /// @brief Sets if the manual power [%]
    /// @param manualPower manual power [%]
    /// @return the new value after limit check
    uint8_t setManualPower(uint8_t manualPower);

    /// @brief Gets the power areas
    /// @param index area index
    PowerArea *getArea(size_t index) { return _areas[index]; };

    /// @brief Gets the area that is responsable for the given temperature
    /// @param temperature the temperature
    /// @return the responsable area or nullptr if none could be found
    PowerArea *getArea(float temperature);

    /// @brief Get the output power limit based on the input and configuration
    /// @param inputTemp the temperature that is used to search for the correct area
    /// @return the power limit in %
    uint8_t getOutputPowerLimit(float inputTemp);
};

/// @brief Holds the power area configuration
class PowerArea
{
private:
    PowerConfig *_config;
    Preferences *_preferences;
    bool _enabled;
    int8_t _start;
    int8_t _end;
    uint8_t _powerLimit;

protected:
public:
    const size_t index;
    const String name;

    /// @brief Creates a new instance of an PowerArea
    /// @param index unique area index
    /// @param config the parent configuration
    /// @param preferences the app preferences to store the configuration
    PowerArea(size_t index, PowerConfig *config, Preferences *preferences);

    /// @brief Gets if the area is responsable for the given temperature
    /// @param temperature the temperature
    /// @return if is responsable
    bool isResponsable(float temperature);

    /// @brief Gets if the area is enabled
    bool isEnabled() { return _enabled; };

    /// @brief Gets if the area configuiration is valid
    bool isValid();

    /// @brief Enables/disables the area
    /// @param enabled defines if the area is enabled
    /// @return the new value after limit check
    bool setEnabled(bool enabled);

    /// @brief Gets the start temperature of the area
    int8_t getStart() { return _start; };

    /// @brief Sets the start temperature of the area
    /// @param start defines the start temperature of the area
    /// @return the new value after limit check
    int8_t setStart(int8_t start);

    /// @brief Gets the end temperature of the area
    int8_t getEnd() { return _end; };

    /// @brief Sets the start temperature of the area
    /// @param end defines the end temperature of the area
    /// @return the new value after limit check
    int8_t setEnd(int8_t end);

    /// @brief Gets the powerlimit of the area
    uint8_t getPowerLimit() { return _powerLimit; }

    /// @brief Sets the powerlimit of the area
    /// @return the new value after limit check
    uint8_t setPowerLimit(uint8_t powerLimit);
};

/// @brief Holds the remanent settings
class Config
{
private:
    Preferences *_preferences;

protected:
public:
    TemperatureConfig *temperatureConfig;
    PowerConfig *powerConfig;

    /// @brief Creates the configuration instance
    Config();
};