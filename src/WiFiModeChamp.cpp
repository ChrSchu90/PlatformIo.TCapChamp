/*
    Source: https://github.com/mathieucarbou/MycilaESPConnect
    Modified to implement only the reconnect and WiFi mode handling,
    since this project does already provide a webinterface.

    In addition: Reconnets to the WiFi after a disconnect, by checking if
    the configured WiFi is available again.
*/

#define LOG_LEVEL NONE

#include <esp_mac.h>
#include <Preferences.h>
#include "WiFiModeChamp.h"

static const char *NetworkStateNames[] = {
    "NETWORK_DISABLED",
    "NETWORK_ENABLED",
    "NETWORK_CONNECTING",
    "NETWORK_TIMEOUT",
    "NETWORK_CONNECTED",
    "NETWORK_DISCONNECTED",
    "NETWORK_RECONNECTING",
    "AP_STARTING",
    "AP_STARTED",
};

const char *WifiModeChampClass::getStateName() const
{
    return NetworkStateNames[static_cast<int>(_state)];
}

const char *WifiModeChampClass::getStateName(WifiModeChampState state) const
{
    return NetworkStateNames[static_cast<int>(state)];
}

WifiModeChampMode WifiModeChampClass::getMode() const
{
    switch (_state)
    {
    case WifiModeChampState::AP_STARTED:
        return WifiModeChampMode::AP;
        break;
    case WifiModeChampState::NETWORK_CONNECTED:
    case WifiModeChampState::NETWORK_DISCONNECTED:
    case WifiModeChampState::NETWORK_RECONNECTING:
        if (WiFi.localIP()[0] != 0)
            return WifiModeChampMode::STA;
        return WifiModeChampMode::None;
    default:
        return WifiModeChampMode::None;
    }
}

const String WifiModeChampClass::getMACAddress(WifiModeChampMode mode) const
{
    String mac = emptyString;

    switch (mode)
    {
    case WifiModeChampMode::AP:
        mac = WiFi.softAPmacAddress();
        break;
    case WifiModeChampMode::STA:
        mac = WiFi.macAddress();
        break;
    default:
        mac = emptyString;
        break;
    }

    if (mac != emptyString && mac != "00:00:00:00:00:00")
        return mac;

    // ESP_MAC_IEEE802154 is used to mean "no MAC address" in this context
    esp_mac_type_t type = esp_mac_type_t::ESP_MAC_IEEE802154;

    switch (mode)
    {
    case WifiModeChampMode::AP:
        type = ESP_MAC_WIFI_SOFTAP;
        break;
    case WifiModeChampMode::STA:
        type = ESP_MAC_WIFI_STA;
        break;
    default:
        break;
    }

    if (type == esp_mac_type_t::ESP_MAC_IEEE802154)
        return emptyString;

    uint8_t bytes[6] = {0, 0, 0, 0, 0, 0};
    if (esp_read_mac(bytes, type) != ESP_OK)
        return emptyString;

    char buffer[18] = {0};
    snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X", bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
    return String(buffer);
}

const IPAddress WifiModeChampClass::getIPAddress(WifiModeChampMode mode) const
{
    const wifi_mode_t wifiMode = WiFi.getMode();
    switch (mode)
    {
    case WifiModeChampMode::AP:
        return wifiMode == WIFI_MODE_AP || wifiMode == WIFI_MODE_APSTA ? WiFi.softAPIP() : IPAddress();
    case WifiModeChampMode::STA:
        return wifiMode == WIFI_MODE_STA ? WiFi.localIP() : IPAddress();
    default:
        return IPAddress();
    }
}

const String WifiModeChampClass::getWiFiSSID() const
{
    switch (WiFi.getMode())
    {
    case WIFI_MODE_AP:
    case WIFI_MODE_APSTA:
        return WiFi.softAPSSID();
    case WIFI_MODE_STA:
        return WiFi.SSID();
    default:
        return emptyString;
    }
}

const String WifiModeChampClass::getWiFiBSSID() const
{
    switch (WiFi.getMode())
    {
    case WIFI_MODE_AP:
    case WIFI_MODE_APSTA:
        return WiFi.softAPmacAddress();
    case WIFI_MODE_STA:
        return WiFi.BSSIDstr();
    default:
        return emptyString;
    }
}

