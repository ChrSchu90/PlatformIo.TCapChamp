#include <math.h>
#include "ThermistorCalc.h"

static const double offsetKelvin = 273.15;
static const double oneThird = 1.0 / 3.0;

double coefficientA;
double coefficientB;
double coefficientC;

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

    coefficientC = (denominatorK1K2 - lnR1R2 * denominatorK1K3 / lnR1R3) / ((lnR1Pow3 - lnR2Pow3) - lnR1R2 * (lnR1Pow3 - lnR3Pow3) / lnR1R3);
    coefficientB = (denominatorK1K2 - coefficientC * (lnR1Pow3 - lnR2Pow3)) / lnR1R2;
    coefficientA = 1 / k1 - coefficientC * lnR1Pow3 - coefficientB * lnR1;
}

/**
 * @brief Calculates the resistance [Ohm] of the given temperature [kelvin]
 *
 * @param kelvin Temperature [kelvin]
 * @return The calculated resistane [Ohm]
 */
const double ThermistorCalc::resistanceFromKelvin(double kelvin)
{
    double alpha = (coefficientA - (1 / kelvin)) / coefficientC;
    double beta = sqrt(pow((coefficientB / (3 * coefficientC)), 3) + (pow(alpha, 2)) / 4);
    return exp(pow(beta - (alpha / 2), oneThird) - pow(beta + (alpha / 2), oneThird));
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
    return 1 / (coefficientA + coefficientB * lnR + coefficientC * pow(lnR, 3));
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
    return celsius + offsetKelvin;
}

/**
 * @brief Converts the temperature [kelvin] into temperature [celsius]
 *
 * @param celsius The temperature in [kelvin]
 * @return The temperature in [celsius]
 */
const double ThermistorCalc::kelvinToCelsius(double kelvin)
{
    return kelvin - offsetKelvin;
}