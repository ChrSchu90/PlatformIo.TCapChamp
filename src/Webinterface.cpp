#define LOG_LEVEL NONE
// #define DEBUG_ESPUI 1

#include <Arduino.h>
#include <ESPUI.h>
#include <dataIndexHTML.h>
#include <Update.h>
#include <esp_arduino_version.h>
#include "Webinterface.h"
#include "WiFiModeChamp.h"
#include "SerialLogging.h"

/*
##############################################
##               Webinterface               ##
##############################################
*/

Webinterface::Webinterface(uint16_t port, Config *config) : _config(config)
{
#ifdef LOG_DEBUG
    ESPUI.setVerbosity(Verbosity::Verbose);
#endif
    
    // Input Temperature Group
    auto inputTempGrp = ESPUI.addControl(ControlType::Label, "Input Temperature", NO_VALUE, ControlColor::None);
    ESPUI.setElementStyle(inputTempGrp, STYLE_HIDDEN);

    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, "Sensor", ControlColor::None, inputTempGrp), STYLE_LBL_INOUT_VALUE_OUTPUT);
    _lblSensorTemp = ESPUI.addControl(ControlType::Label, NO_VALUE, String(NAN) + " °C", ControlColor::None, inputTempGrp);
    ESPUI.setElementStyle(_lblSensorTemp, STYLE_LBL_INOUT);

    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, "Weather API", ControlColor::None, inputTempGrp), STYLE_LBL_INOUT_VALUE_OUTPUT);
    _lblWeatherTemp = ESPUI.addControl(ControlType::Label, "Weather API", String(NAN) + " °C", ControlColor::None, inputTempGrp);
    ESPUI.setElementStyle(_lblWeatherTemp, STYLE_LBL_INOUT);

    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, "Manual", ControlColor::None, inputTempGrp), STYLE_SWITCH_INOUT_MANUAL_ENABLE);
    _swManualTempInput = ESPUI.addControl(
        ControlType::Switcher, NO_VALUE, String(config->temperatureConfig->isManualInputTemp() ? 1 : 0), ControlColor::None, inputTempGrp,
        [](Control *sender, int type, void *UserInfo)
        {
            Webinterface *instance = static_cast<Webinterface *>(UserInfo);
            ESPUI.updateSwitcher(sender->id, instance->_config->temperatureConfig->setManualInputActive(sender->value.toInt() > 0));
            //instance->updateStatus(); // TODO: show enable disable manual input temp?
        },
        this);
    ESPUI.setElementStyle(_swManualTempInput, STYLE_SWITCH_INOUT);
    _numManualTempInput = ESPUI.addControl(
        ControlType::Number, NO_VALUE, String(config->temperatureConfig->getManualInputTemperature(), 1), ControlColor::None, inputTempGrp,
        [](Control *sender, int type, void *UserInfo)
        {
            Webinterface *instance = static_cast<Webinterface *>(UserInfo);
            if(sender->value.isEmpty())
                ESPUI.updateControlValue(sender->id, String(instance->_config->temperatureConfig->getManualInputTemperature(), 1));
            else
                ESPUI.updateControlValue(sender->id, String(instance->_config->temperatureConfig->setManualInputTemperature(sender->value.toFloat()), 1));
            ESPUI.setElementStyle(sender->id, STYLE_NUM_INOUT_MANUAL_INPUT);
        },
        this);
    ESPUI.setElementStyle(_numManualTempInput, STYLE_NUM_INOUT_MANUAL_INPUT);
    ESPUI.addControl(ControlType::Step, NO_VALUE, "0.1", ControlColor::None, _numManualTempInput);



    // Output Temperature Group
    auto outputTempGrp = ESPUI.addControl(ControlType::Label, "Output Temperature", NO_VALUE, ControlColor::None);
    ESPUI.setElementStyle(outputTempGrp, STYLE_HIDDEN);

    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, "Actual", ControlColor::None, outputTempGrp), STYLE_LBL_INOUT_VALUE_OUTPUT);
    _lblTempOutput = ESPUI.addControl(ControlType::Label, NO_VALUE, String(NAN) + " °C", ControlColor::None, outputTempGrp);
    ESPUI.setElementStyle(_lblTempOutput, STYLE_LBL_INOUT);

    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, "Manual", ControlColor::None, outputTempGrp), STYLE_SWITCH_INOUT_MANUAL_ENABLE);
    _swManualTempOutput = ESPUI.addControl(
        ControlType::Switcher, NO_VALUE, String(config->temperatureConfig->isManualOutputTemp() ? 1 : 0), ControlColor::None, outputTempGrp,
        [](Control *sender, int type, void *UserInfo)
        {
            Webinterface *instance = static_cast<Webinterface *>(UserInfo);
            ESPUI.updateSwitcher(sender->id, instance->_config->temperatureConfig->setManualOutputActive(sender->value.toInt() > 0));
            //instance->updateStatus(); // TODO: show enable disable manual output temp?
        },
        this);
    ESPUI.setElementStyle(_swManualTempOutput, STYLE_SWITCH_INOUT);
    _numManualTempOutput = ESPUI.addControl(
        ControlType::Number, NO_VALUE, String(config->temperatureConfig->getManualOutputTemperature(), 1), ControlColor::None, outputTempGrp,
        [](Control *sender, int type, void *UserInfo)
        {
            Webinterface *instance = static_cast<Webinterface *>(UserInfo);
            if(sender->value.isEmpty())
                ESPUI.updateControlValue(sender->id, String(instance->_config->temperatureConfig->getManualOutputTemperature(), 1));
            else
                ESPUI.updateControlValue(sender->id, String(instance->_config->temperatureConfig->setManualOutputTemperature(sender->value.toFloat()), 1));
            ESPUI.setElementStyle(sender->id, STYLE_NUM_INOUT_MANUAL_INPUT);
        },
        this);
    ESPUI.setElementStyle(_numManualTempOutput, STYLE_NUM_INOUT_MANUAL_INPUT);
    ESPUI.addControl(ControlType::Step, NO_VALUE, "0.1", ControlColor::None, _numManualTempOutput);



    // Output Power Group
    auto outputPowerGrp = ESPUI.addControl(ControlType::Label, "Output Power Limit", NO_VALUE, ControlColor::None);
    ESPUI.setElementStyle(outputPowerGrp, STYLE_HIDDEN);

    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, "Actual", ControlColor::None, outputPowerGrp), STYLE_LBL_INOUT_VALUE_OUTPUT);
    _lblPowerOutput = ESPUI.addControl(ControlType::Label, NO_VALUE, "Inactive", ControlColor::None, outputPowerGrp);
    ESPUI.setElementStyle(_lblPowerOutput, STYLE_LBL_INOUT);

    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, "Manual", ControlColor::None, outputPowerGrp), STYLE_SWITCH_INOUT_MANUAL_ENABLE);
    _swManualPowerOutput = ESPUI.addControl(
        ControlType::Switcher, NO_VALUE, String(config->powerConfig->isManualOutputPower() ? 1 : 0), ControlColor::None, outputPowerGrp,
        [](Control *sender, int type, void *UserInfo)
        {
            Webinterface *instance = static_cast<Webinterface *>(UserInfo);
            ESPUI.updateSwitcher(sender->id, instance->_config->powerConfig->setManualOutputActive(sender->value.toInt() > 0));
            //instance->updateStatus(); // TODO: show enable disable manual output power?
        },
        this);
    ESPUI.setElementStyle(_swManualPowerOutput, STYLE_SWITCH_INOUT);
    _numManualPowerOutput = ESPUI.addControl(
        ControlType::Number, NO_VALUE, String(config->powerConfig->getManualPower()), ControlColor::None, outputPowerGrp,
        [](Control *sender, int type, void *UserInfo)
        {
            Webinterface *instance = static_cast<Webinterface *>(UserInfo);
            if(sender->value.isEmpty())
                ESPUI.updateNumber(sender->id, instance->_config->powerConfig->getManualPower());
            else
                ESPUI.updateNumber(sender->id, instance->_config->powerConfig->setManualPower(sender->value.toInt()));
            ESPUI.setElementStyle(sender->id, STYLE_NUM_INOUT_MANUAL_INPUT);
        },
        this);
    ESPUI.setElementStyle(_numManualPowerOutput, STYLE_NUM_INOUT_MANUAL_INPUT);
    ESPUI.addControl(ControlType::Step, NO_VALUE, String(STEP_POWER_LIMIT), ControlColor::None, _numManualPowerOutput);
    

    // Create tabs
    _adjustmentTab = new AdjustmentTab(config);
    _systemInfoTab = new SystemInfoTab();
    _wifiInfoTab = new WifiInfoTab();

    // Start ESP UI https://github.com/s00500/ESPUI
    // ESPUI.prepareFileSystem();  //Copy across current version of ESPUI resources
    // ESPUI.list(); //List all files on LittleFS, for info
    // ESPUI.jsonInitialDocumentSize = 12000;
    // ESPUI.jsonUpdateDocumentSize = 4000;
    ESPUI.captivePortal = true;
    ESPUI.begin("T-Cap Champ", nullptr, nullptr, port);

    // WiFi captive portal for iOS
    //ESPUI.WebServer()->on(
    ESPUI.server->on(
        "/hotspot-detect.html", HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", HTML_INDEX);
            request->send(response);
        });

    // NOTE: Control is added inside SystemTab! The callback needs to be added after the webserver has been started.
    //ESPUI.WebServer()->on(
    ESPUI.server->on(
        "/ota", HTTP_POST, [](AsyncWebServerRequest *request) { request->send(200); },
        [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
        {
            if (!index)
            {
                Serial.printf("UploadStart: %s\n", filename.c_str());

                // calculate sketch space required for the update, for ESP32 use the max constant
                if (!Update.begin(UPDATE_SIZE_UNKNOWN))
                {
                    // start with max available size
                    Update.printError(Serial);
                }
            }

            if (len)
            {
                Update.write(data, len);
            }

            // if the final flag is set then this is the last frame of data
            if (final)
            {
                request->redirect("/");
                if (Update.end(true))
                {
                    // true to set the size to the current progress
                    Serial.printf("Update Success: %ub written\nRebooting...\n", index + len);
                    ESP.restart();
                }
                else
                {
                    Update.printError(Serial);
                }
            }
        });
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
    ESPUI.updateLabel(_lblTempOutput, String(temperature) + " °C");
}