int8_t WifiModeChampClass::getWiFiRSSI() const
{
    return WiFi.getMode() == WIFI_MODE_STA ? WiFi.RSSI() : 0;
}

int8_t WifiModeChampClass::getWiFiSignalQuality() const
{
    return WiFi.getMode() == WIFI_MODE_STA ? wifiSignalQuality(WiFi.RSSI()) : 0;
}

int8_t WifiModeChampClass::wifiSignalQuality(int32_t rssi)
{
    return min(max(2 * (rssi + 100), 0), 100);
    // int32_t s = map(rssi, -90, -30, 0, 100);
    // return s > 100 ? 100 : (s < 0 ? 0 : s);
}

const String WifiModeChampClass::getDefaultHostName()
{
    auto host = String(ESP.getEfuseMac(), HEX);
    host.toUpperCase();
    return host;
}

void WifiModeChampClass::begin(char const *apSSID, char const *apPassword, char const *hostname)
{
    if (_wifiEventListenerId != 0)
        return;

#ifdef LOG_DEBUG
    LOG_DEBUG(F("WiFiModeChamp"), F("begin"), F("Starting in non-blocking mode..."));
#endif

    if (_wifiSsid.isEmpty())
    {
#ifdef LOG_DEBUG
        LOG_DEBUG(F("WiFiModeChamp"), F("begin"), F("Loading WiFi settings from config since nothing is configured..."));
#endif

        Preferences preferences;
        preferences.begin(KEY_WIFI_SETTINGS_NAMESPACE, true);
        auto ssid = _wifiSsid;
        auto password = _wifiPassword;
        if (ssid.isEmpty())
        {
            ssid = preferences.getString(KEY_WIFI_SETTINGS_SSID, emptyString);
        }

        if (password.isEmpty())
        {
            password = preferences.getString(KEY_WIFI_SETTINGS_PASSWORD, emptyString);
        }

        preferences.end();
        setWifiCredentials(ssid, password);
    }

    auto host = getDefaultHostName();
    if (apSSID == "")
    {
        _apSSID = "ESP-" + host;
    }
    else
    {
        _apSSID = apSSID;
    }

    if (hostname == "")
    {
        _hostname = "ESP-" + host;
    }
    else
    {
        _hostname = hostname;
    }

    WiFi.mode(WIFI_STA);
    _apPassword = apPassword;
    _wifiEventListenerId = WiFi.onEvent(std::bind(&WifiModeChampClass::onWiFiEvent, this, std::placeholders::_1));
    _state = WifiModeChampState::NETWORK_ENABLED;
}

bool WifiModeChampClass::setWifiCredentials(const String ssid, const String password, bool save)
{
    if (_wifiSsid == ssid && _wifiPassword == password)
    {
        return false;
    }

    _wifiSsid = ssid;
    _wifiPassword = password;
    if (_wifiPassword.isEmpty() && _wifiPassword.length() < 8)
    {
#ifdef LOG_ERROR
        LOG_ERROR(F("WiFiModeChamp"), F("setWifiCredentials"), F("Password is invalid!"));
#endif
        return false;
    }

    if (save && !_wifiSsid.isEmpty() && (_wifiPassword.isEmpty() || _wifiPassword.length() > 7))
    {
        Preferences preferences;
        preferences.begin(KEY_WIFI_SETTINGS_NAMESPACE, false);
        preferences.putString(KEY_WIFI_SETTINGS_SSID, _wifiSsid);
        preferences.putString(KEY_WIFI_SETTINGS_PASSWORD, _wifiPassword);
        preferences.end();
    }

    if (_state == WifiModeChampState::NETWORK_CONNECTED || _state == WifiModeChampState::NETWORK_CONNECTING || _state == WifiModeChampState::NETWORK_RECONNECTING)
    {
#ifdef LOG_DEBUG
        LOG_DEBUG(F("WiFiModeChamp"), F("setWifiCredentials"), F("Connecting to new AP..."));
#endif
        WiFi.disconnect(true, true);
        setState(WifiModeChampState::NETWORK_TIMEOUT);
    }

    return true;
}

