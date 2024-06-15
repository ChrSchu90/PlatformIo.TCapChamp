#define LOG_LEVEL NONE
// #define DEBUG_ESPUI 1

#include <Arduino.h>
#include <WiFiManager.h>
#include <ESPUI.h>
#include "Webinterface.h"
#include "SerialLogging.h"

/*
##############################################
##               Webinterface               ##
##############################################
*/

Webinterface::Webinterface(uint16_t port, Config *config, WiFiManager *wifiManager)
{
#ifdef DEBUG_ESPUI
    ESPUI.setVerbosity(Verbosity::Verbose);
#endif
    // Create global labels
    _lblSensorTemp = ESPUI.addControl(Label, "Sensor", String(NAN) + " °C", None);
    _lblWeatherTemp = ESPUI.addControl(Label, "Weather API", String(NAN) + " °C", None);
    _lblOutputTemp = ESPUI.addControl(Label, "Output Temperature", String(NAN) + " °C", None);
    _lblOutputPower = ESPUI.addControl(Label, "Output Power Limit", String(NAN) + " \%", None);

    // Create tabs
    temperatureTab = new TemperatureTab(config->temperatureConfig);
    powerTab = new PowerTab(config->powerConfig);
    systemInfoTab = new SystemInfoTab();
    wifiInfoTab = new WifiInfoTab(wifiManager);

    // Start ESP UI https://github.com/s00500/ESPUI
    // ESPUI.prepareFileSystem();  //Copy across current version of ESPUI resources
    // ESPUI.list(); //List all files on LittleFS, for info
    // ESPUI.jsonInitialDocumentSize = 12000;
    // ESPUI.jsonUpdateDocumentSize = 4000;
    ESPUI.begin("T-Cap Champ", nullptr, nullptr, port);
};

void Webinterface::setSensorTemp(const float temperature)
{
    ESPUI.updateLabel(_lblSensorTemp, String(temperature) + " °C");
}

void Webinterface::setWeatherTemp(const float temperature)
{
    ESPUI.updateLabel(_lblWeatherTemp, String(temperature) + " °C");
}

void Webinterface::setOutputTemp(const float temperature)
{
    ESPUI.updateLabel(_lblOutputTemp, String(temperature) + " °C");
}

void Webinterface::setOuputPowerLimit(const uint8_t powerLimit)
{
    ESPUI.updateLabel(_lblOutputPower, String(powerLimit) + " \%");
}

/*
##############################################
##              SystemInfoTab               ##
##############################################
*/

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

    _lblUptime = ESPUI.addControl(Label, "Performance", NO_VALUE, None, _tab);
    _lblHeapUsage = ESPUI.addControl(Label, "Heap Usage", NO_VALUE, None, _lblUptime);
    _lblHeapAllocatedMax = ESPUI.addControl(Label, "Heap Allocated Max", NO_VALUE, None, _lblUptime);
    _lblSketch = ESPUI.addControl(Label, "Sketch Used", NO_VALUE, None, _lblUptime);
    _lblTemperature = ESPUI.addControl(Label, "Temperature", NO_VALUE, None, _lblUptime);

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

