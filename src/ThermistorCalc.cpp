#include <math.h>
#include "ThermistorCalc.h"

static const double OFFSET_KELVIN = 273.15;
static const double ONE_THIRD = 1.0 / 3.0;

double _coefficientA;
double _coefficientB;
double _coefficientC;

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

const double ThermistorCalc::resistanceFromKelvin(double kelvin)
{
    double alpha = (_coefficientA - (1 / kelvin)) / _coefficientC;
    double beta = sqrt(pow((_coefficientB / (3 * _coefficientC)), 3) + (pow(alpha, 2)) / 4);
    return exp(pow(beta - (alpha / 2), ONE_THIRD) - pow(beta + (alpha / 2), ONE_THIRD));
}

const double ThermistorCalc::resistanceFromCelsius(double celsius)
{
    return resistanceFromKelvin(celsiusToKelvin(celsius));
}

const double ThermistorCalc::kelvinFromResistance(double resistance)
{
    double lnR = log(resistance);
    return 1 / (_coefficientA + _coefficientB * lnR + _coefficientC * pow(lnR, 3));
}

const double ThermistorCalc::celsiusFromResistance(double resistance)
{
    return kelvinToCelsius(kelvinFromResistance(resistance));
}

const double ThermistorCalc::celsiusToKelvin(double celsius)
{
    return celsius + OFFSET_KELVIN;
}

const double ThermistorCalc::kelvinToCelsius(double kelvin)
{
    return kelvin - OFFSET_KELVIN;
}