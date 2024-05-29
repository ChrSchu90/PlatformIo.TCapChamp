#pragma once

#include <Preferences.h>

static const size_t TEMP_AREA_AMOUNT = 6;
static const char *KEY_SETING_MANUAL_MODE = "ManualMode";        // Preferences key for manual mode (limited to 15 chars)
static const char *KEY_SETING_MANUAL_TEMP = "ManualTemp";        // Preferences key for manual temperature (limited to 15 chars)
static const char *KEY_SETING_TEMP_AREA_ENABLED = "TempEnabled"; // Preferences key area enabled/disabled NOTE: WITHOUT INDEX
static const char *KEY_SETING_TEMP_AREA_START = "TempStart";     // Preferences key area start temperature NOTE: WITHOUT INDEX
static const char *KEY_SETING_TEMP_AREA_END = "TempEnd";         // Preferences key area end temperature NOTE: WITHOUT INDEX
static const char *KEY_SETING_TEMP_AREA_OFFSET = "TempOffset";   // Preferences key area temperature offset NOTE: WITHOUT INDEX

// Pre-define class
class TemperatureArea;

/// @brief Holds the temperature configuration
class TemperatureConfig
{
private:
    TemperatureArea *_areas[TEMP_AREA_AMOUNT];
    Preferences *_preferences;
    bool _manualMode;
    int _manualTemperature;

protected:
public:
    /// @brief Creates a new instance of an TemperatureConfig
    /// @param preferences the app preferences to stroe the configuration
    TemperatureConfig(Preferences *preferences);

    /// @brief Gets if the manual mode is active
    bool isManualMode() { return _manualMode; };

    /// @brief Sets if the manual mode is active
    /// @param manualMode manual mode enabled/disabled
    void setManualMode(bool manualMode);

    /// @brief Gets the manual temperature
    int getManualTemperature() { return _manualTemperature; };

    /// @brief Sets if the manual temperature
    /// @param manualTemperature manual temperature
    void setManualTemperature(int manualTemperature);

    /// @brief Gets the temperature areas
    /// @param index area index
    TemperatureArea *getArea(size_t index) { return _areas[index]; };

    /// @brief Gets the area that is responsable for the given temperature
    /// @param temperature the temperature
    /// @return the responsable area or nullptr if none could be found
    TemperatureArea *getArea(float temperature);

    /// @brief Get the output temperature based on the input and configuration
    /// @param inputTemp the raw temperature that should be modified based on the configuration
    /// @return the output temperature or NAN if it couldn't be calculated
    float getOutputTemperature(float inputTemp);
};

/// @brief Holds the temperature area configuration
class TemperatureArea
{
private:
    TemperatureConfig *_config;
    Preferences *_preferences;
    bool _enabled;
    int _start;
    int _end;
    int _offset;

protected:
public:
    const size_t index;
    const String name;

    /// @brief Creates a new instance of an TemperatureArea
    /// @param index unique area index
    /// @param config the parent configuration
    /// @param preferences the app preferences to stroe the configuration
    TemperatureArea(size_t index, TemperatureConfig *config, Preferences *preferences);

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
    void setEnabled(bool enabled);

    /// @brief Gets the start temperature of the area
    int getStart() { return _start; };

    /// @brief Sets the start temperature of the area
    /// @param start defines the start temperature of the area
    void setStart(int start);

    /// @brief Gets the end temperature of the area
    int getEnd() { return _end; };

    /// @brief Sets the start temperature of the area
    /// @param end defines the end temperature of the area
    void setEnd(int end);

    /// @brief Gets the temperature offset of the area
    int getOffset() { return _offset; };

    /// @brief Sets the temperature offset of the area
    /// @param end defines the temperature offset of the area
    void setOffset(int offset);
};
