/*
    Calculates the temperature or resistance of an thermistor
    based on the Steinhart–Hart equation (T in degrees Kelvin):
    1/T = a + b(Ln R) + c(Ln R)^3
 */

#include <math.h>
#include "Thermistor.h"

static const double offsetKelvin = 273.15;
static const double oneThird = 1.0 / 3.0;

double coefficientA;
double coefficientB;
double coefficientC;

Thermistor::Thermistor(int cLow, int rLow, int cMid, int rMid, int cHigh, int rHigh)
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

const double Thermistor::resistanceFromKelvin(double kelvin)
{
    double alpha = (coefficientA - (1 / kelvin)) / coefficientC;
    double beta = sqrt(pow((coefficientB / (3 * coefficientC)), 3) + (pow(alpha, 2)) / 4);
    return exp(pow(beta - (alpha / 2), oneThird) - pow(beta + (alpha / 2), oneThird));
}

const double Thermistor::resistanceFromCelsius(double celsius)
{
    return resistanceFromKelvin(celsiusToKelvin(celsius));
}

const double Thermistor::kelvinFromResistance(double resistance)
{
    double lnR = log(resistance);
    return 1 / (coefficientA + coefficientB * lnR + coefficientC * pow(lnR, 3));
}

const double Thermistor::celsiusFromResistance(double resistance)
{
    return kelvinToCelsius(kelvinFromResistance(resistance));
}

const double Thermistor::celsiusToKelvin(double celsius)
{
    return celsius + offsetKelvin;
}

const double Thermistor::kelvinToCelsius(double kelvin)
{
    return kelvin - offsetKelvin;
}