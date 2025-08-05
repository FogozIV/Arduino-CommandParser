//
// Created by fogoz on 30/04/2025.
//

#ifndef PAMITEENSY_ROUNDARRAY_H
#define PAMITEENSY_ROUNDARRAY_H
#include "Arduino.h"
#include "vector"
#include "string"

class RoundArray {
    std::vector<std::string> strs;
    int index = 0;
    int lookingIndex = 0;
    int max_size;
    bool block_if_empty;
public:
    RoundArray(int max_size = 10, bool block_if_empty=true) : strs(max_size, ""), max_size(max_size), block_if_empty(block_if_empty) {

    }

    void add(const std::string &str) {
        std::string cmd = str;
        cmd.erase(cmd.find_last_not_of(" \n\r\t") + 1);
        if (strs[(index + max_size - 1) % max_size] == cmd) {
            return;
        }
        strs[index] = cmd;
        index = (index + 1) % max_size;
        lookingIndex = index;
    }

    const std::string &go_up() {
        lookingIndex = (lookingIndex + max_size - 1) % max_size;
        if (strs[lookingIndex] == "" && block_if_empty) {
            lookingIndex = (lookingIndex + max_size + 1) % max_size;
        }
        return strs[lookingIndex];
    }

    const std::string &go_down() {
        lookingIndex = (lookingIndex + max_size + 1) % max_size;
        if (strs[lookingIndex] == "" && block_if_empty) {
            lookingIndex = (lookingIndex + max_size - 1) % max_size;
        }
        return strs[lookingIndex];
    }

    void goto_last() {
        lookingIndex = index;
    }

};
#endif //PAMITEENSY_ROUNDARRAY_H
