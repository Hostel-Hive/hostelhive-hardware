#pragma once
#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <string>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <iostream>

// Lightweight Arduino String compatibility shim for native x86/host unit testing
class String : public std::string {
public:
    String() : std::string() {}
    String(const char* s) : std::string(s ? s : "") {}
    String(const std::string& s) : std::string(s) {}
    String(int v) : std::string(std::to_string(v)) {}
    String(unsigned long v) : std::string(std::to_string(v)) {}

    void trim() {
        size_t first = find_first_not_of(" \t\n\r");
        if (first == std::string::npos) {
            clear();
            return;
        }
        size_t last = find_last_not_of(" \t\n\r");
        *this = substr(first, (last - first + 1));
    }

    void replace(const std::string& from, const std::string& to) {
        if (from.empty()) return;
        size_t pos = 0;
        while ((pos = find(from, pos)) != std::string::npos) {
            std::string::replace(pos, from.length(), to);
            pos += to.length();
        }
    }

    bool startsWith(const std::string& prefix) const {
        return rfind(prefix, 0) == 0;
    }

    int indexOf(const std::string& target) const {
        size_t pos = find(target);
        return (pos == std::string::npos) ? -1 : (int)pos;
    }

    int indexOf(const char* target) const {
        return indexOf(std::string(target));
    }
};

inline void delay(unsigned long ms) {}
#endif

#endif // TEST_COMMON_H
