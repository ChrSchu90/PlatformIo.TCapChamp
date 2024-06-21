#pragma once

#include <DNSServer.h>
#include <WiFi.h>
#include "SerialLogging.h"

#define KEY_WIFI_SETTINGS_NAMESPACE "WifiModeChamp" // Namespace key for preferences to store data
#define KEY_WIFI_SETTINGS_SSID "WiFiSsid"           // Key for WiFi SSID
#define KEY_WIFI_SETTINGS_PASSWORD "WiFiPassword"   // Key for WiFi Password

enum class WifiModeChampState
{
    // end() => NETWORK_DISABLED
    NETWORK_DISABLED = 0,
    // NETWORK_DISABLED => NETWORK_ENABLED
    NETWORK_ENABLED,

    // NETWORK_ENABLED => NETWORK_CONNECTING
    NETWORK_CONNECTING,
    // NETWORK_CONNECTING => NETWORK_TIMEOUT
    NETWORK_TIMEOUT,
    // NETWORK_CONNECTING => NETWORK_CONNECTED
    // NETWORK_RECONNECTING => NETWORK_CONNECTED
    NETWORK_CONNECTED, // final state
    // NETWORK_CONNECTED => NETWORK_DISCONNECTED
    NETWORK_DISCONNECTED,
    // NETWORK_DISCONNECTED => NETWORK_RECONNECTING
    NETWORK_RECONNECTING,

    // NETWORK_ENABLED => AP_STARTING
    AP_STARTING,
    // AP AP_STARTING => AP_STARTED
    AP_STARTED, // final state
};

enum class WifiModeChampMode
{
    None = 0,
    // wifi ap
    AP = 1,
    // wifi sta
    STA = 2
};

typedef std::function<void(WifiModeChampState previous, WifiModeChampState state)> WifiModeChampStateCallback;

typedef std::function<void(int16_t networkCnt)> WifiModeChampWifiScanCallback;

class WifiModeChampClass
{
public:
    ~WifiModeChampClass() { end(); }

    static int8_t wifiSignalQuality(int32_t rssi);

    /// @brief 
    /// @param apSSID Optional SSID for the AP mode, falls back to ESP-[SerialNumber]
    /// @param apPassword Optional password for the AP mode, if not defined accassable woithout password
    /// @param hostname Optional hostname, falls back to ESP-[SerialNumber]
    void begin(char const *apSSID = "", char const *apPassword = "", char const *hostname = "");

    /// @brief Sets the WiFi credentials for the STA connection, connect to the WiFi and optionally saves the new credentials
    /// @param ssid the SSID for the STA connection
    /// @param password the password for the STA connection (must be at least 8 characters long)
    /// @returns if the credentials could be updated
    bool setWifiCredentials(const String ssid, const String password, bool save = false);

    // loop() method to be called from main loop()
    void loop();

    // Stops the network stack
    void end();

    // Listen for network state change
    void listen(WifiModeChampStateCallback callback) { _stateCallback = callback; }

    void scanCallback(WifiModeChampWifiScanCallback callback) { _scanCallback = callback; }

    // Returns the current network state
    WifiModeChampState getState() const { return _state; }
    // Returns the current network state name
    const char *getStateName() const;
    const char *getStateName(WifiModeChampState state) const;

    /// @brief Gets the default host name
    const String getDefaultHostName();

    // returns the current default mode of the ESP (STA, AP, ETH). ETH has priority over STA if both are connected
    WifiModeChampMode getMode() const;

    inline bool isConnected() const { return getIPAddress()[0] != 0; }

    inline const String getMACAddress() const { return getMACAddress(getMode()); }
    const String getMACAddress(WifiModeChampMode mode) const;

    // Returns the IP address of the WiFi, or IP address of the AP, or empty if not available
    inline const IPAddress getIPAddress() const { return getIPAddress(getMode()); }
    const IPAddress getIPAddress(WifiModeChampMode mode) const;

    /// @brief Gets the SSID of the current WiFi, or SSID of the AP, or empty if not available
    const String getWiFiSSID() const;
    /// @brief Gets the BSSID of the current WiFi, or BSSID of the AP, or empty if not available
    const String getWiFiBSSID() const;
    /// @brief Gets the RSSI of the current WiFi, or -1 if not available
    int8_t getWiFiRSSI() const;
    /// @brief Gets the signal quality (percentage from 0 to 100) of the current WiFi, or -1 if not available
    int8_t getWiFiSignalQuality() const;