void WifiModeChampClass::end()
{
    if (_state == WifiModeChampState::NETWORK_DISABLED)
        return;
#ifdef LOG_DEBUG
    LOG_DEBUG(F("WiFiModeChamp"), F("end"), F("Stopping..."));
#endif
    _lastTime = -1;
    setState(WifiModeChampState::NETWORK_DISABLED);
    WiFi.removeEvent(_wifiEventListenerId);
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_MODE_NULL);
    stopAP();
}

void WifiModeChampClass::loop()
{
    if (_state == WifiModeChampState::NETWORK_DISABLED)
        return;

    if (_dnsServer != nullptr)
    {
        _dnsServer->processNextRequest();
    }

    // start AP mode when network enabled but no wifi ar available, otherwise, tries to connect to WiFi
    if (_state == WifiModeChampState::NETWORK_ENABLED)
    {
        if (_wifiSsid.isEmpty())
        {
            startAP();
        }
        else
        {
            startSTA();
        }
    }

    // connection to WiFi times out ?
    if (_state == WifiModeChampState::NETWORK_CONNECTING && durationPassed(_connectTimeout))
    {
        WiFi.disconnect(true, true);
        setState(WifiModeChampState::NETWORK_TIMEOUT);
    }

    // start AP on network timeout
    if (_state == WifiModeChampState::NETWORK_TIMEOUT)
    {
        startAP();
    }

    // disconnect from network ? reconnect!
    if (_state == WifiModeChampState::NETWORK_DISCONNECTED)
    {
        setState(WifiModeChampState::NETWORK_RECONNECTING);
        resetReconnectTimeout();
    }

    // Switch to AP mode if reconnect timeouts
    if (_state == WifiModeChampState::NETWORK_RECONNECTING && reconnectTimeoutElapsed())
    {
        WiFi.disconnect(true, true);
        setState(WifiModeChampState::NETWORK_TIMEOUT);
    }

    // check if configured WiFi is available again and reconnect
    if (_state == WifiModeChampState::AP_STARTED && !_wifiSsid.isEmpty())
    {
        auto scanCnt = scanWifiNetworks(false);
        if (scanCnt > 0)
        {
            // handle manual WiFi scan request
            if (_scanRequestState == WifiModeChampScanRequestState::Requested || _scanRequestState == WifiModeChampScanRequestState::Pending)
            {
                _scanRequestState = WifiModeChampScanRequestState::Completed;
                if (_scanCallback != nullptr)
                {
                    _scanCallback(scanCnt);
                }
            }
            
            // Check if configured WiFi is available to trigger a reconnect
            for (int16_t i = 0; i < scanCnt; ++i)
            {
                if (WiFi.SSID(i) == _wifiSsid)
                {
                    clearWifiScanResult(); // Only clear on success, otherwise the scan result will be error and the wait time will not work
                    stopAP();
                    setState(WifiModeChampState::NETWORK_ENABLED);
                    break;
                }
            }
        }
    }
    else
    {
        // handle manual WiFi scan request
        if (_scanRequestState == WifiModeChampScanRequestState::Requested || _scanRequestState == WifiModeChampScanRequestState::Pending)
        {
            auto networkCnt = scanWifiNetworks(false);
            if (networkCnt > 0)
            {
                _scanRequestState = WifiModeChampScanRequestState::Completed;
                if (_scanCallback != nullptr)
                {
                    _scanCallback(networkCnt);
                }

                clearWifiScanResult();
            }
        }
    }
}

void WifiModeChampClass::clearConfiguration()
{
    Preferences preferences;
    preferences.begin(KEY_WIFI_SETTINGS_NAMESPACE, false);
    preferences.clear();
    preferences.end();
}

void WifiModeChampClass::setState(WifiModeChampState state)
{
    if (_state == state)
    {
        return;
    }

    const WifiModeChampState previous = _state;
    _state = state;
#ifdef LOG_DEBUG
    LOG_DEBUG(F("WiFiModeChamp"), F("setState"), F("Change state from ") + getStateName(previous) + F(" to ") + getStateName(state));
#endif

    if (_stateCallback != nullptr)
    {
        _stateCallback(previous, state);
    }
}

