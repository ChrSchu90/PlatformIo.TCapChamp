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
    auto inputTempGrp = ESPUI.addControl(ControlType::Label, "Input Temperature", emptyString, ControlColor::None);
    ESPUI.setElementStyle(inputTempGrp, STYLE_HIDDEN);

    _lblSensorTemp = ESPUI.addControl(ControlType::Label, emptyString.c_str(), String(NAN) + " °C", ControlColor::None, inputTempGrp);
    ESPUI.setElementStyle(_lblSensorTemp, STYLE_LBL_INOUT);
    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, emptyString.c_str(), "Sensor", ControlColor::None, inputTempGrp), STYLE_LBL_INOUT_VALUE_OUTPUT);

    _lblWeatherTemp = ESPUI.addControl(ControlType::Label, "Weather API", String(NAN) + " °C", ControlColor::None, inputTempGrp);
    ESPUI.setElementStyle(_lblWeatherTemp, STYLE_LBL_API);
    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, emptyString.c_str(), "Weather API", ControlColor::None, inputTempGrp), STYLE_LBL_API_VALUE_OUTPUT);
    
    _numManualTempInput = ESPUI.addControl(
        ControlType::Number, emptyString.c_str(), String(config->temperatureConfig->getManualInputTemperature(), 1), ControlColor::None, inputTempGrp,
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
    //ESPUI.addControl(ControlType::Step, emptyString, "0.1", ControlColor::None, _numManualTempInput);
    _swManualTempInput = ESPUI.addControl(
        ControlType::Switcher, emptyString.c_str(), String(config->temperatureConfig->isManualInputTemp() ? 1 : 0), ControlColor::None, inputTempGrp,
        [](Control *sender, int type, void *UserInfo)
        {
            Webinterface *instance = static_cast<Webinterface *>(UserInfo);
            ESPUI.updateSwitcher(sender->id, instance->_config->temperatureConfig->setManualInputActive(sender->value.toInt() > 0));
            //instance->updateStatus();
        },
        this);
    ESPUI.setElementStyle(_swManualTempInput, STYLE_SWITCH_INOUT);
    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, emptyString.c_str(), "Manual (fallback if no input available)", ControlColor::None, inputTempGrp), STYLE_LBL_INOUT_MANUAL_ENABLE);



    // Output Temperature Group
    auto outputTempGrp = ESPUI.addControl(ControlType::Label, "Output Temperature", emptyString, ControlColor::None);
    ESPUI.setElementStyle(outputTempGrp, STYLE_HIDDEN);

    _lblTempOutput = ESPUI.addControl(ControlType::Label, emptyString.c_str(), String(NAN) + " °C", ControlColor::None, outputTempGrp);
    ESPUI.setElementStyle(_lblTempOutput, STYLE_LBL_INOUT);
    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, emptyString.c_str(), "Actual", ControlColor::None, outputTempGrp), STYLE_LBL_INOUT_VALUE_OUTPUT);

    _lblTempTarget = ESPUI.addControl(ControlType::Label, emptyString.c_str(), String(NAN) + " °C", ControlColor::None, outputTempGrp);
    ESPUI.setElementStyle(_lblTempTarget, STYLE_LBL_INOUT);
    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, emptyString.c_str(), "Target", ControlColor::None, outputTempGrp), STYLE_LBL_INOUT_VALUE_OUTPUT);
    
    _numManualTempOutput = ESPUI.addControl(
        ControlType::Number, emptyString.c_str(), String(config->temperatureConfig->getManualOutputTemperature(), 1), ControlColor::None, outputTempGrp,
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
    //ESPUI.addControl(ControlType::Step, emptyString, "0.1", ControlColor::None, _numManualTempOutput);
    _swManualTempOutput = ESPUI.addControl(
        ControlType::Switcher, emptyString.c_str(), String(config->temperatureConfig->isManualOutputTemp() ? 1 : 0), ControlColor::None, outputTempGrp,
        [](Control *sender, int type, void *UserInfo)
        {
            Webinterface *instance = static_cast<Webinterface *>(UserInfo);
            ESPUI.updateSwitcher(sender->id, instance->_config->temperatureConfig->setManualOutputActive(sender->value.toInt() > 0));
            //instance->updateStatus();
        },
        this);
    ESPUI.setElementStyle(_swManualTempOutput, STYLE_SWITCH_INOUT);    
    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, emptyString.c_str(), "Manual", ControlColor::None, outputTempGrp), STYLE_LBL_INOUT_MANUAL_ENABLE);



    // Output Power Group
    auto outputPowerGrp = ESPUI.addControl(ControlType::Label, "Output Power Limit", emptyString, ControlColor::None);
    ESPUI.setElementStyle(outputPowerGrp, STYLE_HIDDEN);

    _lblPowerOutput = ESPUI.addControl(ControlType::Label, emptyString.c_str(), "Inactive", ControlColor::None, outputPowerGrp);
    ESPUI.setElementStyle(_lblPowerOutput, STYLE_LBL_INOUT);
    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, emptyString.c_str(), "Actual", ControlColor::None, outputPowerGrp), STYLE_LBL_INOUT_VALUE_OUTPUT);

    _numManualPowerOutput = ESPUI.addControl(
        ControlType::Number, emptyString.c_str(), String(config->powerConfig->getManualPower()), ControlColor::None, outputPowerGrp,
        [](Control *sender, int type, void *UserInfo)
        {
            Webinterface *instance = static_cast<Webinterface *>(UserInfo);
            if(sender->value.isEmpty() || sender->value.toInt() < MIN_POWER_LIMIT)
                ESPUI.updateNumber(sender->id, instance->_config->powerConfig->getManualPower());
            else
                ESPUI.updateNumber(sender->id, instance->_config->powerConfig->setManualPower(sender->value.toInt()));
            ESPUI.setElementStyle(sender->id, STYLE_NUM_INOUT_MANUAL_INPUT);
        },
        this);
    ESPUI.setElementStyle(_numManualPowerOutput, STYLE_NUM_INOUT_MANUAL_INPUT);
    ESPUI.addControl(ControlType::Step, emptyString.c_str(), String(STEP_POWER_LIMIT), ControlColor::None, _numManualPowerOutput);
    _swManualPowerOutput = ESPUI.addControl(
        ControlType::Switcher, emptyString.c_str(), String(config->powerConfig->isManualOutputPower() ? 1 : 0), ControlColor::None, outputPowerGrp,
        [](Control *sender, int type, void *UserInfo)
        {
            Webinterface *instance = static_cast<Webinterface *>(UserInfo);
            ESPUI.updateSwitcher(sender->id, instance->_config->powerConfig->setManualOutputActive(sender->value.toInt() > 0));
            //instance->updateStatus(); // TODO: show enable disable manual output power?
        },
        this);
    ESPUI.setElementStyle(_swManualPowerOutput, STYLE_SWITCH_INOUT);    
    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, emptyString.c_str(), "Manual", ControlColor::None, outputPowerGrp), STYLE_LBL_INOUT_MANUAL_ENABLE);


    // Create tabs
    _adjustmentTab = new AdjustmentTab(config);
    _systemInfoTab = new SystemInfoTab();

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

