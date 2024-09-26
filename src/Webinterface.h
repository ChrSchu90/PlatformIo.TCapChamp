#pragma once

#include "Config.h"

#define STYLE_HIDDEN "background-color: unset; width: 0px; height: 0px; display: none;"
#define STYLE_NUM_TEMP_ADJUST_NORMAL "width: 15%; color: black; background: rgba(255,255,255,0.8);"
#define STYLE_SWITCH_POWER_ADJUST "vertical-align: middle; margin-right: 25px; margin-bottom: 3px; width: 12.5%;"
#define STYLE_NUM_POWER_ADJUST_NORMAL "width: 15%; color: black; background: rgba(255,255,255,0.8);"
#define STYLE_NUM_POWER_ADJUST_ERROR "width: 15%; color: black; background: rgba(231,76,60,0.8);"
#define STYLE_NUM_POWER_ADJUST_DISABLED "width: 15%; color: black; background: rgba(153,153,153,0.8);"
#define STYLE_LBL_ADJUST "background-color: unset; width: 85%; text-align-last: left;"
#define STYLE_LBL_INOUT "width: 15%;"
#define STYLE_SWITCH_INOUT "vertical-align: middle; margin-right: 25px; margin-bottom: 3px; width: 12.5%;"
#define STYLE_NUM_INOUT_MANUAL_INPUT "vertical-align: middle; margin-bottom: 3px; width: 15%; color: black; background: rgba(255,255,255,0.8);"
#define STYLE_LBL_INOUT_VALUE_OUTPUT "background-color: unset; text-align: left; width: 80%;"
#define STYLE_SWITCH_INOUT_MANUAL_ENABLE "background-color: unset; text-align: left; width: 62.5%;" 

static const char * NO_VALUE = "";                                      // Empty string for no value
static const String MAX_TEMPERATURE_VALUE = String(MAX_TEMPERATURE);    // Maximum supported temperature value
static const String MIN_TEMPERATURE_VALUE = String(MIN_TEMPERATURE);    // Minimum supported temperature value
static const String MAX_POWER_LIMIT_VALUE = String(MAX_POWER_LIMIT);    // Maximum supported power limit value
static const String MIN_POWER_LIMIT_VALUE = String(MIN_POWER_LIMIT);    // Minimum supported power limit valie
static const String STEP_POWER_LIMIT_VALUE = String("5");               // Slider step power limit value

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

/// @brief Web UI temperature element that represents a power limit area
class PowerAreaTab
{
private:
    PowerArea *_config;
    uint16_t _swEnabled;
    uint16_t _numStart;
    uint16_t _numEnd;
    uint16_t _numLimit;

protected:
public:
    /// @brief Creates a area for the power limit configuration inside the Web interface
    /// @param groupCtlId the parent control group ID
    /// @param config the power limit area configuration
    PowerAreaTab(const uint16_t groupCtlId, PowerArea *config);

    /// @brief Forces an update of the UI values
    void update();

    /// @brief Foreces an update of the status indicator to make a invalid configuration visible
    void updateStatus();
};

/// @brief Adjustements tab to define the manipulation of output temperature and power limit
class AdjustmentTab 
{
private:
    Config *_config;
    uint16_t _tab;

protected:
public:
    /// @brief Creates the adjustment tab instance
    /// @param config the configuration
    /// @param wifiManager the WiFi manager
    AdjustmentTab(Config *config);
};

/// @brief Web UI
class Webinterface
{
private:
    Config *_config;
    uint16_t _lblSensorTemp;
    uint16_t _lblWeatherTemp;
    uint16_t _swManualTempInput;
    uint16_t _swManualTempOutput;
    uint16_t _swManualPowerOutput;
    uint16_t _numManualTempInput;
    uint16_t _numManualTempOutput;
    uint16_t _numManualPowerOutput;
    uint16_t _lblTempOutput;
    uint16_t _lblPowerOutput;
    AdjustmentTab *_adjustmentTab;
    SystemInfoTab *_systemInfoTab;
    WifiInfoTab *_wifiInfoTab;

protected:
public:

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