void WifiModeChampClass::startSTA()
{
    setState(WifiModeChampState::NETWORK_CONNECTING);

#ifdef LOG_DEBUG
    LOG_DEBUG(F("WiFiModeChamp"), F("startSTA"), F("Starting WiFi..."));
#endif

    WiFi.setSleep(false);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.mode(WIFI_STA);

#ifdef LOG_DEBUG
    LOG_DEBUG(F("WiFiModeChamp"), F("startSTA"), F("Connecting to SSID: ") + _wifiSsid);
#endif

    WiFi.begin(_wifiSsid, _wifiPassword);
    _lastTime = millis();

#ifdef LOG_DEBUG
    LOG_DEBUG(F("WiFiModeChamp"), F("startSTA"), F("WiFi started."));
#endif
}

void WifiModeChampClass::startAP()
{
    setState(WifiModeChampState::AP_STARTING);
#ifdef LOG_DEBUG
    LOG_DEBUG(F("WiFiModeChamp"), F("startAP"), F("Starting Access Point..."));
#endif

    WiFi.setSleep(false);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    // WiFi.mode(WIFI_AP_STA);
    WiFi.mode(WIFI_AP);

    if (_apPassword.isEmpty() || _apPassword.length() < 8)
    {
        // Disabling invalid Access Point password which must be at least 8 characters long when set
        WiFi.softAP(_apSSID, emptyString);
    }
    else
    {
        WiFi.softAP(_apSSID, _apPassword);
    }

    if (_dnsServer == nullptr)
    {
        _dnsServer = new DNSServer();
        _dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
        _dnsServer->start(53, "*", WiFi.softAPIP());
    }

#ifdef LOG_DEBUG
    LOG_DEBUG(F("WiFiModeChamp"), F("startAP"), F("Access Point started."));
#endif
}

void WifiModeChampClass::stopAP()
{
#ifdef LOG_DEBUG
    LOG_DEBUG(F("WiFiModeChamp"), F("stopAP"), F("Stopping Access Point..."));
#endif

    _lastTime = -1;
    WiFi.softAPdisconnect(true);
    if (_dnsServer != nullptr)
    {
        _dnsServer->stop();
        delete _dnsServer;
        _dnsServer = nullptr;
    }

#ifdef LOG_DEBUG
    LOG_DEBUG(F("WiFiModeChamp"), F("stopAP"), F("Access Point stopped."));
#endif
}

void WifiModeChampClass::onWiFiEvent(WiFiEvent_t event)
{
    if (_state == WifiModeChampState::NETWORK_DISABLED)
        return;

    switch (event)
    {
    case ARDUINO_EVENT_WIFI_STA_START:
#ifdef LOG_DEBUG
        LOG_DEBUG(F("WiFiModeChamp"), F("onWiFiEvent"), F("WiFiEvent: ARDUINO_EVENT_WIFI_STA_START (State = ") + getStateName() + ")");
#endif
        WiFi.setHostname(_hostname.c_str());
        break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        if (_state == WifiModeChampState::NETWORK_CONNECTING || _state == WifiModeChampState::NETWORK_RECONNECTING)
        {
#ifdef LOG_DEBUG
            LOG_DEBUG(F("WiFiModeChamp"), F("onWiFiEvent"), F("WiFiEvent: ARDUINO_EVENT_WIFI_STA_GOT_IP (State = ") + getStateName() + ")");
#endif
            _lastTime = -1;
            setState(WifiModeChampState::NETWORK_CONNECTED);
        }
        break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        if (_state == WifiModeChampState::NETWORK_CONNECTED)
        {
            if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
            {
#ifdef LOG_DEBUG
                LOG_DEBUG(F("WiFiModeChamp"), F("onWiFiEvent"), F("WiFiEvent: ARDUINO_EVENT_WIFI_STA_DISCONNECTED (State = ") + getStateName() + ")");
#endif
            }
            else
            {
#ifdef LOG_DEBUG
                LOG_DEBUG(F("WiFiModeChamp"), F("onWiFiEvent"), F("WiFiEvent: ARDUINO_EVENT_WIFI_STA_LOST_IP (State = ") + getStateName() + ")");
#endif
            }
            setState(WifiModeChampState::NETWORK_DISCONNECTED);
        }
        break;

    case ARDUINO_EVENT_WIFI_AP_START:
        // WiFi.setHostname(_hostname.c_str());
        WiFi.softAPsetHostname(_hostname.c_str());
#ifdef LOG_DEBUG
        LOG_DEBUG(F("WiFiModeChamp"), F("onWiFiEvent"), F("WiFiEvent: ARDUINO_EVENT_WIFI_AP_START (State = ") + getStateName() + ")");
#endif
        if (_state == WifiModeChampState::AP_STARTING)
        {
            setState(WifiModeChampState::AP_STARTED);
        }

        break;

    default:
        break;
    }
}