bool Webinterface::getClientIsConnected() { return ESPUI.ws->count() > 0; }

void Webinterface::setSensorTemp(const float temperature)
{
    ESPUI.updateLabel(_lblSensorTemp, String(temperature) + " °C");
}

void Webinterface::setWeatherTemp(const float temperature, const String timestamp)
{
    if(!timestamp.isEmpty())
        ESPUI.updateLabel(_lblWeatherTemp, String(temperature) + " °C (" + timestamp + ")");
    else
        ESPUI.updateLabel(_lblWeatherTemp, String(temperature) + " °C");
}

void Webinterface::setOutputTemp(const float temperature)
{
    ESPUI.updateLabel(_lblTempOutput, String(temperature) + " °C");
}

void Webinterface::setTargetTemp(const float temperature)
{
    ESPUI.updateLabel(_lblTempTarget, String(temperature) + " °C");
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
        ControlType::Tab, emptyString.c_str(), "System", ControlColor::None, Control::noParent,
        [](Control *sender, int type, void *UserInfo)
        {
            // Update on open tab
            SystemInfoTab *instance = static_cast<SystemInfoTab *>(UserInfo);
            instance->_rebootCnt = REBOOT_CLICK_CNT;
            ESPUI.updateButton(instance->_btnReboot, "Restart " + String(instance->_rebootCnt));
            ESPUI.updateText(instance->_txtPassword, WifiModeChamp.getConfiguredWiFiPassword());
            ESPUI.updateText(instance->_txtSsid, WifiModeChamp.getConfiguredWiFiSSID());
            instance->updateBtnSaveState();
        },
        this);

    // Performance group
    _lblPerformance = ESPUI.addControl(ControlType::Label, "Performance", emptyString, ControlColor::None, _tab);
    ESPUI.setElementStyle(_lblPerformance, "background-color: unset; text-align-last: left;");

    // Device info group
    auto device = String("Model:\t ") + String(ESP.getChipModel()) + "\n" +
                "Flash:\t " + String(ESP.getFlashChipSize()) + "\n" +
                "Freq:\t " + String(ESP.getCpuFreqMHz()) + " MHz\n" +
                "Cores:\t " + String(ESP.getChipCores()) + "\n" +
                "Revision: " + String(ESP.getChipRevision());
    auto lblDevice = ESPUI.addControl(ControlType::Label, "Device", device, ControlColor::None, _tab);
    ESPUI.setElementStyle(lblDevice, "background-color: unset; text-align-last: left;");
    _btnReboot = ESPUI.addControl(
        ControlType::Button, emptyString.c_str(), "Restart " + String(_rebootCnt), ControlColor::None, lblDevice,
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

            ESPUI.updateButton(sender->id, "Restart " + String(instance->_rebootCnt));
        },
        this);

    // Software info group
    auto software = String("Arduiono Verison:\t ") + String(ESP_ARDUINO_VERSION_MAJOR) + "." + String(ESP_ARDUINO_VERSION_MINOR) + "." + String(ESP_ARDUINO_VERSION_PATCH) + "\n" +
                "SDK Verison:\t\t " + String(ESP.getSdkVersion()) + "\n" +
                "Build:\t\t\t " + String(__DATE__ " " __TIME__);
    auto lblSoftware = ESPUI.addControl(ControlType::Label, "Software", software, ControlColor::None, _tab);
    ESPUI.setElementStyle(lblSoftware, "background-color: unset; text-align-last: left;");
    auto lblOTA = ESPUI.addControl(ControlType::Label, emptyString.c_str(), "<form method=""POST"" action=""/ota"" enctype=""multipart/form-data""><input type=""file"" name=""data"" style=""color:white;background-color:#999"" /><input type=""submit"" name=""upload"" value=""Upload"" title=""Upload Files"" style=""color:white;background-color:#999;width:100px;height:35px""></form>", ControlColor::None, lblSoftware);
    ESPUI.setElementStyle(lblOTA, "background-color: transparent; width: 100%;");


    // Network info group
    _lblInfoTest = ESPUI.addControl(ControlType::Label, "Network Info", emptyString, ControlColor::None, _tab);
    ESPUI.setElementStyle(_lblInfoTest, "background-color: unset; text-align-last: left;");

    // WiFi configuration group
    auto lblWifiSettings = ESPUI.addControl(ControlType::Label, "WiFi Configuration", emptyString, ControlColor::None, _tab);
    ESPUI.setElementStyle(lblWifiSettings, STYLE_HIDDEN);
    _selSsid = ESPUI.addControl(
        ControlType::Select, emptyString.c_str(), WifiModeChamp.getConfiguredWiFiSSID(), ControlColor::None, lblWifiSettings,
        [](Control *sender, int type, void *UserInfo)
        {
            SystemInfoTab *instance = static_cast<SystemInfoTab *>(UserInfo);
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
        ControlType::Text, emptyString.c_str(), WifiModeChamp.getConfiguredWiFiSSID(), ControlColor::None, lblWifiSettings,
        [](Control *sender, int type, void *UserInfo)
        {
            SystemInfoTab *instance = static_cast<SystemInfoTab *>(UserInfo);
            instance->updateBtnSaveState();
        },
        this);

    _txtPassword = ESPUI.addControl(
        ControlType::Password, emptyString.c_str(), WifiModeChamp.getConfiguredWiFiPassword(), ControlColor::None, lblWifiSettings,
        [](Control *sender, int type, void *UserInfo)
        {
            SystemInfoTab *instance = static_cast<SystemInfoTab *>(UserInfo);
            instance->updateBtnSaveState();
        },
        this);

    _btnSave = ESPUI.addControl(
        ControlType::Button, emptyString.c_str(), "Save & Connect", ControlColor::None, lblWifiSettings,
        [](Control *sender, int type, void *UserInfo)
        {
            if (type != B_DOWN)
            {
                return;
            }

            SystemInfoTab *instance = static_cast<SystemInfoTab *>(UserInfo);

            // There is a racing condition that occured if the save button gets pressed without pressing enter on SSID or Password input.
            // This causes the save button event to be fired a couple of ms before the updated value is written into the text boxes, which 
            // leads into invalid credentials due to outdated values. Therefore the credential update is handled async inside the update event.
            WifiModeChamp.setWifiCredentials(ESPUI.getControl(instance -> _txtSsid)->value, ESPUI.getControl(instance -> _txtPassword)->value, true);
        },
        this);

    _btnScan = ESPUI.addControl(
        ControlType::Button, emptyString.c_str(), "Search for WiFi", ControlColor::None, lblWifiSettings,
        [](Control *sender, int type, void *UserInfo)
        {
            if (type != B_DOWN)
            {
                return;
            }

            SystemInfoTab *instance = static_cast<SystemInfoTab *>(UserInfo);
            WifiModeChamp.scanWifiNetworks();
            ESPUI.setEnabled(sender->id, false);
        },
        this);

    WifiModeChamp.scanCallback(std::bind(&SystemInfoTab::wifiScanCompleted, this, std::placeholders::_1));

    update();
    updateBtnSaveState();
}

