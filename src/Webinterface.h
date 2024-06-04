#pragma once

#include <Config.h>
#include <WiFiManager.h>

// Pre-define class
class TemperatureAreaTab;

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

/// @brief Implements a WiFi information tab inside the Webinterface
class WifiInfoTab
{
private:
    static const int RESET_CLICK_CNT = 5;
    WiFiManager *_wifiManager;
    uint16_t _tab;
    uint16_t _btnReset;
    uint16_t _lblConfiguration;
    uint16_t _lblMac;
    uint16_t _lblIp;
    uint16_t _lblDns;
    uint16_t _lblGateway;
    uint16_t _lblSubnet;
    uint16_t _lblSsid;
    uint16_t _lblRssi;
    int _resetCnt = RESET_CLICK_CNT;

protected:
public:
    /// @brief Creates an instance of the TabWifiInfo
    WifiInfoTab(WiFiManager *wifiManager);

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

protected:
public:
    TemperatureTab *temperatureTab;
    SystemInfoTab *systemInfoTab;
    WifiInfoTab *wifiInfoTab;

    /// @brief Creates the web UI instance
    /// @param port the webinterface port
    /// @param config the configuration
    /// @param wifiManager the WiFi manager
    Webinterface(const uint16_t port, Config *config, WiFiManager *wifiManager);

    /// @brief Updates the sensor temperature inside webinterface
    /// @param temperature the new temperature
    void setSensorTemp(const float temperature);

    /// @brief Updates the weather API temperature inside webinterface
    /// @param temperature the new temperature
    void setWeatherTemp(const float temperature);

    /// @brief Updates the output temperature inside webinterface
    /// @param temperature the new temperature
    void setOutputTemp(const float temperature);
};