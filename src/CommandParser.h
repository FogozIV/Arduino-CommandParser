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

#define MAX_RESPONSE_SIZE 128

// Template to convert strings to integers with error handling
// Supports both signed and unsigned integers
template<typename T>
size_t strToInt(const char* buf, T* value, T min_value, T max_value) {
    size_t position = 0;
    bool isNegative = false;

    if (min_value < 0 && (buf[position] == '+' || buf[position] == '-')) {
        isNegative = (buf[position] == '-');
        position++;
    }

    int base = 10;
    if (buf[position] == '0') {
        if (buf[position + 1] == 'b') { base = 2; position += 2; }
        else if (buf[position + 1] == 'o') { base = 8; position += 2; }
        else if (buf[position + 1] == 'x') { base = 16; position += 2; }
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
        Argument(const std::string& s) : value(s) {}

        double asDouble() const { return std::get<double>(value); }
        uint64_t asUInt64() const { return std::get<uint64_t>(value); }
        int64_t asInt64() const { return std::get<int64_t>(value); }
        const std::string& asString() const { return std::get<std::string>(value); }
    };

private:
    struct Command {
        std::string name;
        std::string argTypes;
        std::function<std::string(std::vector<Argument>)> callback;

        Command(const std::string& name, const std::string& argTypes, std::function<std::string(std::vector<Argument>)> callback)
            : name(name), argTypes(argTypes), callback(callback) {}
    };

    std::vector<Argument> commandArgs;
    std::vector<Command> commandDefinitions;

    size_t parseString(const char* buf, std::string& output) {
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
    bool registerCommand(const std::string& name, const std::string& argTypes, std::function<std::string(std::vector<Argument>)> callback) {
        for (char type : argTypes) {
            if (type != 'd' && type != 'u' && type != 'i' && type != 's') return false;
        }
        commandDefinitions.emplace_back(name, argTypes, callback);
        return true;
    }

    bool processCommand(const std::string& commandStr, std::string& response) {
        std::string command = commandStr;
        std::string name = command.substr(0, command.find(' '));
        command.erase(0, command.find(' ') + 1);

        auto it = std::find_if(commandDefinitions.begin(), commandDefinitions.end(),
                               [&name](const Command& cmd) { return cmd.name == name; });

        if (it == commandDefinitions.end()) {
            response = "Error: Unknown command.";
            return false;
        }

        const std::string& argTypes = it->argTypes;
        commandArgs.clear();

        for (char argType : argTypes) {
            Argument arg;
            switch (argType) {
                case 'd': {
                    char* end;
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
                    size_t bytesRead = strToInt<uint64_t>(command.c_str(), &value, 0, std::numeric_limits<uint64_t>::max());
                    if (bytesRead == 0) {
                        response = "Error: Invalid unsigned integer argument.";
                        return false;
                    }
                    arg = Argument(value);
                    command.erase(0, bytesRead);
                    break;
                }
                case 'i': {
                    int64_t value;
                    size_t bytesRead = strToInt(command.c_str(), &value, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max());
                    if (bytesRead == 0) {
                        response = "Error: Invalid integer argument.";
                        return false;
                    }
                    arg = Argument(value);
                    command.erase(0, bytesRead);
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
};

#endif
