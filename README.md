# PlatformIo.TCapChamp
[![Build](https://github.com/ChrSchu90/PlatformIo.TCapChamp/actions/workflows/build.yml/badge.svg)](https://github.com/ChrSchu90/PlatformIo.TCapChamp/actions/workflows/build.yml)

## Description
Manipulates the measured temperature for heat pumps via ESP32 to adjust the cycle rate.

## ThermistorCalc
Calculates the temperature or resistance of an hermistor based on the [Steinhart–Hart equation](https://en.wikipedia.org/wiki/Steinhart%E2%80%93Hart_equation) (T in egrees Kelvin):

`1/T = a + b(Ln R) + c(Ln R)^3`

This implementation makes only the calculation without the need of linking any I/O!