TemperatureAreaTab::TemperatureAreaTab(TemperatureTab *tab, TemperatureArea *config) : _tab(tab), _config(config)
{
    // ESPUI.addControl(Separator, "Areas", NO_VALUE, None, tab->getTabId());

    _swEnabled = ESPUI.addControl(
        Switcher, config->name.c_str(), String(config->isEnabled() ? 1 : 0), None, tab->getTabId(),
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureAreaTab *instance = static_cast<TemperatureAreaTab *>(UserInfo);
            ESPUI.updateSwitcher(sender->id, instance->_config->setEnabled(sender->value.toInt() > 0));
            instance->updateStatus();
        },
        this);

    // Empty label for spaceing
    ESPUI.setElementStyle(ESPUI.addControl(Label, NO_VALUE, NO_VALUE, None, _swEnabled), "background-color: unset; width: 100%;");

    ESPUI.setElementStyle(ESPUI.addControl(Label, NO_VALUE, "Start °C", None, _swEnabled), "background-color: unset; width: 100%;");
    _sldStart = ESPUI.addControl(
        Slider, "Start", String(config->getStart()), None, _swEnabled,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureAreaTab *instance = static_cast<TemperatureAreaTab *>(UserInfo);
            ESPUI.updateSlider(sender->id, instance->_config->setStart(sender->value.toInt()));
            instance->updateStatus();
        },
        this);        
    ESPUI.addControl(Min, NO_VALUE, MIN_TEMPERATURE_VALUE, None, _sldStart);
    ESPUI.addControl(Max, NO_VALUE, MAX_TEMPERATURE_VALUE, None, _sldStart);
    //ESPUI.addControl(Step, NO_VALUE, "1", None, _sldStart);

    ESPUI.setElementStyle(ESPUI.addControl(Label, NO_VALUE, "End °C", None, _swEnabled), "background-color: unset; width: 100%;");
    _sldEnd = ESPUI.addControl(
        Slider, "End", String(config->getEnd()), None, _swEnabled,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureAreaTab *instance = static_cast<TemperatureAreaTab *>(UserInfo);
            ESPUI.updateSlider(sender->id, instance->_config->setEnd(sender->value.toInt()));
            instance->updateStatus();
        },
        this);
    ESPUI.addControl(Min, NO_VALUE, MIN_TEMPERATURE_VALUE, None, _sldEnd);
    ESPUI.addControl(Max, NO_VALUE, MAX_TEMPERATURE_VALUE, None, _sldEnd);
    //ESPUI.addControl(Step, NO_VALUE, "1", None, _sldEnd);

    ESPUI.setElementStyle(ESPUI.addControl(Label, NO_VALUE, "Offset °C", None, _swEnabled), "background-color: unset; width: 100%;");
    _sldOffset = ESPUI.addControl(
        Slider, "Offset", String(config->getOffset()), None, _swEnabled,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureAreaTab *instance = static_cast<TemperatureAreaTab *>(UserInfo);
            ESPUI.updateSlider(sender->id, instance->_config->setOffset(sender->value.toInt()));
        },
        this);
    ESPUI.addControl(Min, NO_VALUE, MIN_TEMPERATURE_VALUE, None, _sldOffset);
    ESPUI.addControl(Max, NO_VALUE, MAX_TEMPERATURE_VALUE, None, _sldOffset);
    //ESPUI.addControl(Step, NO_VALUE, "1", None, _sldOffset);

    updateStatus();
}

void TemperatureAreaTab::update()
{
    ESPUI.updateSwitcher(_swEnabled, _config->isEnabled());
    ESPUI.updateSlider(_sldStart, _config->getStart());
    ESPUI.updateSlider(_sldEnd, _config->getEnd());
    ESPUI.updateSlider(_sldOffset, _config->getOffset());

    updateStatus();
}

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

TemperatureTab::TemperatureTab(TemperatureConfig *config) : _config(config)
{
    _tab = ESPUI.addControl(Tab, "Temperature", "Temperature");
    _swManualMode = ESPUI.addControl(
        Switcher, "Manual Temperature", String(config->isManualMode() ? 1 : 0), None, _tab,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureTab *instance = static_cast<TemperatureTab *>(UserInfo);
            ESPUI.updateSwitcher(sender->id, instance->_config->setManualMode(sender->value.toInt() > 0));
            instance->updateStatus();
        },
        this);

    _sldManualTemp = ESPUI.addControl(
        Slider, "Temperature", String(config->getManualTemperature()), None, _swManualMode,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureTab *instance = static_cast<TemperatureTab *>(UserInfo);
            ESPUI.updateSlider(sender->id, instance->_config->setManualTemperature(sender->value.toInt()));
        },
        this);
    ESPUI.addControl(Min, NO_VALUE, MIN_TEMPERATURE_VALUE, None, _sldManualTemp);
    ESPUI.addControl(Max, NO_VALUE, MAX_TEMPERATURE_VALUE, None, _sldManualTemp);
    //ESPUI.addControl(Step, NO_VALUE, "1", None, _sldManualTemp);

    ////
    //// TEST START
    ////

    // auto testSliderGroup = ESPUI.addControl(Label, "Temperature Adjustment", NO_VALUE, None, _tab);
    // ESPUI.setElementStyle(testSliderGroup, "background-color: unset; width: 0px; height: 0px; display: none;");
    // String inputStyle = "width: 30%;";
    // String labelStyle = "background-color: unset; width: 70%; text-align-last: left;";
    //// for (short i = 25; i >= -20; i--)
    // for (short i = 0; i >= 1; i--)
    //{
    //     auto inputCtl = ESPUI.addControl(
    //         Number, NO_VALUE, String(i), None, testSliderGroup,
    //         [](Control *sender, int type, void *UserInfo)
    //         {
    //             Serial.println("Value temperature changed to " + String(sender->value));
    //         },
    //         this);
    //     ESPUI.setElementStyle(inputCtl, inputStyle);
    //
    //    auto lblCtl = ESPUI.addControl(Label, NO_VALUE, "°C  at " + String(i) + " °C", None, testSliderGroup);
    //    ESPUI.setElementStyle(lblCtl, labelStyle);
    //}

    ////
    //// TEST END
    ////

    for (size_t i = 0; i < TEMP_AREA_AMOUNT; i++)
    {
        _areas[i] = new TemperatureAreaTab(this, config->getArea(i));
    }

    updateStatus();
}

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

