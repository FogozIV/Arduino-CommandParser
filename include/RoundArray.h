//
// Created by fogoz on 30/04/2025.
//

#ifndef PAMITEENSY_ROUNDARRAY_H
#define PAMITEENSY_ROUNDARRAY_H
#include "Arduino.h"
#include "FlashString.h"
#include "vector"
#include "string"

class RoundArray {
    std::vector<String> strs;
    int index = 0;
    int lookingIndex = 0;
    int max_size;
    bool block_if_empty;
public:
    RoundArray(int max_size = 10, bool block_if_empty=true) : strs(max_size, ""), max_size(max_size), block_if_empty(block_if_empty) {

    }

    void add(const String &str) {
        String cmd = str;
        cmd.remove(findLastNotOf(cmd, " \n\r\t") + 1);
        if (strs[(index + max_size - 1) % max_size] == cmd) {
            return;
        }
        strs[index] = cmd;
        index = (index + 1) % max_size;
        lookingIndex = index;
    }

    const String &go_up() {
        lookingIndex = (lookingIndex + max_size - 1) % max_size;
        if (strs[lookingIndex] == "" && block_if_empty) {
            lookingIndex = (lookingIndex + max_size + 1) % max_size;
        }
        return strs[lookingIndex];
    }

    const String &go_down() {
        lookingIndex = (lookingIndex + max_size + 1) % max_size;
        if (strs[lookingIndex] == "" && block_if_empty) {
            lookingIndex = (lookingIndex + max_size - 1) % max_size;
        }
        return strs[lookingIndex];
    }

};
#endif //PAMITEENSY_ROUNDARRAY_H