void SystemInfoTab::wifiScanCompleted(int16_t networkCnt)
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

void SystemInfoTab::updateBtnSaveState()
{
    auto configuredSsid = WifiModeChamp.getConfiguredWiFiSSID();
    auto txtSsid = ESPUI.getControl(_txtSsid)->value;
    auto configuredPassword = WifiModeChamp.getConfiguredWiFiPassword();
    auto txtPassword = ESPUI.getControl(_txtPassword)->value;
    auto valid = txtPassword.isEmpty() || txtPassword.length() > 7;
    auto changePending = valid && (configuredSsid != txtSsid || configuredPassword != txtPassword);
    ESPUI.setEnabled(_btnSave, changePending);
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

    float freeHeap = ESP.getFreeHeap();
    uint32_t heapSize = ESP.getHeapSize();
    float usedHeap = heapSize - freeHeap;
    float maxUsedHeap = ESP.getMaxAllocHeap();
    float freeSketch = ESP.getFreeSketchSpace();
    uint32_t sketchSize = freeSketch + ESP.getSketchSize();

    auto performance = String("Uptime:\t\t\t\t") + String(days) + "d " + String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s\n" +
                "Heap Usage:\t\t\t" + String(usedHeap, 0) + "/" + String(heapSize) + " (" + String(usedHeap / heapSize * 100.0f, 2) + " %)\n" +
                "Heap Allocated Max:\t" + String(maxUsedHeap, 0) + " (" + String(maxUsedHeap / heapSize * 100.0f, 2) + " %)\n" +
                "Sketch Used:\t\t\t" + String(sketchSize - freeSketch, 0) + "/" + String(sketchSize) + " (" + String(freeSketch / sketchSize * 100.0f, 2) + " %)\n" +
                "Temperature:\t\t\t" + String(temperatureRead(), 1) + " °C";
    ESPUI.updateLabel(_lblPerformance, performance);



    auto rssi = WiFi.RSSI();
    auto info = String("Hostname:\t") + WiFi.getHostname() + "\n" +
                "MAC:\t\t" + WiFi.macAddress() + "\n" +
                "IP:\t\t\t" + WiFi.localIP().toString() + "\n" +
                "DNS:\t\t" + WiFi.dnsIP().toString() + "\n" +
                "Gateway:\t" + WiFi.gatewayIP().toString() + "\n" +
                "Subnet:\t\t" + WiFi.subnetMask().toString() + "\n" +
                "SSID:\t\t" + WiFi.SSID() + "\n" +
                "RSSI:\t\t" + String(rssi) + " db (" + String(WifiModeChampClass::wifiSignalQuality(rssi)) + " %)";
    ESPUI.updateLabel(_lblInfoTest, info);
}


