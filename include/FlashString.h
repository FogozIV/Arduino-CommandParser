//
// Created by fogoz on 11/05/2025.
//

#ifndef FLASHSTRING_H
#define FLASHSTRING_H

#include <memory>

#include "Arduino.h"

// Store the string in PROGMEM properly
template<size_t N>
class LowercaseFlash {
public:
    LowercaseFlash(const char (&input)[N]) {
        for (size_t i = 0; i < N; i++) {
            data[i] = tolower(input[i]);
        }
    }

    const char* get() const { return data; }
    size_t length() const { return N; }

    char data[N];
};

// Macro that properly handles PROGMEM strings
#define F_LOWER(str) ([]() -> const char* { \
static DMAMEM LowercaseFlash<sizeof(str)> lower_instance(str); \
return (&lower_instance.data[0]); \
})()

class FlashString {
private:
    enum MemoryState {
        UNIQUE_PTR,
        PROG_MEM,
        STATIC_CHAR,
        BUFFER_PROGMEM,
        BUFFER_STATIC_CHAR
    };
    const char *str = nullptr;
    MemoryState state = MemoryState::STATIC_CHAR;
    mutable std::unique_ptr<String> ram_str;
public:
    FlashString(const char *str);

    FlashString(const __FlashStringHelper *str);

    FlashString(const String &str);

    FlashString(const FlashString& flashString);

    void loadBuffer();

    void unloadBuffer();

    const char *c_str() const;

    size_t size() const;

    bool empty() const;

    FlashString substr(size_t pos, int pos_end = -1) const;

    bool operator==(const FlashString& other) const;

    bool operator==(const String& other) const;

    bool operator==(const __FlashStringHelper* other) const;

    bool operator!=(const FlashString& other) const;

    FlashString operator+(const FlashString& other) const;

    FlashString& operator=(const FlashString& other);

    operator String() const;

    char operator[](size_t index) const;
};

FlashString operator+(const __FlashStringHelper *str, const FlashString& rhs);

String operator+(const __FlashStringHelper *str, const String &rhs);


int findLastNotOf(const String &str, const String &charsToIgnore, int fromIndex = -1);

int findFirstOf(const String &str, const String &charsToFind, int fromIndex = 0);

int findFirstNotOf(const String &str, const String &charsToIgnore, int fromIndex = 0);

int findLastOf(const String &str, const String &charsToFind, int fromIndex = -1);

#endif //FLASHSTRING_H
