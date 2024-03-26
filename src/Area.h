/**
 * @file Area.h
 * @author ChrSchu
 * @brief Represents a configuration area
 * @date 2024-03-24
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <Arduino.h>
#include <ESPUI.h>

class Area
{
private:
    const String _statusLbl = String("Area " + String(index) + " Status");
    const String _enabledLbl = String("Area " + String(index) + " Enabled");
    const String _startLbl = String("Area " + String(index) + " Start");
    const String _endLbl = String("Area " + String(index) + " End");
    const String _offsetLbl = String("Area " + String(index) + " Offset");
    const String _powerLbl = String("Area " + String(index) + " Power");
protected:
public:
    const byte index;

     /// @brief Creates a new instance of an configurateable area
     /// @param dashboard Pointer tho the dashboard to add teh area cards
     /// @param id Area ID
     Area(byte id);
};
