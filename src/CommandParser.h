/*
  CommandParser.h - Library for parsing commands of the form "COMMAND_NAME ARG1 ARG2 ARG3 ...".
*/

#ifndef __COMMAND_PARSER_H__
#define __COMMAND_PARSER_H__

#include <limits>
#include <string>
#include <variant>
#include <vector>
#include <functional>
#include <cctype>
#include <cstdio>
#include <algorithm>
#include "RoundArray.h"
#include "StateMachine.h"

#define MAX_RESPONSE_SIZE 128

#define TERMINAL_END_LINE_WITH_LINE_FEED 1
#define TERMINAL_END_LINE_WITH_CARRIAGE_RETURN 2
#define TERMINAL_END_LINE_WITH_BOTH 3
struct TerminalIdentifier{
    uint8_t type = 0;
    bool identified = false;
    bool identifying = false;
} identifier;
// Template to convert strings to integers with error handling
// Supports both signed and unsigned integers
template<typename T>
size_t strToInt(const char *buf, T *value, T min_value, T max_value) {
    size_t position = 0;
    bool isNegative = false;

    if (min_value < 0 && (buf[position] == '+' || buf[position] == '-')) {
        isNegative = (buf[position] == '-');
        position++;
    }

    int base = 10;
    if (buf[position] == '0') {
        if (buf[position + 1] == 'b') {
            base = 2;
            position += 2;
        }
        else if (buf[position + 1] == 'o') {
            base = 8;
            position += 2;
        }
        else if (buf[position + 1] == 'x') {
            base = 16;
            position += 2;
        }
    }

    T result = 0;
    while (true) {
        char c = buf[position];
        int digit = -1;
        if (std::isdigit(c)) {
            digit = c - '0';
        } else if (base == 16 && std::isalpha(c)) {
            digit = std::tolower(c) - 'a' + 10;
        }

        if (digit < 0 || digit >= base) break;
        if (result > (max_value - digit) / base) return 0;

        result = result * base + digit;
        position++;
    }

    if (isNegative) result = -result;
    if (result < min_value || result > max_value) return 0;

    *value = result;
    return position;
}

class CommandParser {
public:
    struct Argument {
        std::variant<double, uint64_t, int64_t, std::string> value;

        Argument() = default;

        Argument(double d) : value(d) {}

        Argument(uint64_t u) : value(u) {}

        Argument(int64_t i) : value(i) {}

        Argument(const std::string &s) : value(s) {}

        double asDouble() const { return std::get<double>(value); }

        uint64_t asUInt64() const { return std::get<uint64_t>(value); }

        int64_t asInt64() const { return std::get<int64_t>(value); }

        const std::string &asString() const { return std::get<std::string>(value); }
    };

    struct Command {
        std::string name;
        std::string argTypes;
        std::function<std::string(std::vector<Argument>)> callback;
        std::string description;

        Command(const std::string &name, const std::string &argTypes,
                std::function<std::string(std::vector<Argument>)> callback, std::string description)
                : name(name), argTypes(argTypes), callback(callback), description(description) {}
    };

private:
    std::vector<Argument> commandArgs;
    std::vector<Command> commandDefinitions;

    size_t parseString(const char *buf, std::string &output) {
        size_t readCount = 0;
        bool isQuoted = (buf[0] == '"');

        if (isQuoted) readCount++;

        while (buf[readCount] != '\0') {
            char c = buf[readCount++];
            if (isQuoted && c == '"') break;
            if (!isQuoted && std::isspace(c)) break;
            output.push_back(c);
        }

        return readCount;
    }

public:
    bool registerCommand(const std::string &name, const std::string &argTypes,
                         std::function<std::string(std::vector<Argument>)> callback, std::string description = "") {
        for (char type: argTypes) {
            if (type != 'd' && type != 'u' && type != 'i' && type != 's') return false;
        }
        std::string new_name;
        for (auto &c: name) {
            new_name += tolower(c);
        }
        commandDefinitions.emplace_back(new_name, argTypes, callback, description);
        return true;
    }

    bool processCommand(const std::string &commandStr, std::string &response) {
        std::string command = commandStr;
        command.erase(command.find_last_not_of(" \n\r\t") + 1);
        std::string name = command.substr(0, command.find(' '));
        command.erase(0, command.find(' ') + 1);

        auto it = std::find_if(commandDefinitions.begin(), commandDefinitions.end(),
                               [&name](const Command &cmd) {
                                   return cmd.name == name;
                               });


        if (it == commandDefinitions.end()) {
            response = "Error: Unknown command.";
            return false;
        }

        const std::string &argTypes = it->argTypes;
        commandArgs.clear();

        for (char argType: argTypes) {
            Argument arg;
            switch (argType) {
                case 'd': {
                    char *end;
                    double value = std::strtod(command.c_str(), &end);
                    if (end == command.c_str()) {
                        response = "Error: Invalid double argument.";
                        return false;
                    }
                    arg = Argument(value);
                    command.erase(0, end - command.c_str());
                    break;
                }
                case 'u': {
                    uint64_t value;
                    size_t bytesRead = strToInt<uint64_t>(command.c_str(), &value, 0,
                                                          std::numeric_limits<uint64_t>::max());
                    if (bytesRead == 0) {
                        response = "Error: Invalid unsigned integer argument.";
                        return false;
                    }
                    arg = Argument(value);
                    command.erase(0, bytesRead);
                    if (command.size() > 0) {
                        command.erase(0, 1);
                    }
                    break;
                }
                case 'i': {
                    int64_t value;
                    size_t bytesRead = strToInt(command.c_str(), &value, std::numeric_limits<int64_t>::min(),
                                                std::numeric_limits<int64_t>::max());
                    if (bytesRead == 0) {
                        response = "Error: Invalid integer argument.";
                        return false;
                    }
                    arg = Argument(value);
                    command.erase(0, bytesRead);
                    if (command.size() > 0) {
                        command.erase(0, 1);
                    }
                    break;
                }
                case 's': {
                    std::string value;
                    size_t bytesRead = parseString(command.c_str(), value);
                    if (bytesRead == 0) {
                        response = "Error: Invalid string argument.";
                        return false;
                    }
                    arg = Argument(value);
                    command.erase(0, bytesRead);
                    break;
                }
            }
            commandArgs.push_back(arg);
        }

        response = it->callback(commandArgs);
        return true;
    }

