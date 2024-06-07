#pragma once

#define ERROR 4
#define WARNING 3
#define INFO 2
#define DEBUG 1
#define NONE 0

#ifndef LOG_LEVEL
    #define LOG_LEVEL NONE
#endif

#if LOG_LEVEL == DEBUG
    #define LOG_DEBUG(cl, fn, msg) Serial.println(String(F("DEBUG\t[")) + cl + String(F(".")) + fn + String(F("] ")) + msg)
#endif

#if LOG_LEVEL == DEBUG || LOG_LEVEL == INFO
    #define LOG_INFO(cl, fn, msg) Serial.println(String(F("INFO\t[")) + cl + String(F(".")) + fn + String(F("] ")) + msg)
#endif

#if LOG_LEVEL == DEBUG || LOG_LEVEL == INFO || LOG_LEVEL == WARNING
    #define LOG_WARNING(cl, fn, msg) Serial.println(String(F("WARNING\t[")) + cl + String(F(".")) + fn + String(F("] ")) + msg)
#endif

#if LOG_LEVEL == DEBUG || LOG_LEVEL == INFO || LOG_LEVEL == WARNING || LOG_LEVEL == ERROR
    #define LOG_ERROR(cl, fn, msg) Serial.println(String(F("ERROR\t[")) + cl + String(F(".")) + fn + String(F("] ")) + msg)
#endif