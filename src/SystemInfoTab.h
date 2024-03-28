#pragma once

/**
 * @brief Implements a system information tab inside the Webinterface
 */
class SystemInfoTab
{
private:
    static const int REBOOT_CLICK_CNT = 5;
    uint16_t _tab;
    uint16_t _btnReboot;
    uint16_t _lblUptime;
    uint16_t _lblHeapUsage;
    uint16_t _lblHeapAllocatedMax;
    uint16_t _lblSketch;
    uint16_t _lblTemperature;
    uint32_t _flashSize;
    uint32_t _heapSize;
    uint32_t _sketchSize;
    int _rebootCnt = REBOOT_CLICK_CNT;

protected:
public:

    /// @brief Creates an instance of the SystemInfoTab
    SystemInfoTab();

    /// @brief Update the system information
    void update();
};