/*
##############################################
##             AdjustmentTab                ##
##############################################
*/

AdjustmentTab::AdjustmentTab(Config *config) : _config(config)
{
    _tab = ESPUI.addControl(Tab, emptyString.c_str(), "Adjustments");

    auto tmpAdjGrp = ESPUI.addControl(ControlType::Label, "Temperature Adjustment", emptyString, ControlColor::None, _tab);
    ESPUI.setElementStyle(tmpAdjGrp, STYLE_HIDDEN);
    for (size_t i = 0; i < TEMP_ADJUST_AMOUNT; i++)
    {
        auto adj = config->temperatureConfig->getAdjustment(i);
        auto numAdjTemp = ESPUI.addControl(
             Number, emptyString.c_str(), String(adj->getTemperatureOffset(), 1), ControlColor::None, tmpAdjGrp,
             [](Control *sender, int type, void *UserInfo)
             {
                TemperatureAdjustment *instance = static_cast<TemperatureAdjustment *>(UserInfo);
                if(sender->value.isEmpty())
                    ESPUI.updateControlValue(sender->id, String(instance->getTemperatureOffset(), 1));
                else
                    ESPUI.updateControlValue(sender->id, String(instance->setTemperatureOffset(sender->value.toFloat()), 1));
                ESPUI.setElementStyle(sender->id, STYLE_NUM_TEMP_ADJUST_NORMAL);
             },
             adj);
        ESPUI.setElementStyle(numAdjTemp, STYLE_NUM_TEMP_ADJUST_NORMAL);
        ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, emptyString.c_str(), "°C offset at " + String(adj->tempReal) + " °C", ControlColor::None, tmpAdjGrp), STYLE_LBL_ADJUST);
    }

    auto pwrAdjGrp = ESPUI.addControl(ControlType::Label, "Power Adjustment", emptyString, ControlColor::None, _tab);
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
    _numEnd = ESPUI.addControl(
        ControlType::Number, emptyString.c_str(), String(config->getEnd(), 1), ControlColor::None, groupCtlId,
        [](Control *sender, int type, void *UserInfo)
        {
            PowerAreaTab *instance = static_cast<PowerAreaTab *>(UserInfo);
            if(sender->value.isEmpty())
                ESPUI.updateControlValue(sender->id, String(instance->_config->getEnd(), 1));
            else
                ESPUI.updateControlValue(sender->id, String(instance->_config->setEnd(sender->value.toFloat()), 1));
            instance->updateStatus();
        },
        this);
    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, emptyString.c_str(), "°C   -", ControlColor::None, groupCtlId), "background-color: unset; text-align: left; width: 12.5%;");
        
    _numStart = ESPUI.addControl(
        ControlType::Number, emptyString.c_str(), String(config->getStart(), 1), ControlColor::None, groupCtlId,
        [](Control *sender, int type, void *UserInfo)
        {
            PowerAreaTab *instance = static_cast<PowerAreaTab *>(UserInfo);
            if(sender->value.isEmpty())
                ESPUI.updateControlValue(sender->id, String(instance->_config->getStart(), 1));
            else
                ESPUI.updateControlValue(sender->id, String(instance->_config->setStart(sender->value.toFloat()), 1));
            instance->updateStatus();
        },
        this);
    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, emptyString.c_str(), "°C   =", ControlColor::None, groupCtlId), "background-color: unset; text-align: left; width: 13.5%;");
    
    _numLimit = ESPUI.addControl(
        ControlType::Number, emptyString.c_str(), String(config->getPowerLimit()), ControlColor::None, groupCtlId,
        [](Control *sender, int type, void *UserInfo)
        {
            PowerAreaTab *instance = static_cast<PowerAreaTab *>(UserInfo);
            if(sender->value.isEmpty() || sender->value.toInt() < MIN_POWER_LIMIT)
                ESPUI.updateNumber(sender->id, instance->_config->getPowerLimit());
            else
                ESPUI.updateNumber(sender->id, instance->_config->setPowerLimit(sender->value.toInt()));
            instance->updateStatus();
        },
        this);
    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, emptyString.c_str(), "\%", ControlColor::None, groupCtlId), "background-color: unset; text-align: left; width: 26%;");
    ESPUI.addControl(ControlType::Step, emptyString.c_str(), String(STEP_POWER_LIMIT), ControlColor::None, _numLimit);

    updateStatus();
}

void PowerAreaTab::update()
{
    ESPUI.updateControlValue(_numStart, String(_config->getStart(), 1));
    ESPUI.updateControlValue(_numEnd, String(_config->getEnd(), 1));
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
