/**
 * @file WifiInfo.cpp
 * @author ChrSchu
 * @brief Implements a WiFi information tab inside the Webinterface
 * @date 2024-03-24
 *
 * @copyright Copyright (c) 2024
 *
 */

#include <Arduino.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <ESPUI.h>
#include <HTTPClient.h>
#include "TabWifiInfo.h"

/// @brief Creates an instance of the WifiInfo
TabWifiInfo::TabWifiInfo(WiFiManager *wifiManager) : _wifiManager(wifiManager)
{
    _tab = ESPUI.addControl(
        Tab, "WiFi", "WiFi", None, None,
        [](Control *sender, int type, void *UserInfo)
        {
            // Update on open tab
            TabWifiInfo *instance = static_cast<TabWifiInfo *>(UserInfo);
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

            TabWifiInfo *instance = static_cast<TabWifiInfo *>(UserInfo);
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
void TabWifiInfo::update()
{
    ESPUI.updateLabel(_lblMac, "MAC: " + String(WiFi.macAddress()));
    ESPUI.updateLabel(_lblIp, "IP: " + String(WiFi.localIP().toString()));
    ESPUI.updateLabel(_lblDns, "DNS: " + String(WiFi.dnsIP().toString()));
    ESPUI.updateLabel(_lblGateway, "Gateway: " + String(WiFi.gatewayIP().toString()));
    ESPUI.updateLabel(_lblSubnet, "Subnet: " + String(WiFi.subnetMask().toString()));
    ESPUI.updateLabel(_lblSsid, "SSID: " + String(WiFi.SSID()));

    auto rssi = WiFi.RSSI();
    ESPUI.updateLabel(_lblRssi, "RSSI: " + String(rssi) + " (" + String(min(max(2 * (rssi + 100), 0), 100)) + "%)");
}