#pragma once

#include <WiFiManager.h>

/**
 * @brief Implements a WiFi information tab inside the Webinterface
 */
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