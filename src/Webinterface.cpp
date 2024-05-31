#include <Arduino.h>
#include <WiFiManager.h>
#include <ESPUI.h>
#include "Webinterface.h"

/*
##############################################
##               Webinterface               ##
##############################################
*/

/// @brief Creates the web UI instance
/// @param config the configuration
/// @param wifiManager the WiFi manager
Webinterface::Webinterface(Config *config, WiFiManager *wifiManager)
{
    // Create global labels
    _lblSensorTemp = ESPUI.addControl(Label, "Sensor", String(NAN) + " °C", None);
    _lblWeatherTemp = ESPUI.addControl(Label, "Weather", String(NAN) + " °C", None);
    _lblOutputTemp = ESPUI.addControl(Label, "Output", String(NAN) + " °C", None);

    // Create tabs
    temperatureTab = new TemperatureTab(config->temperatureConfig);
    systemInfoTab = new SystemInfoTab();
    wifiInfoTab = new WifiInfoTab(wifiManager);

    // Start ESP UI https://github.com/s00500/ESPUI
    ESPUI.begin("T-Cap Champ");
};

/// @brief Updates the sensor temperature inside webinterface
/// @param temperature the new temperature
void Webinterface::setSensorTemp(const float temperature)
{
    ESPUI.updateLabel(_lblSensorTemp, String(temperature) + " °C");
}

/// @brief Updates the weather API temperature inside webinterface
/// @param temperature the new temperature
void Webinterface::setWeatherTemp(const float temperature)
{
    ESPUI.updateLabel(_lblWeatherTemp, String(temperature) + " °C");
}

/// @brief Updates the output temperature inside webinterface
/// @param temperature the new temperature
void Webinterface::setOutputTemp(const float temperature)
{
    ESPUI.updateLabel(_lblOutputTemp, String(temperature) + " °C");
}

/*
##############################################
##              SystemInfoTab               ##
##############################################
*/

/// @brief Creates an instance of the SystemInfoTab
SystemInfoTab::SystemInfoTab()
{
    _tab = ESPUI.addControl(
        Tab, "System", "System", None, None,
        [](Control *sender, int type, void *UserInfo)
        {
            // Update on open tab
            SystemInfoTab *instance = static_cast<SystemInfoTab *>(UserInfo);
            instance->update();
            instance->_rebootCnt = REBOOT_CLICK_CNT;
            ESPUI.updateButton(instance->_btnReboot, "Press " + String(instance->_rebootCnt) + " times");
        },
        this);

    _heapSize = ESP.getHeapSize();
    _flashSize = ESP.getFlashChipSize();
    _sketchSize = ESP.getFreeSketchSpace() + ESP.getSketchSize();

    _lblUptime = ESPUI.addControl(Label, "Performance", "", None, _tab);
    _lblHeapUsage = ESPUI.addControl(Label, "Heap Usage", "", None, _lblUptime);
    _lblHeapAllocatedMax = ESPUI.addControl(Label, "Heap Allocated Max", "", None, _lblUptime);
    _lblSketch = ESPUI.addControl(Label, "Sketch Used", "", None, _lblUptime);
    _lblTemperature = ESPUI.addControl(Label, "Temperature", "", None, _lblUptime);

    auto lblDevice = ESPUI.addControl(Label, "Device", "Model: " + String(ESP.getChipModel()), None, _tab);
    ESPUI.addControl(Label, "Flash", "Flash: " + String(_flashSize), None, lblDevice);
    ESPUI.addControl(Label, "Freq", "Freq: " + String(ESP.getCpuFreqMHz()) + " MHz", None, lblDevice);
    ESPUI.addControl(Label, "Cores", "Cores: " + String(ESP.getChipCores()), None, lblDevice);
    ESPUI.addControl(Label, "Revision", "Revision: " + String(ESP.getChipRevision()), None, lblDevice);

    auto lblSoftware = ESPUI.addControl(Label, "Software", "Arduiono Verison: " + String(VER_ARDUINO_STR), None, _tab);
    ESPUI.addControl(Label, "SDK", "SDK Verison: " + String(ESP.getSdkVersion()), None, lblSoftware);
    ESPUI.addControl(Label, "Build", "Build: " + String(__DATE__ " " __TIME__), None, lblSoftware);

    _btnReboot = ESPUI.addControl(
        ControlType::Button, "Reboot", "Press " + String(_rebootCnt) + " times", Carrot, _tab,
        [](Control *sender, int type, void *UserInfo)
        {
            if (type != B_DOWN)
            {
                return;
            }

            SystemInfoTab *instance = static_cast<SystemInfoTab *>(UserInfo);
            instance->_rebootCnt--;
            if (instance->_rebootCnt < 1)
            {
                ESP.restart();
            }

            ESPUI.updateButton(sender->id, "Press " + String(instance->_rebootCnt) + " times");
        },
        this);

    update();
}

