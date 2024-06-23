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

// const char *OTA_INDEX PROGMEM = R"=====(<!DOCTYPE html><html><head><meta charset=utf-8><title>OTA</title></head><body><div class="upload"><form method="POST" action="/ota" enctype="multipart/form-data"><input type="file" name="data" /><input type="submit" name="upload" value="Upload" title="Upload Files"></form></div></body></html>)=====";

Webinterface::Webinterface(uint16_t port, Config *config)
{
#ifdef LOG_DEBUG
    ESPUI.setVerbosity(Verbosity::Verbose);
#endif
    // Create global labels
    _lblSensorTemp = ESPUI.addControl(ControlType::Label, "Sensor", String(NAN) + " °C", None);
    _lblWeatherTemp = ESPUI.addControl(ControlType::Label, "Weather API", String(NAN) + " °C", None);
    _lblOutputTemp = ESPUI.addControl(ControlType::Label, "Output Temperature", String(NAN) + " °C", None);
    _lblOutputPower = ESPUI.addControl(ControlType::Label, "Output Power Limit", String(NAN) + " \%", None);

    // Create tabs
    temperatureTab = new TemperatureTab(config->temperatureConfig);
    powerTab = new PowerTab(config->powerConfig);
    systemInfoTab = new SystemInfoTab();
    wifiInfoTab = new WifiInfoTab();

    // Start ESP UI https://github.com/s00500/ESPUI
    // ESPUI.prepareFileSystem();  //Copy across current version of ESPUI resources
    // ESPUI.list(); //List all files on LittleFS, for info
    // ESPUI.jsonInitialDocumentSize = 12000;
    // ESPUI.jsonUpdateDocumentSize = 4000;
    ESPUI.captivePortal = true;
    ESPUI.begin("T-Cap Champ", nullptr, nullptr, port);

    // WiFi captive portal for iOS
    ESPUI.server->on(
        "/hotspot-detect.html",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {
            AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", HTML_INDEX);
            request->send(response);
        });

    // NOTE: Control is added inside SystemTab! The callback needs to be added after the webserver has been started.
    ESPUI.server->on(
        "/ota",
        HTTP_POST,
        [](AsyncWebServerRequest *request)
        {
            request->send(200);
        },
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
    ESPUI.updateLabel(_lblOutputTemp, String(temperature) + " °C");
}

void Webinterface::setOuputPowerLimit(const float powerLimit)
{
    if (powerLimit < 10)
    {
        ESPUI.updateLabel(_lblOutputPower, "Not Active");
    }
    else
    {
        ESPUI.updateLabel(_lblOutputPower, String(powerLimit, 0) + " \%");
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

    auto lblOTA = ESPUI.addControl(ControlType::Label, "OTA Update", "<form method=""POST"" action=""/ota"" enctype=""multipart/form-data""><input type=""file"" name=""data"" /><input type=""submit"" name=""upload"" value=""Upload"" title=""Upload Files""></form>", ControlColor::None, _tab);
    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, NO_VALUE, ControlColor::None, lblOTA), "background-color: unset; width: 100%;");

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
    // ESPUI.addControl(Separator, "Areas", NO_VALUE, ControlColor::None, tab->getTabId());

    _swEnabled = ESPUI.addControl(
        ControlType::Switcher, config->name.c_str(), String(config->isEnabled() ? 1 : 0), ControlColor::None, tab->getTabId(),
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureAreaTab *instance = static_cast<TemperatureAreaTab *>(UserInfo);
            ESPUI.updateSwitcher(sender->id, instance->_config->setEnabled(sender->value.toInt() > 0));
            instance->updateStatus();
        },
        this);

    // Empty label for spaceing
    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, NO_VALUE, ControlColor::None, _swEnabled), "background-color: unset; width: 100%;");

    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, "Start °C", ControlColor::None, _swEnabled), "background-color: unset; width: 100%;");
    _sldStart = ESPUI.addControl(
        ControlType::Slider, "Start", String(config->getStart()), ControlColor::None, _swEnabled,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureAreaTab *instance = static_cast<TemperatureAreaTab *>(UserInfo);
            ESPUI.updateSlider(sender->id, instance->_config->setStart(sender->value.toInt()));
            instance->updateStatus();
        },
        this);
    ESPUI.addControl(ControlType::Min, NO_VALUE, MIN_TEMPERATURE_VALUE, ControlColor::None, _sldStart);
    ESPUI.addControl(ControlType::Max, NO_VALUE, MAX_TEMPERATURE_VALUE, ControlColor::None, _sldStart);
    // ESPUI.addControl(Step, NO_VALUE, "1", ControlColor::None, _sldStart);

    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, "End °C", ControlColor::None, _swEnabled), "background-color: unset; width: 100%;");
    _sldEnd = ESPUI.addControl(
        Slider, "End", String(config->getEnd()), ControlColor::None, _swEnabled,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureAreaTab *instance = static_cast<TemperatureAreaTab *>(UserInfo);
            ESPUI.updateSlider(sender->id, instance->_config->setEnd(sender->value.toInt()));
            instance->updateStatus();
        },
        this);
    ESPUI.addControl(ControlType::Min, NO_VALUE, MIN_TEMPERATURE_VALUE, ControlColor::None, _sldEnd);
    ESPUI.addControl(ControlType::Max, NO_VALUE, MAX_TEMPERATURE_VALUE, ControlColor::None, _sldEnd);
    // ESPUI.addControl(Step, NO_VALUE, "1", ControlColor::None, _sldEnd);

    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, "Offset °C", ControlColor::None, _swEnabled), "background-color: unset; width: 100%;");
    _sldOffset = ESPUI.addControl(
        ControlType::Slider, "Offset", String(config->getOffset()), ControlColor::None, _swEnabled,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureAreaTab *instance = static_cast<TemperatureAreaTab *>(UserInfo);
            ESPUI.updateSlider(sender->id, instance->_config->setOffset(sender->value.toInt()));
        },
        this);
    ESPUI.addControl(ControlType::Min, NO_VALUE, MIN_TEMPERATURE_VALUE, ControlColor::None, _sldOffset);
    ESPUI.addControl(ControlType::Max, NO_VALUE, MAX_TEMPERATURE_VALUE, ControlColor::None, _sldOffset);
    // ESPUI.addControl(Step, NO_VALUE, "1", ControlColor::None, _sldOffset);

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
    _tab = ESPUI.addControl(Tab, NO_VALUE, "Temperature");
    _swManualMode = ESPUI.addControl(
        ControlType::Switcher, "Manual Temperature", String(config->isManualMode() ? 1 : 0), ControlColor::None, _tab,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureTab *instance = static_cast<TemperatureTab *>(UserInfo);
            ESPUI.updateSwitcher(sender->id, instance->_config->setManualMode(sender->value.toInt() > 0));
            instance->updateStatus();
        },
        this);

    _sldManualTemp = ESPUI.addControl(
        ControlType::Slider, "Temperature", String(config->getManualTemperature()), ControlColor::None, _swManualMode,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureTab *instance = static_cast<TemperatureTab *>(UserInfo);
            ESPUI.updateSlider(sender->id, instance->_config->setManualTemperature(sender->value.toInt()));
        },
        this);
    ESPUI.addControl(ControlType::Min, NO_VALUE, MIN_TEMPERATURE_VALUE, ControlColor::None, _sldManualTemp);
    ESPUI.addControl(ControlType::Max, NO_VALUE, MAX_TEMPERATURE_VALUE, ControlColor::None, _sldManualTemp);
    // ESPUI.addControl(Step, NO_VALUE, "1", ControlColor::None, _sldManualTemp);

    ////
    //// TEST START
    ////

    // auto testSliderGroup = ESPUI.addControl(ControlType::Label, "Temperature Adjustment", NO_VALUE, ControlColor::None, _tab);
    // ESPUI.setElementStyle(testSliderGroup, "background-color: unset; width: 0px; height: 0px; display: none;");
    // String inputStyle = "width: 30%;";
    // String labelStyle = "background-color: unset; width: 70%; text-align-last: left;";
    //// for (short i = 25; i >= -20; i--)
    // for (short i = 0; i >= 1; i--)
    //{
    //     auto inputCtl = ESPUI.addControl(
    //         Number, NO_VALUE, String(i), ControlColor::None, testSliderGroup,
    //         [](Control *sender, int type, void *UserInfo)
    //         {
    //             Serial.println("Value temperature changed to " + String(sender->value));
    //         },
    //         this);
    //     ESPUI.setElementStyle(inputCtl, inputStyle);
    //
    //    auto lblCtl = ESPUI.addControl(ControlType::Label, NO_VALUE, "°C  at " + String(i) + " °C", ControlColor::None, testSliderGroup);
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
    // ESPUI.addControl(Separator, "Areas", NO_VALUE, ControlColor::None, tab->getTabId());

    _swEnabled = ESPUI.addControl(
        ControlType::Switcher, config->name.c_str(), String(config->isEnabled() ? 1 : 0), ControlColor::None, tab->getTabId(),
        [](Control *sender, int type, void *UserInfo)
        {
            PowerAreaTab *instance = static_cast<PowerAreaTab *>(UserInfo);
            ESPUI.updateSwitcher(sender->id, instance->_config->setEnabled(sender->value.toInt() > 0));
            instance->updateStatus();
        },
        this);

    // Empty label for spaceing
    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, NO_VALUE, ControlColor::None, _swEnabled), "background-color: unset; width: 100%;");

    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, "Start °C", ControlColor::None, _swEnabled), "background-color: unset; width: 100%;");
    _sldStart = ESPUI.addControl(
        Slider, "Start", String(config->getStart()), ControlColor::None, _swEnabled,
        [](Control *sender, int type, void *UserInfo)
        {
            PowerAreaTab *instance = static_cast<PowerAreaTab *>(UserInfo);
            ESPUI.updateSlider(sender->id, instance->_config->setStart(sender->value.toInt()));
            instance->updateStatus();
        },
        this);
    ESPUI.addControl(ControlType::Min, NO_VALUE, MIN_TEMPERATURE_VALUE, ControlColor::None, _sldStart);
    ESPUI.addControl(ControlType::Max, NO_VALUE, MAX_TEMPERATURE_VALUE, ControlColor::None, _sldStart);
    // ESPUI.addControl(Step, NO_VALUE, "1", ControlColor::None, _sldStart);

    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, "End °C", ControlColor::None, _swEnabled), "background-color: unset; width: 100%;");
    _sldEnd = ESPUI.addControl(
        ControlType::Slider, "End", String(config->getEnd()), ControlColor::None, _swEnabled,
        [](Control *sender, int type, void *UserInfo)
        {
            PowerAreaTab *instance = static_cast<PowerAreaTab *>(UserInfo);
            ESPUI.updateSlider(sender->id, instance->_config->setEnd(sender->value.toInt()));
            instance->updateStatus();
        },
        this);
    ESPUI.addControl(ControlType::Min, NO_VALUE, MIN_TEMPERATURE_VALUE, ControlColor::None, _sldEnd);
    ESPUI.addControl(ControlType::Max, NO_VALUE, MAX_TEMPERATURE_VALUE, ControlColor::None, _sldEnd);
    // ESPUI.addControl(Step, NO_VALUE, "1", ControlColor::None, _sldEnd);

    ESPUI.setElementStyle(ESPUI.addControl(ControlType::Label, NO_VALUE, "Power Limit \%", ControlColor::None, _swEnabled), "background-color: unset; width: 100%;");
    _sldPowerLimit = ESPUI.addControl(
        ControlType::Slider, "Limit", String(config->getPowerLimit()), ControlColor::None, _swEnabled,
        [](Control *sender, int type, void *UserInfo)
        {
            PowerAreaTab *instance = static_cast<PowerAreaTab *>(UserInfo);
            ESPUI.updateSlider(sender->id, instance->_config->setPowerLimit(sender->value.toInt()));
        },
        this);
    ESPUI.addControl(ControlType::Min, NO_VALUE, MIN_POWER_LIMIT_VALUE, ControlColor::None, _sldPowerLimit);
    ESPUI.addControl(ControlType::Max, NO_VALUE, MAX_POWER_LIMIT_VALUE, ControlColor::None, _sldPowerLimit);
    ESPUI.addControl(ControlType::Step, NO_VALUE, STEP_POWER_LIMIT_VALUE, ControlColor::None, _sldPowerLimit);

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
    _tab = ESPUI.addControl(Tab, NO_VALUE, "Power Limit");
    _swManualMode = ESPUI.addControl(
        ControlType::Switcher, "Manual Power Limit", String(config->isManualMode() ? 1 : 0), ControlColor::None, _tab,
        [](Control *sender, int type, void *UserInfo)
        {
            PowerTab *instance = static_cast<PowerTab *>(UserInfo);
            ESPUI.updateSwitcher(sender->id, instance->_config->setManualMode(sender->value.toInt() > 0));
            instance->updateStatus();
        },
        this);

    _sldManualPower = ESPUI.addControl(
        ControlType::Slider, "Power", String(config->getManualPower()), ControlColor::None, _swManualMode,
        [](Control *sender, int type, void *UserInfo)
        {
            PowerTab *instance = static_cast<PowerTab *>(UserInfo);
            ESPUI.updateSlider(sender->id, instance->_config->setManualPower(sender->value.toInt()));
        },
        this);
    ESPUI.addControl(ControlType::Min, NO_VALUE, MIN_POWER_LIMIT_VALUE, ControlColor::None, _sldManualPower);
    ESPUI.addControl(ControlType::Max, NO_VALUE, MAX_POWER_LIMIT_VALUE, ControlColor::None, _sldManualPower);
    ESPUI.addControl(ControlType::Step, NO_VALUE, STEP_POWER_LIMIT_VALUE, ControlColor::None, _sldManualPower);

    ////
    //// TEST START
    ////

    // auto testSliderGroup = ESPUI.addControl(ControlType::Label, "Power Adjustment", NO_VALUE, ControlColor::None, _tab);
    // ESPUI.setElementStyle(testSliderGroup, "background-color: unset; width: 0px; height: 0px; display: none;");
    // String inputStyle = "width: 30%;";
    // String labelStyle = "background-color: unset; width: 70%; text-align-last: left;";
    //// for (short i = 25; i >= -20; i--)
    // for (short i = 20; i >= -20; i--)
    //{
    //     auto inputCtl = ESPUI.addControl(
    //         Number, NO_VALUE, "100", ControlColor::None, testSliderGroup,
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
    //    auto lblCtl = ESPUI.addControl(ControlType::Label, NO_VALUE, "\% at " + String(i) + " °C", ControlColor::None, testSliderGroup);
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
    ESPUI.setElementStyle(lblWifiSettings, "background-color: unset; width: 0px; height: 0px; display: none;");
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
        // TODO: is there a better solution? On the other hand is will not be used very much...
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