void TemperatureTab::updateStatus()
{
    ESPUI.setEnabled(_sldManualTemp, _config->isManualMode());
}

/*
##############################################
##                PowerArea                 ##
##############################################
*/

PowerAreaTab::PowerAreaTab(PowerTab *tab, PowerArea *config) : _tab(tab), _config(config)
{
    // ESPUI.addControl(Separator, "Areas", NO_VALUE, None, tab->getTabId());

    _swEnabled = ESPUI.addControl(
        Switcher, config->name.c_str(), String(config->isEnabled() ? 1 : 0), None, tab->getTabId(),
        [](Control *sender, int type, void *UserInfo)
        {
            PowerAreaTab *instance = static_cast<PowerAreaTab *>(UserInfo);
            ESPUI.updateSwitcher(sender->id, instance->_config->setEnabled(sender->value.toInt() > 0));
            instance->updateStatus();
        },
        this);

    // Empty label for spaceing
    ESPUI.setElementStyle(ESPUI.addControl(Label, NO_VALUE, NO_VALUE, None, _swEnabled), "background-color: unset; width: 100%;");

    ESPUI.setElementStyle(ESPUI.addControl(Label, NO_VALUE, "Start °C", None, _swEnabled), "background-color: unset; width: 100%;");
    _sldStart = ESPUI.addControl(
        Slider, "Start", String(config->getStart()), None, _swEnabled,
        [](Control *sender, int type, void *UserInfo)
        {
            PowerAreaTab *instance = static_cast<PowerAreaTab *>(UserInfo);
            ESPUI.updateSlider(sender->id, instance->_config->setStart(sender->value.toInt()));
            instance->updateStatus();
        },
        this);
    ESPUI.addControl(Min, NO_VALUE, MIN_TEMPERATURE_VALUE, None, _sldStart);
    ESPUI.addControl(Max, NO_VALUE, MAX_TEMPERATURE_VALUE, None, _sldStart);
    // ESPUI.addControl(Step, NO_VALUE, "1", None, _sldStart);

    ESPUI.setElementStyle(ESPUI.addControl(Label, NO_VALUE, "End °C", None, _swEnabled), "background-color: unset; width: 100%;");
    _sldEnd = ESPUI.addControl(
        Slider, "End", String(config->getEnd()), None, _swEnabled,
        [](Control *sender, int type, void *UserInfo)
        {
            PowerAreaTab *instance = static_cast<PowerAreaTab *>(UserInfo);
            ESPUI.updateSlider(sender->id, instance->_config->setEnd(sender->value.toInt()));
            instance->updateStatus();
        },
        this);
    ESPUI.addControl(Min, NO_VALUE, MIN_TEMPERATURE_VALUE, None, _sldEnd);
    ESPUI.addControl(Max, NO_VALUE, MAX_TEMPERATURE_VALUE, None, _sldEnd);
    // ESPUI.addControl(Step, NO_VALUE, "1", None, _sldEnd);

    ESPUI.setElementStyle(ESPUI.addControl(Label, NO_VALUE, "Power Limit \%", None, _swEnabled), "background-color: unset; width: 100%;");
    _sldPowerLimit = ESPUI.addControl(
        Slider, "Limit", String(config->getPowerLimit()), None, _swEnabled,
        [](Control *sender, int type, void *UserInfo)
        {
            PowerAreaTab *instance = static_cast<PowerAreaTab *>(UserInfo);
            ESPUI.updateSlider(sender->id, instance->_config->setPowerLimit(sender->value.toInt()));
        },
        this);
    ESPUI.addControl(Min, NO_VALUE, MIN_POWER_LIMIT_VALUE, None, _sldPowerLimit);
    ESPUI.addControl(Max, NO_VALUE, MAX_POWER_LIMIT_VALUE, None, _sldPowerLimit);
    ESPUI.addControl(Step, NO_VALUE, STEP_POWER_LIMIT_VALUE, None, _sldPowerLimit);

    updateStatus();
}

void PowerAreaTab::update()
{
    ESPUI.updateSwitcher(_swEnabled, _config->isEnabled());
    ESPUI.updateSlider(_sldStart, _config->getStart());
    ESPUI.updateSlider(_sldEnd, _config->getEnd());
    ESPUI.updateSlider(_sldPowerLimit, _config->getPowerLimit());

    updateStatus();
}

