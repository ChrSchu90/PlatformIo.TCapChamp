/*
    Calculates the temperature or resistance of an thermistor
    based on the Steinhart–Hart equation (T in degrees Kelvin):
    1/T = a + b(Ln R) + c(Ln R)^3
 */
#pragma once

class Thermistor
{
private:
protected:
public:
    Thermistor(int cLow, int rLow, int cMid, int rMid, int cHigh, int rHigh);

    const double resistanceFromKelvin(double kelvin);

    const double resistanceFromCelsius(double celsius);

    const double kelvinFromResistance(double resistance);

    const double celsiusFromResistance(double resistance);

    static const double celsiusToKelvin(double celsius);

    static const double kelvinToCelsius(double kelvin);
};