void Webinterface::setOuputPowerLimit(const float powerLimit)
{
    if (powerLimit < 10)
    {
        ESPUI.updateLabel(_lblPowerOutput, "Inactive");
    }
    else
    {
        ESPUI.updateLabel(_lblPowerOutput, String(powerLimit, 0) + " \%");
    }
}

/*
##############################################
##              SystemInfoTab               ##
##############################################
*/

SystemInfoTab::SystemInfoTab()
{
    _tab = ESPUI.addControl(
        ControlType::Tab, NO_VALUE, "System", ControlColor::None, Control::noParent,
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

    _lblUptime = ESPUI.addControl(ControlType::Label, "Performance", NO_VALUE, ControlColor::None, _tab);
    _lblHeapUsage = ESPUI.addControl(ControlType::Label, "Heap Usage", NO_VALUE, ControlColor::None, _lblUptime);
    _lblHeapAllocatedMax = ESPUI.addControl(ControlType::Label, "Heap Allocated Max", NO_VALUE, ControlColor::None, _lblUptime);
    _lblSketch = ESPUI.addControl(ControlType::Label, "Sketch Used", NO_VALUE, ControlColor::None, _lblUptime);
    _lblTemperature = ESPUI.addControl(ControlType::Label, "Temperature", NO_VALUE, ControlColor::None, _lblUptime);

    auto lblDevice = ESPUI.addControl(ControlType::Label, "Device", "Model: " + String(ESP.getChipModel()), ControlColor::None, _tab);
    ESPUI.addControl(ControlType::Label, "Flash", "Flash: " + String(_flashSize), ControlColor::None, lblDevice);
    ESPUI.addControl(ControlType::Label, "Freq", "Freq: " + String(ESP.getCpuFreqMHz()) + " MHz", ControlColor::None, lblDevice);
    ESPUI.addControl(ControlType::Label, "Cores", "Cores: " + String(ESP.getChipCores()), ControlColor::None, lblDevice);
    ESPUI.addControl(ControlType::Label, "Revision", "Revision: " + String(ESP.getChipRevision()), ControlColor::None, lblDevice);

    auto lblSoftware = ESPUI.addControl(ControlType::Label, "Software", "Arduiono Verison: " + String(ESP_ARDUINO_VERSION_MAJOR) + "." + String(ESP_ARDUINO_VERSION_MINOR) + "." + String(ESP_ARDUINO_VERSION_PATCH), ControlColor::None, _tab);
    ESPUI.addControl(ControlType::Label, "SDK", "SDK Verison: " + String(ESP.getSdkVersion()), ControlColor::None, lblSoftware);
    ESPUI.addControl(ControlType::Label, "Build", "Build: " + String(__DATE__ " " __TIME__), ControlColor::None, lblSoftware);

    auto lblOTA = ESPUI.addControl(ControlType::Label, "OTA Update", "<form method=""POST"" action=""/ota"" enctype=""multipart/form-data""><input type=""file"" name=""data"" style=""color:white;background-color:#999"" /><input type=""submit"" name=""upload"" value=""Upload"" title=""Upload Files"" style=""color:white;background-color:#999;width:100px;height:35px""></form>", ControlColor::None, _tab);
    ESPUI.setElementStyle(lblOTA, "background-color: transparent; width: 100%;");

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
##             AdjustmentTab                ##
##############################################
*/

AdjustmentTab::AdjustmentTab(Config *config) : _config(config)
{
    _tab = ESPUI.addControl(Tab, NO_VALUE, "Adjustments");

    auto tmpAdjGrp = ESPUI.addControl(ControlType::Label, "Temperature Adjustment", NO_VALUE, ControlColor::None, _tab);
    ESPUI.setElementStyle(tmpAdjGrp, STYLE_HIDDEN);
    for (size_t i = 0; i < TEMP_ADJUST_AMOUNT; i++)
    {
        auto adj = config->temperatureConfig->getAdjustment(i);
        auto numAdjTemp = ESPUI.addControl(
             Number, NO_VALUE, String(adj->getTemperatureAdjusted()), ControlColor::None, tmpAdjGrp,
             [](Control *sender, int type, void *UserInfo)
             {
                TemperatureAdjustment *instance = static_cast<TemperatureAdjustment *>(UserInfo);
                if(sender->value.isEmpty())
                    ESPUI.updateNumber(sender->id, instance->getTemperatureAdjusted());
                else
                    ESPUI.updateNumber(sender->id, instance->setTemperatureAdjusted(sender->value.toInt()));
                ESPUI.setElementStyle(sender->id, STYLE_NUM_TEMP_ADJUST_NORMAL);
             },
             adj);
        
        ESPUI.setElementStyle(numAdjTemp, STYLE_NUM_TEMP_ADJUST_NORMAL);
    
        auto lblCtl = ESPUI.addControl(ControlType::Label, NO_VALUE, "°C  at " + String(adj->tempReal) + " °C", ControlColor::None, tmpAdjGrp);
        ESPUI.setElementStyle(lblCtl, STYLE_LBL_ADJUST);
    }


    auto pwrAdjGrp = ESPUI.addControl(ControlType::Label, "Power Adjustment", NO_VALUE, ControlColor::None, _tab);
    ESPUI.setElementStyle(pwrAdjGrp, STYLE_HIDDEN);
    for (size_t i = 0; i < POWER_AREA_AMOUNT; i++)
    {
        new PowerAreaTab(pwrAdjGrp, config->powerConfig->getArea(i));
    }
}

/*
##############################################
##                PowerArea                 ##
##############################################
*/

PowerAreaTab::PowerAreaTab(const uint16_t groupCtlId, PowerArea *config) : _config(config)
{
    _swEnabled = ESPUI.addControl(
        ControlType::Switcher, config->name.c_str(), String(config->isEnabled() ? 1 : 0), ControlColor::None, groupCtlId,
        [](Control *sender, int type, void *UserInfo)
        {
            PowerAreaTab *instance = static_cast<PowerAreaTab *>(UserInfo);
            ESPUI.updateSwitcher(sender->id, instance->_config->setEnabled(sender->value.toInt() > 0));
            instance->updateStatus();
        },
        this);
    ESPUI.setElementStyle(_swEnabled, STYLE_SWITCH_POWER_ADJUST);
    
    _numEnd = ESPUI.addControl(
        ControlType::Number, NO_VALUE, String(config->getEnd()), ControlColor::None, groupCtlId,
        [](Control *sender, int type, void *UserInfo)
        {
            PowerAreaTab *instance = static_cast<PowerAreaTab *>(UserInfo);
            if(sender->value.isEmpty())
                ESPUI.updateNumber(sender->id, instance->_config->getEnd());
            else
                ESPUI.updateNumber(sender->id, instance->_config->setEnd(sender->value.toInt()));
            instance->updateStatus();
        },
        this);
    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, "°C  - ", ControlColor::None, groupCtlId), "background-color: unset; text-align: left; width: 10%;");
        
    _numStart = ESPUI.addControl(
        ControlType::Number, NO_VALUE, String(config->getStart()), ControlColor::None, groupCtlId,
        [](Control *sender, int type, void *UserInfo)
        {
            PowerAreaTab *instance = static_cast<PowerAreaTab *>(UserInfo);
            if(sender->value.isEmpty())
                ESPUI.updateNumber(sender->id, instance->_config->getStart());
            else
                ESPUI.updateNumber(sender->id, instance->_config->setStart(sender->value.toInt()));
            instance->updateStatus();
        },
        this);
    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, "°C   ", ControlColor::None, groupCtlId), "background-color: unset; text-align: left; width: 10%;");
    
    _numLimit = ESPUI.addControl(
        ControlType::Number, NO_VALUE, String(config->getPowerLimit()), ControlColor::None, groupCtlId,
        [](Control *sender, int type, void *UserInfo)
        {
            PowerAreaTab *instance = static_cast<PowerAreaTab *>(UserInfo);
            if(sender->value.isEmpty())
                ESPUI.updateNumber(sender->id, instance->_config->getPowerLimit());
            else
                ESPUI.updateNumber(sender->id, instance->_config->setPowerLimit(sender->value.toInt()));
            instance->updateStatus();
        },
        this);
    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, "\%", ControlColor::None, groupCtlId), "background-color: unset; text-align: left; width: 15%;");
    ESPUI.addControl(ControlType::Step, NO_VALUE, String(STEP_POWER_LIMIT), ControlColor::None, _numLimit);

    updateStatus();
}