/// @brief Update the system information
void SystemInfoTab::update()
{
    auto currentMillis = esp_timer_get_time() / 1000;
    auto seconds = currentMillis / 1000;
    auto minutes = seconds / 60;
    auto hours = minutes / 60;
    auto days = hours / 24;
    currentMillis %= 1000;
    seconds %= 60;
    minutes %= 60;
    hours %= 24;
    ESPUI.updateLabel(_lblUptime, "Uptime: " + String(days) + "d " + String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s");

    float freeHeap = ESP.getFreeHeap();
    float usedHeap = _heapSize - freeHeap;
    ESPUI.updateLabel(_lblHeapUsage, "Heap Usage: " + String(usedHeap, 0) + "/" + String(_heapSize) + " (" + String(usedHeap / _heapSize * 100.0f, 2) + " %)");
    float maxUsedHeap = ESP.getMaxAllocHeap();
    ESPUI.updateLabel(_lblHeapAllocatedMax, "Heap Allocated Max: " + String(maxUsedHeap, 0) + " (" + String(maxUsedHeap / _heapSize * 100.0f, 2) + " %)");

    float freeSketch = ESP.getFreeSketchSpace();
    ESPUI.updateLabel(_lblSketch, "Sketch Used: " + String(_sketchSize - freeSketch, 0) + "/" + String(_sketchSize) + " (" + String(freeSketch / _sketchSize * 100.0f, 2) + " %)");

    ESPUI.updateLabel(_lblTemperature, "Temperature: " + String(temperatureRead()) + " °C");
}

/*
##############################################
##           TemperatureAreaTab             ##
##############################################
*/

/// @brief Creates a new tab for the temperature area configuration inside the Web interface
/// @param tab the parent temperature tab
/// @param config the temperature area configuration
TemperatureAreaTab::TemperatureAreaTab(TemperatureTab *tab, TemperatureArea *config) : _tab(tab), _config(config)
{
    // ESPUI.addControl(Separator, "Areas", "", None, tab->getTabId());

    _swEnabled = ESPUI.addControl(
        Switcher, config->name.c_str(), String(config->isEnabled() ? 1 : 0), None, tab->getTabId(),
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureAreaTab *instance = static_cast<TemperatureAreaTab *>(UserInfo);
            bool newValue = sender->value.toInt() > 0;
            instance->_config->setEnabled(newValue);
            ESPUI.updateSwitcher(sender->id, newValue);
            instance->updateStatus();
        },
        this);

    // Empty label for spaceing
    ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, _swEnabled), "background-color: unset; width: 100%;");

    ESPUI.setElementStyle(ESPUI.addControl(Label, "", "Start °C", None, _swEnabled), "background-color: unset; width: 100%;");
    _sldStart = ESPUI.addControl(
        Slider, "Start", String(config->getStart()), None, _swEnabled,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureAreaTab *instance = static_cast<TemperatureAreaTab *>(UserInfo);
            int newValue = sender->value.toInt();
            instance->_config->setStart(newValue);
            ESPUI.updateSlider(sender->id, newValue);
            instance->updateStatus();
        },
        this);
    ESPUI.addControl(Min, "", "-20", None, _sldStart);
    ESPUI.addControl(Max, "", "30", None, _sldStart);
    ESPUI.addControl(Step, "", "1", None, _sldStart);

    ESPUI.setElementStyle(ESPUI.addControl(Label, "", "End °C", None, _swEnabled), "background-color: unset; width: 100%;");
    _sldEnd = ESPUI.addControl(
        Slider, "End", String(config->getEnd()), None, _swEnabled,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureAreaTab *instance = static_cast<TemperatureAreaTab *>(UserInfo);
            int newValue = sender->value.toInt();
            instance->_config->setEnd(newValue);
            ESPUI.updateSlider(sender->id, newValue);
            instance->updateStatus();
        },
        this);
    ESPUI.addControl(Min, "", "-20", None, _sldEnd);
    ESPUI.addControl(Max, "", "30", None, _sldEnd);
    ESPUI.addControl(Step, "", "1", None, _sldEnd);

    ESPUI.setElementStyle(ESPUI.addControl(Label, "", "Offset °C", None, _swEnabled), "background-color: unset; width: 100%;");
    _sldOffset = ESPUI.addControl(
        Slider, "Offset", String(config->getOffset()), None, _swEnabled,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureAreaTab *instance = static_cast<TemperatureAreaTab *>(UserInfo);
            int newValue = sender->value.toInt();
            instance->_config->setOffset(newValue);
            ESPUI.updateSlider(sender->id, newValue);
        },
        this);
    ESPUI.addControl(Min, "", "-20", None, _sldOffset);
    ESPUI.addControl(Max, "", "20", None, _sldOffset);
    ESPUI.addControl(Step, "", "1", None, _sldOffset);

    updateStatus();
}

