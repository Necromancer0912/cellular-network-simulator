#include "IOHelpers.h"
#include <cmath>

extern basicIO io;

void IOHelpers::print(const char* text) {
    io.outputstring(text);
}

void IOHelpers::println(const char* text) {
    io.outputstring(text);
    io.terminate();
}

void IOHelpers::print(int value) {
    io.outputint(value);
}

void IOHelpers::print(long value) {
    io.outputint(value);
}

void IOHelpers::print(long long value) {
    if (value == 0) {
        io.outputstring("0");
        return;
    }
    
    char buffer[32];
    char temp[32];
    int i = 0;
    bool isNegative = false;
    
    if (value < 0) {
        isNegative = true;
        value = -value;
    }
    
    while (value > 0) {
        temp[i++] = (value % 10) + '0';
        value /= 10;
    }
    
    if (isNegative) temp[i++] = '-';
    
    int j = 0;
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';
    
    io.outputstring(buffer);
}

void IOHelpers::print(double value) {
    printDouble(value);
}

void IOHelpers::println(int value) {
    io.outputint(value);
    io.terminate();
}

void IOHelpers::println(double value) {
    printDouble(value);
    io.terminate();
}

void IOHelpers::printInt(int value) {
    io.outputint(value);
}

void IOHelpers::printNewline() {
    io.terminate();
}

void IOHelpers::print(const std::string& text) {
    io.outputstring(text.c_str());
}

void IOHelpers::println(const std::string& text) {
    io.outputstring(text.c_str());
    io.terminate();
}

void IOHelpers::intToString(int value, char* buffer, int bufferSize) {
    int i = 0;
    bool isNegative = false;
    
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    
    if (value < 0) {
        isNegative = true;
        value = -value;
    }
    
    // Convert digits in reverse
    while (value > 0 && i < bufferSize - 1) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    if (isNegative && i < bufferSize - 1) {
        buffer[i++] = '-';
    }
    
    buffer[i] = '\0';
    
    // Reverse the string
    for (int j = 0; j < i / 2; j++) {
        char temp = buffer[j];
        buffer[j] = buffer[i - 1 - j];
        buffer[i - 1 - j] = temp;
    }
}

void IOHelpers::doubleToString(double value, char* buffer, int bufferSize, int precision) {
    int intPart = (int)value;
    double fracPart = value - intPart;
    
    if (fracPart < 0) fracPart = -fracPart;
    
    // Convert integer part
    char intBuffer[32];
    intToString(intPart, intBuffer, 32);
    
    int i = 0;
    while (intBuffer[i] && i < bufferSize - 1) {
        buffer[i] = intBuffer[i];
        i++;
    }
    
    // Add decimal point
    if (i < bufferSize - 1 && precision > 0) {
        buffer[i++] = '.';
        
        // Add fractional part
        for (int p = 0; p < precision && i < bufferSize - 1; p++) {
            fracPart *= 10;
            int digit = (int)fracPart;
            buffer[i++] = '0' + digit;
            fracPart -= digit;
        }
    }
    
    buffer[i] = '\0';
}

void IOHelpers::printDouble(double value, int precision) {
    char buffer[64];
    doubleToString(value, buffer, 64, precision);
    io.outputstring(buffer);
}

int IOHelpers::readInt() {
    return io.inputint();
}

double IOHelpers::readDouble() {
    char buffer[32];
    io.inputstring(buffer, 32);
    try {
        return std::stod(buffer);
    } catch (...) {
        return 0.0;
    }
}

void IOHelpers::readString(char* buffer, int size) {
    io.inputstring(buffer, size);
}
