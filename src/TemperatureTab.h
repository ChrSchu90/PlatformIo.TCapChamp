#pragma once

#include <TemperatureConfig.h>

// Pre-define class
class TemperatureAreaTab;

/// @brief Web UI tab to interact with the Temperature Configuration
class TemperatureTab
{
private:
    TemperatureAreaTab *_areas[TEMP_AREA_AMOUNT];
    TemperatureConfig *_config;
    uint16_t _tab;
    uint16_t _swManualMode;
    uint16_t _sldManualTemp;

protected:
public:
    /// @brief Creates a new tab for the temperature configuration inside the Web interface
    /// @param config the temperature configuration
    TemperatureTab(TemperatureConfig *config);
    
    /// @brief Gets the ID of the UI Tab
    uint16_t getTabId() { return _tab; };

    /// @brief Forces an update of the UI values
    void update();

    /// @brief Foreces an update of the status indicator to make a invalid configuration visible
    void updateStatus();
};

/// @brief Web UI temperature element that represents a teperature area
class TemperatureAreaTab
{
private:    
    TemperatureTab *_tab;
    TemperatureArea *_config;
    uint16_t _swEnabled;
    uint16_t _sldStart;
    uint16_t _sldEnd;
    uint16_t _sldOffset;   

protected:
public:
    /// @brief Creates a new tab for the temperature area configuration inside the Web interface
    /// @param tab the parent temperature tab
    /// @param config the temperature area configuration
    TemperatureAreaTab(TemperatureTab *tab, TemperatureArea *config);

    /// @brief Forces an update of the UI values
    void update();

    /// @brief Foreces an update of the status indicator to make a invalid configuration visible
    void updateStatus();
};