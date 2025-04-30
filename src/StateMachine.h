//
// Created by fogoz on 30/04/2025.
//

#ifndef PAMITEENSY_STATEMACHINE_H
#define PAMITEENSY_STATEMACHINE_H
#include "Arduino.h"
#include "vector"

#define UP_LAST_CHAR 'A'
#define DOWN_LAST_CHAR 'B'
#define RIGHT_LAST_CHAR 'C'
#define LEFT_LAST_CHAR 'D'


class StateMachine {
    bool started = false;
    std::vector<uint8_t> chars;
    std::vector<std::function<void()>> functions;

public:
    StateMachine() : functions(26, nullptr) {

    }

    void set(char c, std::function<void()> fct){
        functions[c-'A'] = fct;
    }

    void begin() {
        chars.clear();
        started = true;
    }

    bool isStarted() {
        return started;
    }

    std::vector<uint8_t> append(char c) {
        if (chars.empty() && c == 91) {
            chars.push_back(c);
            return {};
        }
        if (chars.size() == 1 && chars[0] == '[') {
            if(c>=65 && c <= 90){
                auto a = functions[c-'A'];
                if(a){
                    a();
                }
                chars.clear();
                started = false;
            }
        }
        std::vector<uint8_t> result = chars;
        chars.clear();
        started = false;
        return result;

    }
};
#endif //PAMITEENSY_STATEMACHINE_H