/// @brief Forces an update of the UI values
void TemperatureAreaTab::update()
{
    ESPUI.updateSwitcher(_swEnabled, _config->isEnabled());
    ESPUI.updateSlider(_sldStart, _config->getStart());
    ESPUI.updateSlider(_sldEnd, _config->getEnd());
    ESPUI.updateSlider(_sldOffset, _config->getOffset());

    updateStatus();
}

/// @brief Foreces an update of the status indicator to make a invalid configuration visible
void TemperatureAreaTab::updateStatus()
{
    ESPUI.setEnabled(_sldStart, _config->isEnabled());
    ESPUI.setEnabled(_sldEnd, _config->isEnabled());
    ESPUI.setEnabled(_sldOffset, _config->isEnabled());

    if (_config->isEnabled() && !_config->isValid())
    {
        ESPUI.setPanelStyle(_swEnabled, "background-color: #E74C3C;"); // red
    }
    else
    {
        ESPUI.setPanelStyle(_swEnabled, "background-color: #444857;"); // none
    }
}

/*
##############################################
##             TemperatureTab               ##
##############################################
*/

/// @brief Creates a new tab for the temperature configuration inside the Web interface
/// @param config the temperature configuration
TemperatureTab::TemperatureTab(TemperatureConfig *config) : _config(config)
{
    _tab = ESPUI.addControl(Tab, "Temperature", "Temperature");
    _swManualMode = ESPUI.addControl(
        Switcher, "Manual Temperature", String(config->isManualMode() ? 1 : 0), None, _tab,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureTab *instance = static_cast<TemperatureTab *>(UserInfo);
            bool newValue = sender->value.toInt() > 0;
            instance->_config->setManualMode(newValue);
            ESPUI.updateSwitcher(sender->id, newValue);
            instance->updateStatus();
        },
        this);

    _sldManualTemp = ESPUI.addControl(
        Slider, "Temperature", String(config->getManualTemperature()), None, _swManualMode,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureTab *instance = static_cast<TemperatureTab *>(UserInfo);
            int newValue = sender->value.toInt();
            instance->_config->setManualTemperature(newValue);
            ESPUI.updateSlider(sender->id, newValue);
        },
        this);
    ESPUI.addControl(Min, "", "-20", None, _sldManualTemp);
    ESPUI.addControl(Max, "", "30", None, _sldManualTemp);
    ESPUI.addControl(Step, "", "1", None, _sldManualTemp);

    for (size_t i = 0; i < TEMP_AREA_AMOUNT; i++)
    {
        _areas[i] = new TemperatureAreaTab(this, config->getArea(i));
    }

    updateStatus();
}