void PowerAreaTab::update()
{
    ESPUI.updateSwitcher(_swEnabled, _config->isEnabled());
    ESPUI.updateNumber(_numStart, _config->getStart());
    ESPUI.updateNumber(_numEnd, _config->getEnd());
    ESPUI.updateNumber(_numLimit, _config->getPowerLimit());
    updateStatus();
}

void PowerAreaTab::updateStatus()
{
    auto inputStyle = !_config->isEnabled() ? STYLE_NUM_POWER_ADJUST_DISABLED : _config->isValid() ? STYLE_NUM_POWER_ADJUST_NORMAL : STYLE_NUM_POWER_ADJUST_ERROR;
    ESPUI.setElementStyle(_numStart, inputStyle);
    ESPUI.setElementStyle(_numEnd, inputStyle);
    ESPUI.setElementStyle(_numLimit, inputStyle);
}

/*
##############################################
##               WifiInfoTab                ##
##############################################
*/

WifiInfoTab::WifiInfoTab()
{
    _tab = ESPUI.addControl(
        ControlType::Tab, NO_VALUE, "WiFi", ControlColor::None, Control::noParent,
        [](Control *sender, int type, void *UserInfo)
        {
            // Update on open tab
            WifiInfoTab *instance = static_cast<WifiInfoTab *>(UserInfo);
            instance->update();
            ESPUI.updateText(instance->_txtPassword, WifiModeChamp.getConfiguredWiFiPassword());
            ESPUI.updateText(instance->_txtSsid, WifiModeChamp.getConfiguredWiFiSSID());
            instance->updateBtnSaveState();
        },
        this);

    _lblInfo = ESPUI.addControl(ControlType::Label, "Info", "Hostname: " + String(WiFi.getHostname()), ControlColor::None, _tab);
    _lblMac = ESPUI.addControl(ControlType::Label, "MAC", NO_VALUE, ControlColor::None, _lblInfo);
    _lblIp = ESPUI.addControl(ControlType::Label, "IP", NO_VALUE, ControlColor::None, _lblInfo);
    _lblDns = ESPUI.addControl(ControlType::Label, "DNS", NO_VALUE, ControlColor::None, _lblInfo);
    _lblGateway = ESPUI.addControl(ControlType::Label, "Gateway", NO_VALUE, ControlColor::None, _lblInfo);
    _lblSubnet = ESPUI.addControl(ControlType::Label, "Subnet", NO_VALUE, ControlColor::None, _lblInfo);
    _lblSsid = ESPUI.addControl(ControlType::Label, "SSID", NO_VALUE, ControlColor::None, _lblInfo);
    _lblRssi = ESPUI.addControl(ControlType::Label, "RSSI", NO_VALUE, ControlColor::None, _lblInfo);

    auto lblWifiSettings = ESPUI.addControl(ControlType::Label, "WiFi Configuration", NO_VALUE, ControlColor::None, _tab);
    ESPUI.setElementStyle(lblWifiSettings, STYLE_HIDDEN);
    _selSsid = ESPUI.addControl(
        ControlType::Select, NO_VALUE, WifiModeChamp.getConfiguredWiFiSSID(), ControlColor::None, lblWifiSettings,
        [](Control *sender, int type, void *UserInfo)
        {
            WifiInfoTab *instance = static_cast<WifiInfoTab *>(UserInfo);
            if (!sender->value.isEmpty())
            {
                ESPUI.updateText(instance->_txtSsid, sender->value);
            }

            instance->updateBtnSaveState();
        },
        this);

    ESPUI.setEnabled(_selSsid, false);
    if(WifiModeChamp.getConfiguredWiFiSSID().isEmpty())
    {
        ESPUI.addControl(ControlType::Option, emptyString.c_str(), emptyString, ControlColor::None, _selSsid);
    }    

    _txtSsid = ESPUI.addControl(
        ControlType::Text, NO_VALUE, WifiModeChamp.getConfiguredWiFiSSID(), ControlColor::None, lblWifiSettings,
        [](Control *sender, int type, void *UserInfo)
        {
            WifiInfoTab *instance = static_cast<WifiInfoTab *>(UserInfo);
            instance->updateBtnSaveState();
        },
        this);

    _txtPassword = ESPUI.addControl(
        ControlType::Password, NO_VALUE, WifiModeChamp.getConfiguredWiFiPassword(), ControlColor::None, lblWifiSettings,
        [](Control *sender, int type, void *UserInfo)
        {
            WifiInfoTab *instance = static_cast<WifiInfoTab *>(UserInfo);
            instance->updateBtnSaveState();
        },
        this);

    _btnSave = ESPUI.addControl(
        ControlType::Button, NO_VALUE, "Save & Connect", ControlColor::None, lblWifiSettings,
        [](Control *sender, int type, void *UserInfo)
        {
            if (type != B_DOWN)
            {
                return;
            }

            WifiInfoTab *instance = static_cast<WifiInfoTab *>(UserInfo);
            WifiModeChamp.setWifiCredentials(ESPUI.getControl(instance -> _txtSsid)->value, ESPUI.getControl(instance -> _txtPassword)->value, true);
        },
        this);

    _btnScan = ESPUI.addControl(
        ControlType::Button, NO_VALUE, "Search for WiFi", ControlColor::None, lblWifiSettings,
        [](Control *sender, int type, void *UserInfo)
        {
            if (type != B_DOWN)
            {
                return;
            }

            WifiInfoTab *instance = static_cast<WifiInfoTab *>(UserInfo);
            WifiModeChamp.scanWifiNetworks();
            ESPUI.setEnabled(sender->id, false);
        },
        this);

    WifiModeChamp.scanCallback(std::bind(&WifiInfoTab::wifiScanCompleted, this, std::placeholders::_1));
    update();
    updateBtnSaveState();
}