void WifiModeChampClass::clearWifiScanResult()
{
#ifdef LOG_DEBUG
    LOG_DEBUG(F("WiFiModeChamp"), F("clearWifiScanResult"), F("Cleaning up WiFi scan results"));
#endif
    WiFi.scanDelete();
    //_lastScanStarted = -1;
    //_lastScanCompleted = -1;
}

void WifiModeChampClass::startWifiScan()
{
#ifdef LOG_DEBUG
    LOG_DEBUG(F("WiFiModeChamp"), F("startWifiScan"), F("Starting scan..."));
#endif
    WiFi.scanDelete();
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
    WiFi.scanNetworks(true);
    _lastScanCompleted = -1;
    _lastScanStarted = millis();
}

int16_t WifiModeChampClass::scanWifiNetworks(bool ignoreWaitTime)
{
    auto scanCnt = WiFi.scanComplete();
    if (scanCnt == WIFI_SCAN_RUNNING)
    {
        return 0;
    }

    // Scan completed
    if (scanCnt > 0 && _lastScanStarted > 0)
    {
#ifdef LOG_DEBUG
        LOG_DEBUG(F("WiFiModeChamp"), F("scanWifiNetworks"), F("Scan completed with ") + scanCnt + " available networks");
#endif
        _lastScanCompleted = millis();
        _lastScanStarted = -1;
        return scanCnt;
    }

    // Foreced scan as request
    if (_scanRequestState == WifiModeChampScanRequestState::Requested)
    {
#ifdef LOG_DEBUG
        LOG_DEBUG(F("WiFiModeChamp"), F("scanWifiNetworks"), F("Starting forced WiFi scan..."));
#endif
        _scanRequestState = WifiModeChampScanRequestState::Pending;
        startWifiScan();
        return 0;
    }

    auto currentMillis = millis();
    if (scanCnt == WIFI_SCAN_FAILED)
    {
        // Scan is cleared or has failed
        startWifiScan();
        return 0;
    }

    if (_lastScanStarted > 0 && currentMillis - _lastScanStarted >= _timeoutWifiScan * 1000)
    {
#ifdef LOG_DEBUG
        LOG_DEBUG(F("WiFiModeChamp"), F("scanWifiNetworks"), F("WiFi scan timed out"));
#endif
        startWifiScan();
        return 0;
    }

    if (_lastScanCompleted > 0 && currentMillis - _lastScanCompleted >= _waitBetweenWifiScans * 1000)
    {
#ifdef LOG_DEBUG
        LOG_DEBUG(F("WiFiModeChamp"), F("scanWifiNetworks"), F("Starting WiFi scan after wating time..."));
#endif
        startWifiScan();
        return 0;
    }

    return 0;
}

bool WifiModeChampClass::durationPassed(uint32_t intervalSec)
{
    if (_lastTime >= 0 && millis() - (uint32_t)_lastTime >= intervalSec * 1000)
    {
        _lastTime = -1;
        return true;
    }
    return false;
}

bool WifiModeChampClass::reconnectTimeoutElapsed()
{
    if (_lastReconnectTime >= 0 && millis() - _lastReconnectTime >= _reconnectTimeout * 1000)
    {
        _lastReconnectTime = -1;
        return true;
    }

    return false;
}

WifiModeChampClass WifiModeChamp;
