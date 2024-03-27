/**
 * @file SystemInfo.cpp
 * @author ChrSchu
 * @brief Implements a system information tab inside the Webinterface
 * @date 2024-03-24
 *
 * @copyright Copyright (c) 2024
 *
 */

#include <Arduino.h>
#include <WifiManager.h>
#include <ESPUI.h>
#include "TabSystemInfo.h"

/// @brief Creates an instance of the SystemInfo
TabSystemInfo::TabSystemInfo()
{
    _tab = ESPUI.addControl(
        Tab, "System", "System", None, None,
        [](Control *sender, int type, void *UserInfo)
        {
            // Update on open tab
            TabSystemInfo *instance = static_cast<TabSystemInfo *>(UserInfo);
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

            TabSystemInfo *instance = static_cast<TabSystemInfo *>(UserInfo);
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
void TabSystemInfo::update()
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