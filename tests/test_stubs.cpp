#include <iostream>
#include <string>
#include <cstring>

#include "IOHelpers.h"

// Minimal IOHelpers implementations for unit tests (avoid linking project basicIO/syscall)

void IOHelpers::print(const char* text) {
    std::cout << text;
}

void IOHelpers::print(const std::string& text) {
    std::cout << text;
}

void IOHelpers::println(const char* text) {
    std::cout << text << std::endl;
}

void IOHelpers::println(const std::string& text) {
    std::cout << text << std::endl;
}

void IOHelpers::print(int value) {
    std::cout << value;
}

void IOHelpers::print(long value) {
    std::cout << value;
}

void IOHelpers::print(long long value) {
    std::cout << value;
}

void IOHelpers::print(double value) {
    std::cout << value;
}

void IOHelpers::println(int value) {
    std::cout << value << std::endl;
}

void IOHelpers::println(double value) {
    std::cout << value << std::endl;
}

void IOHelpers::printInt(int value) {
    std::cout << value;
}

void IOHelpers::printDouble(double value, int precision) {
    std::cout.setf(std::ios::fixed); std::cout.precision(precision);
    std::cout << value;
}

void IOHelpers::printNewline() {
    std::cout << std::endl;
}

int IOHelpers::readInt() {
    int v; if(!(std::cin >> v)) return 0; return v;
}

double IOHelpers::readDouble() {
    double d; if(!(std::cin >> d)) return 0.0; return d;
}

void IOHelpers::readString(char* buffer, int size) {
    std::string s; std::getline(std::cin, s);
    strncpy(buffer, s.c_str(), size-1); buffer[size-1] = '\0';
}

void IOHelpers::intToString(int value, char* buffer, int bufferSize) {
    snprintf(buffer, bufferSize, "%d", value);
}

void IOHelpers::doubleToString(double value, char* buffer, int bufferSize, int precision) {
    snprintf(buffer, bufferSize, "%.*f", precision, value);
}