void WifiInfoTab::wifiScanCompleted(int16_t networkCnt)
{
    ESPUI.setEnabled(_btnScan, true);
    if (networkCnt < 1)
    {
        return;
    }

    ESPUI.setEnabled(_selSsid, true);

    // For AP mode only add new WiFis, since captive portals might not support page reload
    if(WifiModeChamp.getMode() == WifiModeChampMode::AP)
    {
        for (int16_t n = 0; n < networkCnt; ++n)
        {
            auto ssid = WiFi.SSID(n);
            auto found = false;
            for (int16_t o = 0; o < AmountWiFiOptions; ++o)
            {
                if(_wifiOptions[o].Control < 1)
                {
                    // Slot is free and can't contain any WiFi
                    break;
                }

                if(_wifiOptions[o].Value == ssid)
                {
                    // WiFi already exists
                    found = true;
                    break;
                }
            }

            if(!found)
            {
                for (int16_t o = 0; o < AmountWiFiOptions; ++o)
                {
                    if(_wifiOptions[o].Control > 0)
                    {
                        // Slot is in use
                        continue;
                    }

                    // Add new option to a free slot
                    _wifiOptions[o].Value = ssid;
                    auto rssi = WiFi.RSSI(n);
                    _wifiOptions[o].Label = String(rssi) + "db (" + String(WifiModeChampClass::wifiSignalQuality(rssi)) + "%) " + _wifiOptions[o].Value;
                    _wifiOptions[o].Control = ESPUI.addControl(ControlType::Option, _wifiOptions[o].Label.c_str(), _wifiOptions[o].Value, ControlColor::None, _selSsid);
                    break;
                }
            }
        }
    }
    else
    {
        // TODO: is there a better solution? On the other hand it will not be used very much...
        for (int16_t o = 0; o < AmountWiFiOptions; ++o)
        {
            // Remove old option
            if(_wifiOptions[o].Control > 0)
            {
                ESPUI.removeControl(_wifiOptions[o].Control, false);
                _wifiOptions[o].Control = 0;
                _wifiOptions[o].Label = emptyString;
                _wifiOptions[o].Value = emptyString;
            }            

            // Add new WiFi to option that has been found
            if (o < networkCnt)
            {
                _wifiOptions[o].Value = WiFi.SSID(o);
                auto rssi = WiFi.RSSI(o);
                _wifiOptions[o].Label = String(rssi) + "db (" + String(WifiModeChampClass::wifiSignalQuality(rssi)) + "%) " + _wifiOptions[o].Value;
                _wifiOptions[o].Control = ESPUI.addControl(ControlType::Option, _wifiOptions[o].Label.c_str(), _wifiOptions[o].Value, ControlColor::None, _selSsid);
            }
        }
        
        ESPUI.jsonReload();
    }
}

void WifiInfoTab::updateBtnSaveState()
{
    auto configuredSsid = WifiModeChamp.getConfiguredWiFiSSID();
    auto txtSsid = ESPUI.getControl(_txtSsid)->value;
    auto configuredPassword = WifiModeChamp.getConfiguredWiFiPassword();
    auto txtPassword = ESPUI.getControl(_txtPassword)->value;
    auto valid = txtPassword.isEmpty() || txtPassword.length() > 7;
    auto changePending = valid && (configuredSsid != txtSsid || configuredPassword != txtPassword);
    ESPUI.setEnabled(_btnSave, changePending);
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
    ESPUI.updateLabel(_lblRssi, "RSSI: " + String(rssi) + " db (" + String(WifiModeChampClass::wifiSignalQuality(rssi)) + " %)");
}