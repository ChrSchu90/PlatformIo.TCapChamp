/**
 * @file Area.h
 * @author ChrSchu
 * @brief Represents a configuration area
 * @date 2024-03-24
 *
 * @copyright Copyright (c) 2024
 *
 */

#include <Arduino.h>
#include <ESPUI.h>
#include "Area.h"

/// @brief Creates a new instance of an configurateable area
/// @param dashboard Pointer tho the dashboard to add teh area cards
/// @param id Area ID
Area::Area(byte index) : index(index)
                                             {

                                             };