void PowerAreaTab::updateStatus()
{
    ESPUI.setEnabled(_sldStart, _config->isEnabled());
    ESPUI.setEnabled(_sldEnd, _config->isEnabled());
    ESPUI.setEnabled(_sldPowerLimit, _config->isEnabled());

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
##                PowerTab                  ##
##############################################
*/

PowerTab::PowerTab(PowerConfig *config) : _config(config)
{
    _tab = ESPUI.addControl(Tab, "Power Limit", "Power");
    _swManualMode = ESPUI.addControl(
        Switcher, "Manual Power Limit", String(config->isManualMode() ? 1 : 0), None, _tab,
        [](Control *sender, int type, void *UserInfo)
        {
            PowerTab *instance = static_cast<PowerTab *>(UserInfo);
            ESPUI.updateSwitcher(sender->id, instance->_config->setManualMode(sender->value.toInt() > 0));
            instance->updateStatus();
        },
        this);

    _sldManualPower = ESPUI.addControl(
        Slider, "Power", String(config->getManualPower()), None, _swManualMode,
        [](Control *sender, int type, void *UserInfo)
        {
            PowerTab *instance = static_cast<PowerTab *>(UserInfo);
            ESPUI.updateSlider(sender->id, instance->_config->setManualPower(sender->value.toInt()));
        },
        this);
    ESPUI.addControl(Min, NO_VALUE, MIN_POWER_LIMIT_VALUE, None, _sldManualPower);
    ESPUI.addControl(Max, NO_VALUE, MAX_POWER_LIMIT_VALUE, None, _sldManualPower);
    ESPUI.addControl(Step, NO_VALUE, STEP_POWER_LIMIT_VALUE, None, _sldManualPower);

    ////
    //// TEST START
    ////

    // auto testSliderGroup = ESPUI.addControl(Label, "Power Adjustment", NO_VALUE, None, _tab);
    // ESPUI.setElementStyle(testSliderGroup, "background-color: unset; width: 0px; height: 0px; display: none;");
    // String inputStyle = "width: 30%;";
    // String labelStyle = "background-color: unset; width: 70%; text-align-last: left;";
    //// for (short i = 25; i >= -20; i--)
    // for (short i = 20; i >= -20; i--)
    //{
    //     auto inputCtl = ESPUI.addControl(
    //         Number, NO_VALUE, "100", None, testSliderGroup,
    //         [](Control *sender, int type, void *UserInfo)
    //         {
    //             Serial.println("Value power changed to " + String(sender->value));
    //             int newValue = sender->value.toInt();
    //             if (newValue > 100)
    //             {
    //                 ESPUI.updateNumber(sender->id, 100);
    //             }
    //
    //            if (newValue < 0)
    //            {
    //                ESPUI.updateNumber(sender->id, 0);
    //            }
    //        },
    //        this);
    //    ESPUI.setElementStyle(inputCtl, inputStyle);
    //
    //    auto lblCtl = ESPUI.addControl(Label, NO_VALUE, "\% at " + String(i) + " °C", None, testSliderGroup);
    //    ESPUI.setElementStyle(lblCtl, labelStyle);
    //}

    ////
    //// TEST END
    ////

    for (size_t i = 0; i < POWER_AREA_AMOUNT; i++)
    {
        _areas[i] = new PowerAreaTab(this, config->getArea(i));
    }

    updateStatus();
}

void PowerTab::update()
{
    ESPUI.updateSwitcher(_swManualMode, _config->isManualMode());
    ESPUI.updateSlider(_sldManualPower, _config->getManualPower());
    // for (size_t i = 0; i < TEMP_AREA_AMOUNT; i++)
    //{
    //     _areas[i]->update();
    // }

    updateStatus();
}

void PowerTab::updateStatus()
{
    ESPUI.setEnabled(_sldManualPower, _config->isManualMode());
}

/*
##############################################
##               WifiInfoTab                ##
##############################################
*/

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
    _lblMac = ESPUI.addControl(Label, "MAC", NO_VALUE, None, _lblConfiguration);
    _lblIp = ESPUI.addControl(Label, "IP", NO_VALUE, None, _lblConfiguration);
    _lblDns = ESPUI.addControl(Label, "DNS", NO_VALUE, None, _lblConfiguration);
    _lblGateway = ESPUI.addControl(Label, "Gateway", NO_VALUE, None, _lblConfiguration);
    _lblSubnet = ESPUI.addControl(Label, "Subnet", NO_VALUE, None, _lblConfiguration);
    _lblSsid = ESPUI.addControl(Label, "SSID", NO_VALUE, None, _lblConfiguration);
    _lblRssi = ESPUI.addControl(Label, "RSSI", NO_VALUE, None, _lblConfiguration);

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