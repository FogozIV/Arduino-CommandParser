//
// Created by fogoz on 11/05/2025.
//

#include "FlashString.h"


FlashString::FlashString(const char *str) {
    this->str = str;
}

FlashString::FlashString(const __FlashStringHelper *str) {
    Serial.println(reinterpret_cast<uintptr_t>(str), HEX);
    this->str = reinterpret_cast<const char *>(str);
    state = MemoryState::PROG_MEM;
}

FlashString::FlashString(const String &str) {
    this->ram_str = std::make_unique<String>(str);
    state = MemoryState::UNIQUE_PTR;
}

FlashString::FlashString(const FlashString &flashString) {
    this->str = flashString.str;
    switch (state) {
        case BUFFER_PROGMEM:
            this->state = PROG_MEM;
            break;
        case BUFFER_STATIC_CHAR:
            this->state = STATIC_CHAR;
            break;
        default:
            this->state = flashString.state;
            break;
    }
    if (this->state == MemoryState::UNIQUE_PTR) {
        this->ram_str = std::make_unique<String>(*flashString.ram_str);
    }
}

void FlashString::loadBuffer() {
    if (state == UNIQUE_PTR || state == BUFFER_STATIC_CHAR || state == BUFFER_PROGMEM) {
        return;
    }
    this->ram_str = std::make_unique<String>(c_str());
    if (state == PROG_MEM) {
        state = BUFFER_PROGMEM;
    }else if (state == STATIC_CHAR) {
        state = BUFFER_STATIC_CHAR;
    }
}

void FlashString::unloadBuffer() {
    if (state == UNIQUE_PTR || state == STATIC_CHAR || state == PROG_MEM) {
        return;
    }
    if (state == BUFFER_STATIC_CHAR) {
        state = STATIC_CHAR;
    }else if (state == BUFFER_PROGMEM) {
        state = PROG_MEM;
    }
    ram_str.reset();
}


const char * FlashString::c_str() const {
    switch (state) {
        case BUFFER_STATIC_CHAR:
        case BUFFER_PROGMEM:
        case UNIQUE_PTR:
            return ram_str->c_str();
        case PROG_MEM:
            return this->str;
        case STATIC_CHAR:
            return this->str;
    }
    return "";
}

size_t FlashString::size() const {
    switch (state) {
        case BUFFER_STATIC_CHAR:
        case BUFFER_PROGMEM:
        case UNIQUE_PTR:
            return ram_str->length();
        case PROG_MEM:
            return strlen_P(this->str);
        case STATIC_CHAR:
            return strlen(this->str);
    }
    return 0;
}

bool FlashString::empty() const {
    return size() == 0;
}

FlashString FlashString::substr(size_t pos, int pos_end) const {
    return FlashString(String(c_str()).substring(pos, pos_end));
}

bool FlashString::operator==(const FlashString &other) const {
    return strcmp(c_str(), other.c_str()) == 0;
}

bool FlashString::operator==(const String &other) const {
    return strcmp(c_str(), other.c_str()) == 0;
}

bool FlashString::operator==(const __FlashStringHelper *other) const {
    return strcmp(c_str(), reinterpret_cast<const char*>(other));
}

bool FlashString::operator!=(const FlashString &other) const {
    return strcmp(c_str(), other.c_str()) != 0;
}

FlashString FlashString::operator+(const FlashString &other) const {
    String str = String(this->c_str());
    str += String(other.c_str());

    return {str};
}

FlashString & FlashString::operator=(const FlashString &other) {
    this->str = other.str;
    this->state = other.state;
    if (this->state == MemoryState::UNIQUE_PTR) {
        this->ram_str = std::make_unique<String>(*other.ram_str);
    }
    switch (this->state) {
        case BUFFER_PROGMEM:
            this->state = PROG_MEM;
            break;
        case BUFFER_STATIC_CHAR:
            this->state = STATIC_CHAR;
            break;
        default: break;
    }
    return *this;
}

FlashString::operator String() const {
    return String(c_str());
}

char FlashString::operator[](size_t index) const {
    return c_str()[index];
}

FlashString operator+(const __FlashStringHelper *str, const FlashString &rhs) {
    return FlashString(str) + rhs;
}

String operator+(const __FlashStringHelper *str, const String &rhs) {
    String new_str(str);
    new_str += rhs;
    return new_str;
}

int findLastNotOf(const String &str, const String &charsToIgnore, int fromIndex) {
    if (fromIndex < 0) fromIndex = str.length() - 1; // default to last character

    for (int i = fromIndex; i >= 0; i--) {
        if (charsToIgnore.indexOf(str.charAt(i)) == -1) {
            return i; // found a character not in charsToIgnore
        }
    }
    return -1; // not found
}

int findFirstOf(const String &str, const String &charsToFind, int fromIndex) {
    for (int i = fromIndex; i < str.length(); i++) {
        if (charsToFind.indexOf(str.charAt(i)) >= 0) {
            return i; // found a matching character
        }
    }
    return -1; // not found
}

int findFirstNotOf(const String &str, const String &charsToIgnore, int fromIndex) {
    for (int i = fromIndex; i < str.length(); i++) {
        if (charsToIgnore.indexOf(str.charAt(i)) == -1) {
            return i; // found a character not in the ignore list
        }
    }
    return -1; // all characters matched the ignore list
}

int findLastOf(const String &str, const String &charsToFind, int fromIndex) {
    if (fromIndex < 0) fromIndex = str.length() - 1;
    for (int i = fromIndex; i >= 0; i--) {
        if (charsToFind.indexOf(str.charAt(i)) >= 0) {
            return i; // found a matching character
        }
    }
    return -1; // not found
}
