/*
    Source: https://github.com/mathieucarbou/MycilaESPConnect
    Modified to implement only the reconnect and WiFi mode handling,
    since this project does already provide a webinterface.

    In addition: Reconnets to the WiFi after a disconnect, by checking if
    the configured WiFi is available again.
*/

#pragma once

#include <DNSServer.h>
#include <WiFi.h>
#include "SerialLogging.h"

#define KEY_WIFI_SETTINGS_NAMESPACE "WifiModeChamp" // Preferences namespace key for preferences to store data
#define KEY_WIFI_SETTINGS_SSID "WiFiSsid"           // Preferences value key for the WiFi SSID
#define KEY_WIFI_SETTINGS_PASSWORD "WiFiPassword"   // Preferences value key for the WiFi Password

/// @brief State of the WiFiModeChamp
enum class WifiModeChampState
{
    // Network handling is disbaled due to missing begin() or by end()
    NETWORK_DISABLED = 0,
    // Network handling is enabled by begin()
    NETWORK_ENABLED,
    // Starting network connection
    NETWORK_CONNECTING,
    // Network connection has timed out
    NETWORK_TIMEOUT,
    // Network is connected
    NETWORK_CONNECTED,
    // Network is disconnected
    NETWORK_DISCONNECTED,
    // Reconnecting to network
    NETWORK_RECONNECTING,
    // Starting AP mode
    AP_STARTING,
    // AP mode is started
    AP_STARTED,
};

/// @brief Mode of the WiFiModeChamp
enum class WifiModeChampMode
{
    // Default value
    None = 0,
    // Running in AP mode
    AP = 1,
    // Running in STA mode
    STA = 2
};

/// @brief WiFi scan request state
enum class WifiModeChampScanRequestState
{
    // Default value
    None = 0,
    // Request has been made
    Requested = 1,
    // Request is pending
    Pending = 2,
    // Request has been completed
    Completed = 3
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
    /// @param blockInitalConnect Optional block while connecting for the first time until connection fails or successfully connected based on connect timeout
    /// @param apPassword Optional password for the AP mode, if not defined accassable woithout password
    /// @param hostname Optional hostname, falls back to ESP-[SerialNumber]
    void begin(char const *apSSID = "", const bool blockInitalConnect = false, char const *apPassword = "", char const *hostname = "");

    /// @brief Sets the WiFi credentials for the STA connection, connect to the WiFi and optionally saves the new credentials
    /// @param ssid the SSID for the STA connection
    /// @param password the password for the STA connection (must be at least 8 characters long)
    /// @returns if the credentials could be updated
    bool setWifiCredentials(const String ssid, const String password, bool save = false);

    /// @brief loop() method to be called from main loop()
    void loop();

    /// @brief Stops the network stack
    void end();

    /// @brief Listen for network state change
    void listen(WifiModeChampStateCallback callback) { _stateCallback = callback; }

    /// @brief Listen for async WiFi scan results
    void scanCallback(WifiModeChampWifiScanCallback callback) { _scanCallback = callback; }

    /// @brief Returns the current network state
    WifiModeChampState getState() const { return _state; }
    /// @brief Returns the current network state name
    const char *getStateName() const;
    /// @brief Returns the network state name
    const char *getStateName(WifiModeChampState state) const;

    /// @brief Gets the default host name
    const String getDefaultHostName();

    /// @brief returns the current default mode of the ESP (None, STA or AP).
    WifiModeChampMode getMode() const;

    /// @brief Gets if the ESP is connected to the WiFi
    inline bool isConnected() const { return getIPAddress()[0] != 0; }

    /// @brief Returns the MAC address of the ESP with respect to the current mode
    inline const String getMACAddress() const { return getMACAddress(getMode()); }
    /// @brief Returns the MAC address of the ESP
    const String getMACAddress(WifiModeChampMode mode) const;

    /// @brief Returns the IP address of the WiFi, or IP address of the AP, or empty if not available
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
    void scanWifiNetworks() { _scanRequestState = WifiModeChampScanRequestState::Requested; }
    /// @brief Gets if there is a WiFi scan pending
    bool getWifiScanPending() { return _scanRequestState == WifiModeChampScanRequestState::Completed; }

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

    /// @brief Maximum duration in seconds that the ESP will try to connect to the WiFi before giving up and start the AP mode
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
    WifiModeChampState _state = WifiModeChampState::NETWORK_DISABLED;                      // Current state
    WifiModeChampStateCallback _stateCallback = nullptr;                                   // Callback for state changes
    WifiModeChampWifiScanCallback _scanCallback = nullptr;                                 // Callback for WiFi scan completed
    DNSServer *_dnsServer = nullptr;                                                       // DNS server for AP mode
    int64_t _lastTime = -1;                                                                // Timestamp for async timeout detection
    String _hostname = "";                                                                 // Device host name
    String _apSSID = emptyString;                                                          // SSID for AP mode
    String _apPassword = emptyString;                                                      // Optional password for AP mode (needs to be at least 8 chars long)
    String _wifiSsid = emptyString;                                                        // SSID for STA connection
    String _wifiPassword = emptyString;                                                    // Password for STA connection
    uint32_t _connectTimeout = 20;                                                         // Timeout for connect [seconds]
    uint32_t _reconnectTimeout = 180;                                                      // Timeout for re-connect, if elapsed switch to AP mode [seconds]
    int64_t _lastReconnectTime = -1;                                                       // Millis since last reconnect
    uint32_t _waitBetweenWifiScans = 20;                                                   // Wait time between automatic WiFi scans for reconnect check in AP mode [seconds]
    uint32_t _timeoutWifiScan = 10;                                                        // Timeout for WiFi scans [seconds]
    int64_t _lastScanCompleted = -1;                                                       // Millis since last WiFi scan has been completed
    int64_t _lastScanStarted = -1;                                                         // Missis since last WiFi scan has been started
    WifiModeChampScanRequestState _scanRequestState = WifiModeChampScanRequestState::None; // Scan request state
    WiFiEventId_t _wifiEventListenerId = 0;                                                // Event handler ID for WiFi state callbacks
    bool _saveCredentials = false;                                                         // Save WiFi credentials on next successful connect after they have been changed

private:
    /// @brief Updates the current state
    void setState(WifiModeChampState state);
    /// @brief Switch to WiFi client mode
    void startSTA();
    /// @brief Switch to AP mode
    void startAP();
    /// @brief Stop AP mode
    void stopAP();
    /// @brief WiFi event handler to detect disconnects etc.
    void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
    /// @brief Check if timeout has elapsed by using _lastTime
    bool durationPassed(uint32_t intervalSec);
    /// @brief Reset the reconnect timeout
    void resetReconnectTimeout() { _lastReconnectTime = millis(); }
    /// @brief Check if reconnect timeout has elapsed
    bool reconnectTimeoutElapsed();
    /// @brief Scan available WiFi networks and return the amount of WiFi networks that has been found
    int16_t scanWifiNetworks(bool ignoreWaitTime);
    /// @brief Clear the WiFi scan result to release the used memory
    void clearWifiScanResult();
    /// @brief Starts a async WiFi scan
    void startWifiScan();
    /// @brief Saves the currently used credentials
    void saveCurrentCredentials();
};

extern WifiModeChampClass WifiModeChamp;
