#ifndef IO_HELPERS_H
#define IO_HELPERS_H

#include "basicIO.h"
#include <string>

// Helper functions to make basicIO easier to use
class IOHelpers {
public:
    // Print functions
    static void print(const char* text);
    static void print(const std::string& text);
    static void print(int value);
    static void print(long value);
    static void print(long long value);
    static void print(double value);
    static void println(const char* text);
    static void println(const std::string& text);
    static void println(int value);
    static void println(double value);
    static void printInt(int value);
    static void printDouble(double value, int precision = 2);
    static void printNewline();
    
    // Input functions
    static int readInt();
    static double readDouble();
    static void readString(char* buffer, int size);
    
    // Utility functions
    static void intToString(int value, char* buffer, int bufferSize);
    static void doubleToString(double value, char* buffer, int bufferSize, int precision);
};

#endif
