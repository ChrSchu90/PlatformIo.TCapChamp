/**
 * @file ThermistorCalc.cpp
 * @author ChrSchu
 * @brief Calculates the temperature or resistance of an thermistor
 * based on the Steinhart–Hart equation (T in degrees Kelvin):
 * 1/T = a + b(Ln R) + c(Ln R)^3
 * @date 2024-03-24
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <math.h>
#include "ThermistorCalc.h"

static const double OFFSET_KELVIN = 273.15;
static const double ONE_THIRD = 1.0 / 3.0;

double _coefficientA;
double _coefficientB;
double _coefficientC;

/**
 * @brief Creates an instance of the calculator
 *
 */
ThermistorCalc::ThermistorCalc(int cLow, int rLow, int cMid, int rMid, int cHigh, int rHigh)
{
    double lnR1 = log(rLow);
    double k1 = celsiusToKelvin(cLow);
    double lnR2 = log(rMid);
    double k2 = celsiusToKelvin(cMid);
    double lnR3 = log(rHigh);
    double k3 = celsiusToKelvin(cHigh);
    double lnR1R2 = lnR1 - lnR2;
    double lnR1R3 = lnR1 - lnR3;
    double denominatorK1K2 = (1 / k1) - (1 / k2);
    double denominatorK1K3 = (1 / k1) - (1 / k3);

    double lnR1Pow3 = pow(lnR1, 3);
    double lnR2Pow3 = pow(lnR2, 3);
    double lnR3Pow3 = pow(lnR3, 3);

    _coefficientC = (denominatorK1K2 - lnR1R2 * denominatorK1K3 / lnR1R3) / ((lnR1Pow3 - lnR2Pow3) - lnR1R2 * (lnR1Pow3 - lnR3Pow3) / lnR1R3);
    _coefficientB = (denominatorK1K2 - _coefficientC * (lnR1Pow3 - lnR2Pow3)) / lnR1R2;
    _coefficientA = 1 / k1 - _coefficientC * lnR1Pow3 - _coefficientB * lnR1;
}

/**
 * @brief Calculates the resistance [Ohm] of the given temperature [kelvin]
 *
 * @param kelvin Temperature [kelvin]
 * @return The calculated resistane [Ohm]
 */
const double ThermistorCalc::resistanceFromKelvin(double kelvin)
{
    double alpha = (_coefficientA - (1 / kelvin)) / _coefficientC;
    double beta = sqrt(pow((_coefficientB / (3 * _coefficientC)), 3) + (pow(alpha, 2)) / 4);
    return exp(pow(beta - (alpha / 2), ONE_THIRD) - pow(beta + (alpha / 2), ONE_THIRD));
}

/**
 * @brief Calculates the resistance [Ohm] of the given temperature [celsius]
 *
 * @param celsius Temperature in [celsius]
 * @return The calculated resistane [Ohm]
 */
const double ThermistorCalc::resistanceFromCelsius(double celsius)
{
    return resistanceFromKelvin(celsiusToKelvin(celsius));
}

/**
 * @brief Calculates the temperature [kelvin] by the given resistance [Ohm]
 *
 * @param resistance The resistance [Ohm]
 * @return The calculated temperature [kelvin]
 */
const double ThermistorCalc::kelvinFromResistance(double resistance)
{
    double lnR = log(resistance);
    return 1 / (_coefficientA + _coefficientB * lnR + _coefficientC * pow(lnR, 3));
}

/**
 * @brief Calculates the temperature [celsius] by the given resistance [Ohm]
 *
 * @param resistance The resistance [Ohm]
 * @return The temperature [celsius]
 */
const double ThermistorCalc::celsiusFromResistance(double resistance)
{
    return kelvinToCelsius(kelvinFromResistance(resistance));
}

/**
 * @brief Converts the temperature [celsius] into temperature [kelvin]
 *
 * @param celsius The temperature in [celsius]
 * @return The temperature in [kelvin]
 */
const double ThermistorCalc::celsiusToKelvin(double celsius)
{
    return celsius + OFFSET_KELVIN;
}

/**
 * @brief Converts the temperature [kelvin] into temperature [celsius]
 *
 * @param celsius The temperature in [kelvin]
 * @return The temperature in [celsius]
 */
const double ThermistorCalc::kelvinToCelsius(double kelvin)
{
    return kelvin - OFFSET_KELVIN;
}