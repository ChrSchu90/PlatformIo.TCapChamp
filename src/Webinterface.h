#pragma once

#include "Config.h"

static const char * NO_VALUE = "";                                      // Empty string for no value
static const String MAX_TEMPERATURE_VALUE = String(MAX_TEMPERATURE);    // Maximum supported temperature value
static const String MIN_TEMPERATURE_VALUE = String(MIN_TEMPERATURE);    // Minimum supported temperature value
static const String MAX_POWER_LIMIT_VALUE = String(MAX_POWER_LIMIT);    // Maximum supported power limit value
static const String MIN_POWER_LIMIT_VALUE = String(MIN_POWER_LIMIT);    // Minimum supported power limit valie
static const String STEP_POWER_LIMIT_VALUE = String("5");               // Slider step power limit value

class TemperatureAreaTab;
class PowerAreaTab;

/// @brief Implements a system information tab inside the Webinterface
class SystemInfoTab
{
private:
    static const int REBOOT_CLICK_CNT = 5;
    uint16_t _tab;
    uint16_t _btnReboot;
    uint16_t _lblUptime;
    uint16_t _lblHeapUsage;
    uint16_t _lblHeapAllocatedMax;
    uint16_t _lblSketch;
    uint16_t _lblTemperature;
    uint32_t _flashSize;
    uint32_t _heapSize;
    uint32_t _sketchSize;
    int _rebootCnt = REBOOT_CLICK_CNT;

protected:
public:
    /// @brief Creates an instance of the SystemInfoTab
    SystemInfoTab();

    /// @brief Update the system information
    void update();
};

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

/// @brief Web UI tab to interact with the Power Configuration
class PowerTab
{
private:
    PowerAreaTab *_areas[POWER_AREA_AMOUNT];
    PowerConfig *_config;
    uint16_t _tab;
    uint16_t _swManualMode;
    uint16_t _sldManualPower;

protected:
public:
    /// @brief Creates a new tab for the power configuration inside the Web interface
    /// @param config the power configuration
    PowerTab(PowerConfig *config);

    /// @brief Gets the ID of the UI Tab
    uint16_t getTabId() { return _tab; };

    /// @brief Forces an update of the UI values
    void update();

    /// @brief Foreces an update of the status indicator to make a invalid configuration visible
    void updateStatus();
};

/// @brief Web UI temperature element that represents a power limit area
class PowerAreaTab
{
private:
    PowerTab *_tab;
    PowerArea *_config;
    uint16_t _swEnabled;
    uint16_t _sldStart;
    uint16_t _sldEnd;
    uint16_t _sldPowerLimit;

protected:
public:
    /// @brief Creates a area for the power limit configuration inside the Web interface
    /// @param tab the parent power limit tab
    /// @param config the power limit area configuration
    PowerAreaTab(PowerTab *tab, PowerArea *config);

    /// @brief Forces an update of the UI values
    void update();

    /// @brief Foreces an update of the status indicator to make a invalid configuration visible
    void updateStatus();
};

/// @brief Holds a available WiFi option
struct WiFiOption 
{
    String Label = emptyString;
    String Value = emptyString;
    uint16_t Control = 0;
};

/// @brief Implements a WiFi information tab inside the Webinterface
class WifiInfoTab
{
private:
    static const int16_t AmountWiFiOptions = 8;
    uint16_t _tab;
    uint16_t _lblInfo;
    uint16_t _lblMac;
    uint16_t _lblIp;
    uint16_t _lblDns;
    uint16_t _lblGateway;
    uint16_t _lblSubnet;
    uint16_t _lblSsid;
    uint16_t _lblRssi;
    uint16_t _selSsid;
    uint16_t _txtSsid;
    uint16_t _txtPassword;
    uint16_t _btnSave;
    uint16_t _btnScan;
    WiFiOption _wifiOptions[AmountWiFiOptions];

    void updateBtnSaveState();
    void wifiScanCompleted(int16_t networkCnt);
    
protected:
public:
    /// @brief Creates an instance of the TabWifiInfo
    WifiInfoTab();

    /// @brief Update the WiFi information
    void update();
};

/// @brief Web UI
class Webinterface
{
private:
    uint16_t _lblSensorTemp;
    uint16_t _lblWeatherTemp;
    uint16_t _lblOutputTemp;
    uint16_t _lblOutputPower;

protected:
public:
    TemperatureTab *temperatureTab;
    PowerTab *powerTab;
    SystemInfoTab *systemInfoTab;
    WifiInfoTab *wifiInfoTab;

    /// @brief Creates the web UI instance
    /// @param port the webinterface port
    /// @param config the configuration
    /// @param wifiManager the WiFi manager
    Webinterface(const uint16_t port, Config *config);

    /// @brief Updates the sensor temperature inside webinterface
    /// @param temperature the new temperature
    void setSensorTemp(const float temperature);

    /// @brief Updates the weather API temperature inside webinterface
    /// @param temperature the new temperature
    void setWeatherTemp(const float temperature);

    /// @brief Updates the output temperature inside webinterface
    /// @param temperature the new temperature
    void setOutputTemp(const float temperature);

    /// @brief Updates the output power limit inside webinterface
    /// @param powerLimit the new power limit [%]
    void setOuputPowerLimit(const float powerLimit);
};