    [[nodiscard]] std::vector<Command> command_definitions() const {
        return commandDefinitions;
    }
};

inline void clearline() {
    if(identifier.type == TERMINAL_END_LINE_WITH_CARRIAGE_RETURN || identifier.type == TERMINAL_END_LINE_WITH_LINE_FEED){
        Serial.print("\r");
        std::string str(40, ' ');
        Serial.print(str.c_str());
        Serial.print("\r");
    }else{
        Serial.println();
    }

}

//source chatgpt
std::string longestCommonPrefix(const std::vector<std::string> &strs) {
    if (strs.empty()) return "";

    // Start by assuming the whole first string is the prefix
    std::string prefix = strs[0];

    for (size_t i = 1; i < strs.size(); ++i) {
        // Compare prefix with each string
        size_t j = 0;
        while (j < prefix.size() && j < strs[i].size() && prefix[j] == strs[i][j]) {
            ++j;
        }
        prefix = prefix.substr(0, j);

        if (prefix.empty()) break; // early exit if no common prefix
    }

    return prefix;
}


void setCommand(std::string& cmd, const std::string &a) {
    if (a != "") {
        clearline();
        cmd = a;
        Serial.print(a.c_str());
    }
}

[[noreturn]] inline void handle_commandline(void *command_parser) {
    auto *parser = static_cast<CommandParser *>(command_parser);
    std::string cmd;
    std::string response;
    RoundArray array;
    StateMachine stateMachine;
    TerminalIdentifier id;



    stateMachine.set(UP_LAST_CHAR, [&]{
        setCommand(cmd, array.go_up());
    });
    stateMachine.set(DOWN_LAST_CHAR, [&]{
        setCommand(cmd, array.go_down());
    });

    while (true) {
        while (Serial.available()) {
            char c = Serial.read();
            if (c == 27) {
                stateMachine.begin();
                continue;
            }
            if (stateMachine.isStarted()) {
                auto result = stateMachine.append(c);
                if (result.empty())
                    continue;
                for (auto a: result) {
                    Serial.print((char) a);
                }
            }
            if (c == 8) {
                if (!cmd.empty()) {
                    cmd.pop_back();
                    clearline();
                    Serial.print(cmd.c_str());
                }
            } else if (c == 9) {
                std::vector<CommandParser::Command> args;
                std::vector<std::string> argsStrings;
                for (auto &cmd_data: parser->command_definitions()) {
                    if (cmd_data.name.rfind(cmd, 0) == 0) {
                        args.push_back(cmd_data);
                        argsStrings.push_back(cmd_data.name);
                    }
                }
                if (args.empty()) {

                } else {
                    if (args.size() == 1) {
                        clearline();
                        cmd = args.front().name;
                        Serial.print(cmd.c_str());
                    } else {
                        Serial.println();
                        for (CommandParser::Command &cmd: args) {
                            Serial.println((cmd.name + ": " + (cmd.description.empty() ? "No description found"
                                                                                       : cmd.description)).c_str());
                        }
                        cmd = longestCommonPrefix(argsStrings);
                        Serial.print(cmd.c_str());
                    }
                }
            } else {

                if(identifier.identifying){
                    if(c == '\n') {
                        identifier.type = TERMINAL_END_LINE_WITH_BOTH;
                        identifier.identifying = false;
                        identifier.identified = true;
                        continue;
                    }else {
                        identifier.type = TERMINAL_END_LINE_WITH_CARRIAGE_RETURN;
                        identifier.identifying = false;
                        identifier.identified = true;
                    }
                }

                if(!identifier.identified && c=='\n'){
                    identifier.type = TERMINAL_END_LINE_WITH_LINE_FEED;
                    identifier.identified = true;
                    identifier.identifying = false;
                }
                cmd += c;
                Serial.write(c);
            }


            if ((c == '\r' && !identifier.identified) || (c == '\r' && identifier.type == TERMINAL_END_LINE_WITH_CARRIAGE_RETURN) || (c== '\n' && (identifier.type == TERMINAL_END_LINE_WITH_LINE_FEED || identifier.type == TERMINAL_END_LINE_WITH_BOTH))) {
                if(c=='\r'){
                    Serial.println();
                }
                if(identifier.identified && identifier.type == TERMINAL_END_LINE_WITH_LINE_FEED){
                    Serial.write('\r');
                }
                if(!identifier.identified){
                    identifier.identifying = true;
                }
                parser->processCommand(cmd, response);
                array.add(cmd);
                cmd = "";
                Serial.println(response.c_str());
            }
        }
        Threads::yield();
    }
}

#endif