    /// @brief Scan avaialbe WiFi networks, use the scanCallback to receive the scan result
    void scanWifiNetworks() { _scanRequested = true; }
    /// @brief Gets if there is a WiFi scan pending
    bool getWifiScanPending() { return _scanRequested == true; }


    /// @brief Gets the hostname passed from begin()
    const String &getHostname() const { return _hostname; }

    /// @brief Gets the SSID name used for the AP mode
    const String &getAccessPointSSID() const { return _apSSID; }
    /// @brief Gets the Password used for the AP mode
    const String &getAccessPointPassword() const { return _apPassword; }

    /// @brief SSID name to connect to, loaded from config or set from begin()
    const String &getConfiguredWiFiSSID() const { return _wifiSsid; }
    /// @brief Password for the WiFi to connect to, loaded from config or set from begin()
    const String &getConfiguredWiFiPassword() const { return _wifiPassword; }

    /// @brief Maximum duration in seconds that the a reconnect will be pending before switching to AP mode
    uint32_t getReconnectTimeout() const { return _reconnectTimeout; }
    /// @brief Maximum duration in seconds that the a reconnect will be pending before switching to AP mode
    void setReconnectTimeout(uint32_t timeout) { _reconnectTimeout = std::max(timeout, (uint32_t)10); }

    // Maximum duration in seconds that the ESP will try to connect to the WiFi before giving up and start the AP mode
    uint32_t getConnectTimeout() const { return _connectTimeout; }
    /// @brief Maximum in seconds duration that the ESP will try to connect to the WiFi before giving up and start the AP mode
    void setConnectTimeout(uint32_t timeout) { _connectTimeout = std::max(timeout, (uint32_t)5); }

    /// @brief Wait time in seconds between WiFi scans, is also used to reconnect to the configured SSID if connection got loss and reconnect timeouts
    uint32_t getWifiScanWaitTime() const { return _waitBetweenWifiScans; }
    /// @brief Wait time in seconds between WiFi scans, is also used to reconnect to the configured SSID if connection got loss and reconnect timeouts
    void setWifiScanWaitTime(uint32_t waitTime) { _waitBetweenWifiScans = std::max(waitTime, (uint32_t)5); }
    /// @brief Timeout in seconds for WiFi scans
    uint32_t getWifiScanTimeout() const { return _timeoutWifiScan; }
    /// @brief Timeout in seconds for WiFi scans
    void setWifiScanTimeout(uint32_t waitTime) { _timeoutWifiScan = std::max(waitTime, (uint32_t)5); }

    /// @brief when using auto-load and save of configuration, this method can clear saved states.
    void clearConfiguration();

private:
    WifiModeChampState _state = WifiModeChampState::NETWORK_DISABLED;
    WifiModeChampStateCallback _stateCallback = nullptr;
    WifiModeChampWifiScanCallback _scanCallback = nullptr;
    DNSServer *_dnsServer = nullptr;
    int64_t _lastTime = -1;
    String _hostname = "";
    String _apSSID = emptyString;
    String _apPassword = emptyString;
    String _wifiSsid = emptyString;         // SSID for STA connection
    String _wifiPassword = emptyString;     // Password for STA connection
    uint32_t _connectTimeout = 20;
    uint32_t _reconnectTimeout = 180;
    int64_t _lastReconnectTime = -1;
    uint32_t _waitBetweenWifiScans = 20;
    uint32_t _timeoutWifiScan = 10;
    int64_t _lastScanCompleted = -1;
    int64_t _lastScanStarted = -1;
    bool _scanRequested = false;
    WiFiEventId_t _wifiEventListenerId = 0;

private:
    void _setState(WifiModeChampState state);
    void _startSTA();
    void _startAP();
    void _stopAP();
    void _onWiFiEvent(WiFiEvent_t event);
    bool _durationPassed(uint32_t intervalSec);

    void resetReconnectTimeout() { _lastReconnectTime = millis(); }
    bool reconnectTimeoutElapsed();

    /// @brief Scan available WiFi networks
    /// @return the amount of WiFi networks that has been found
    int16_t scanWifiNetworks(bool ignoreWaitTime);
    void clearWifiScanResult();
    void startWifiScan();    
};

extern WifiModeChampClass WifiModeChamp;