/// @brief Forces an update of the UI values
void TemperatureTab::update()
{
    ESPUI.updateSwitcher(_swManualMode, _config->isManualMode());
    ESPUI.updateSlider(_sldManualTemp, _config->getManualTemperature());
    for (size_t i = 0; i < TEMP_AREA_AMOUNT; i++)
    {
        _areas[i]->update();
    }

    updateStatus();
}

/// @brief Foreces an update of the status indicator to make a invalid configuration visible
void TemperatureTab::updateStatus()
{
    ESPUI.setEnabled(_sldManualTemp, _config->isManualMode());
}

/*
##############################################
##               WifiInfoTab                ##
##############################################
*/

/// @brief Creates an instance of the WifiInfo
WifiInfoTab::WifiInfoTab(WiFiManager *wifiManager) : _wifiManager(wifiManager)
{
    _tab = ESPUI.addControl(
        Tab, "WiFi", "WiFi", None, None,
        [](Control *sender, int type, void *UserInfo)
        {
            // Update on open tab
            WifiInfoTab *instance = static_cast<WifiInfoTab *>(UserInfo);
            instance->update();
            instance->_resetCnt = RESET_CLICK_CNT;
            ESPUI.updateButton(instance->_btnReset, "Press " + String(instance->_resetCnt) + " times");
        },
        this);

    _lblConfiguration = ESPUI.addControl(Label, "Configuration", "Hostname: " + String(WiFi.getHostname()), None, _tab);
    _lblMac = ESPUI.addControl(Label, "MAC", "", None, _lblConfiguration);
    _lblIp = ESPUI.addControl(Label, "IP", "", None, _lblConfiguration);
    _lblDns = ESPUI.addControl(Label, "DNS", "", None, _lblConfiguration);
    _lblGateway = ESPUI.addControl(Label, "Gateway", "", None, _lblConfiguration);
    _lblSubnet = ESPUI.addControl(Label, "Subnet", "", None, _lblConfiguration);
    _lblSsid = ESPUI.addControl(Label, "SSID", "", None, _lblConfiguration);
    _lblRssi = ESPUI.addControl(Label, "RSSI", "", None, _lblConfiguration);

    _btnReset = ESPUI.addControl(
        ControlType::Button, "Reset", "Press " + String(_resetCnt) + " times", Carrot, _tab,
        [](Control *sender, int type, void *UserInfo)
        {
            if (type != B_DOWN)
            {
                return;
            }

            WifiInfoTab *instance = static_cast<WifiInfoTab *>(UserInfo);
            instance->_resetCnt--;
            if (instance->_resetCnt < 1)
            {
                instance->_wifiManager->resetSettings();
                ESP.restart();
            }

            ESPUI.updateButton(sender->id, "Press " + String(instance->_resetCnt) + " times");
        },
        this);

    update();
}

/// @brief Update the WiFi information
void WifiInfoTab::update()
{
    ESPUI.updateLabel(_lblMac, "MAC: " + String(WiFi.macAddress()));
    ESPUI.updateLabel(_lblIp, "IP: " + String(WiFi.localIP().toString()));
    ESPUI.updateLabel(_lblDns, "DNS: " + String(WiFi.dnsIP().toString()));
    ESPUI.updateLabel(_lblGateway, "Gateway: " + String(WiFi.gatewayIP().toString()));
    ESPUI.updateLabel(_lblSubnet, "Subnet: " + String(WiFi.subnetMask().toString()));
    ESPUI.updateLabel(_lblSsid, "SSID: " + String(WiFi.SSID()));

    auto rssi = WiFi.RSSI();
    ESPUI.updateLabel(_lblRssi, "RSSI: " + String(rssi) + " db (" + String(min(max(2 * (rssi + 100), 0), 100)) + " %)");
}