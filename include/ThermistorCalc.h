#pragma once

/**
 * @brief Calculates the temperature or resistance of an thermistor
 * based on the Steinhart–Hart equation (T in degrees Kelvin):
 * 1/T = a + b(Ln R) + c(Ln R)^3
 */
class ThermistorCalc
{
private:
    double _coefficientA;
    double _coefficientB;
    double _coefficientC;
protected:
public:
    /// @brief Creates an instance of the calculator
    /// @param cLow Low reference temperature in Celsius
    /// @param rLow Low reference resistance on Ohm
    /// @param cMid Mid reference temperature in Celsius
    /// @param rMid  Mid reference resistance on Ohm
    /// @param cHigh High reference temperature in Celsius
    /// @param rHigh High reference resistance on Ohm
    ThermistorCalc(int cLow, int rLow, int cMid, int rMid, int cHigh, int rHigh);

    /// @brief Calculates the resistance [Ohm] of the given temperature [kelvin]
    /// @param kelvin Temperature [kelvin]
    /// @return The calculated resistane [Ohm]
    const double resistanceFromKelvin(double kelvin);

    /// @brief Calculates the resistance [Ohm] of the given temperature [celsius]
    /// @param celsius Temperature in [celsius]
    /// @return The calculated resistane [Ohm]
    const double resistanceFromCelsius(double celsius);

    /// @brief Calculates the temperature [kelvin] by the given resistance [Ohm]
    /// @param resistance The resistance [Ohm]
    /// @return The calculated temperature [kelvin]
    const double kelvinFromResistance(double resistance);

    /// @brief Calculates the temperature [celsius] by the given resistance [Ohm]
    /// @param resistance The resistance [Ohm]
    /// @return The temperature [celsius]
    const double celsiusFromResistance(double resistance);

    /// @brief Converts the temperature [celsius] into temperature [kelvin]
    /// @param celsius The temperature in [celsius]
    /// @return The temperature in [kelvin]
    static const double celsiusToKelvin(double celsius);

    /// @brief Converts the temperature [kelvin] into temperature [celsius]
    /// @param kelvin The temperature in [kelvin]
    /// @return The temperature in [celsius]
    static const double kelvinToCelsius(double kelvin);
};