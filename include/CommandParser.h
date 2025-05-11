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
#include <memory>

#include "FlashString.h"
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
};
#define MATHOPS \
MATHOP(ADD, add) \
MATHOP(SUB, sub) \
MATHOP(MUL, mult) \
MATHOP(DIV, div) \
MATHOP(MOD, mod) \
MATHOP(POW, pow) \
MATHOP(SET, set) \
MATHOP(EMPTY, )
#define MATHOP(name, str) name,
enum MathOP {
    MATHOPS
    MathOPCount
};
#undef MATHOP
#define MATHOP(name, str) FlashString(F(#str)),

inline static FlashString mathOP_names[] = {
    MATHOPS
};

inline static const FlashString& mathOPToString(MathOP op) {
    if (op >= MathOP::MathOPCount || op < 0) return F("Unknown");
    return mathOP_names[static_cast<int>(op)];
}

inline static MathOP stringToMathOP(FlashString str) {
    for (int i = 0; i < MathOPCount; ++i) {
        FlashString str_name = mathOP_names[i];
        if (str_name == str)
            return static_cast<MathOP>(i);
    }
    return MathOPCount;
}

class DoubleRef {
public:
    virtual ~DoubleRef() = default;
    virtual double get() const = 0;
    virtual void set(double value) = 0;
};
template<typename T>
class DoubleRefImpl : public DoubleRef {
    T& ref;
public:
    DoubleRefImpl(T& ref) : ref(ref) {}
    double get() const override { return static_cast<double>(ref); }
    void set(double value) override { ref = static_cast<T>(value); }
};



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
        std::variant<double, uint64_t, int64_t, String> value;

        Argument() = default;

        Argument(double d) : value(d) {}

        Argument(uint64_t u) : value(u) {}

        Argument(int64_t i) : value(i) {}

        Argument(const String &s) : value(s) {}

        double asDouble() const { return std::get<double>(value); }

        uint64_t asUInt64() const { return std::get<uint64_t>(value); }

        int64_t asInt64() const { return std::get<int64_t>(value); }

        const String &asString() const { return std::get<String>(value); }

    };

    struct BaseCommand {
        FlashString name;
        FlashString description;
        BaseCommand(const FlashString name, const FlashString description) : name(name), description(description) {}
    };

    struct Command : public BaseCommand {
        FlashString argTypes;
        std::function<FlashString(std::vector<Argument>, Stream& stream)> callback;

        Command(const FlashString name, const FlashString &argTypes,
                std::function<FlashString(std::vector<Argument>, Stream& stream)> callback, FlashString description)
                : BaseCommand(name, description), argTypes(argTypes), callback(callback) {}
    };


    struct MathCommand : public BaseCommand {
        std::shared_ptr<DoubleRef> value;
        std::function<FlashString( Stream& stream, double value, MathOP op)> callback;
        template<typename T>
        MathCommand(const FlashString name, T& value,
                std::function<FlashString( Stream& stream, double value, MathOP op)> callback, FlashString description)
                    :BaseCommand(name, description), value(std::make_shared<DoubleRefImpl<T>>(value)),   callback(callback){}
    };

private:
    std::vector<Argument> commandArgs;
    std::vector<Command> commandDefinitions;
    std::vector<MathCommand> mathCommandDefinition;

    size_t parseString(const char *buf, String &output) {
        size_t readCount = 0;
        bool isQuoted = (buf[0] == '"');

        if (isQuoted) readCount++;

        while (buf[readCount] != '\0') {
            char c = buf[readCount++];
            if (isQuoted && c == '"') break;
            if (!isQuoted && std::isspace(c)) break;
            output += c;
        }

        return readCount;
    }

public:
    bool registerCommand(FlashString name, FlashString argTypes,
                         std::function<FlashString(std::vector<Argument>, Stream& stream)> callback, FlashString description = "") {
        for (char type: String(argTypes)) {
            if (type != 'd' && type != 'u' && type != 'i' && type != 's') return false;
        }
        commandDefinitions.emplace_back(name, argTypes, callback, description);
        return true;
    }
    template<typename T>
    bool registerMathCommand(FlashString name, T& value, std::function<FlashString(Stream& stream, double value, MathOP op)> callback, String description = "") {
        mathCommandDefinition.emplace_back(name, value, callback, description);
        return true;
    }


    std::tuple<std::vector<CommandParser::BaseCommand>, std::vector<FlashString>> tab_complete(String cmd) {
        std::vector<CommandParser::BaseCommand> args;
        std::vector<FlashString> argsStrings;
        for (auto&c : cmd) {
            c = tolower(c);
        }
        for (auto &cmd_data: command_definitions()) {
            if (String(cmd_data.name).indexOf(cmd, 0) == 0) {
                args.push_back(cmd_data);
                argsStrings.push_back(cmd_data.name);
            }
        }
        for (auto &cmd_data: mathCommandDefinition) {
            if (String(cmd_data.name).indexOf(cmd, 0) == 0) {
                args.push_back(cmd_data);
                argsStrings.push_back(String(cmd_data.name) + " ");
            }
        }
        if (args.empty()) {
            String command = cmd;
            auto of = findFirstOf(command, FlashString(F("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRTSUVWXYZ")));
            if (of == -1) {
                return {args, argsStrings};
            }
            command.remove(0, of);
            const auto empty_char_pos = findFirstOf(command,' ');
            if (empty_char_pos == -1) {
                return {args, argsStrings};
            }
            String name = command.substring(0, empty_char_pos);
            command.remove(0, empty_char_pos);
            command.remove(0, findFirstNotOf(command, " \n\r\t"));
            auto it_math = std::find_if(mathCommandDefinition.begin(), mathCommandDefinition.end(),[&name](const MathCommand& cmd){ return cmd.name == name;});
            if (it_math == mathCommandDefinition.end()) {
                return {args, argsStrings};
            }
            for (auto &cmd_data : mathOP_names) {
                if (cmd_data == F(""))
                    continue;
                if (String(cmd_data).indexOf(command, 0) == 0) {
                    argsStrings.push_back(it_math->name + F(" ") + cmd_data);
                    args.push_back({it_math->name + F(" ") + cmd_data, F("Using the command ") + it_math->name + F(" ") + cmd_data + F(" to modify the value of ") + it_math->name});
                }
            }
        }
        return {args, argsStrings};
    }

    bool processCommand(const String &commandStr, FlashString &response, Stream& stream) {
        String command = commandStr;
        for (auto&c : command) {
            c = tolower(c);
        }
        command.remove(findLastNotOf(command, " \n\r\t") + 1);
        command.remove(0, findFirstOf(command, FlashString(F("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRTSUVWXYZ"))));
        const auto empty_char_pos = findFirstOf(command, ' ');
        String name = command.substring(0, empty_char_pos);
        if (empty_char_pos == -1) {
            command.remove(0, command.length());
        }else {
            command.remove(0, empty_char_pos + 1);
        }
        auto pos = findLastNotOf(command, " \n\r\t");
        if (pos != 0 && pos != -1) {
            command.remove(0, pos);
        }

        auto it = std::find_if(commandDefinitions.begin(), commandDefinitions.end(),
                               [&name](const Command &cmd) {
                                   return cmd.name == name;
                               });

        if (it == commandDefinitions.end()) {
            auto it_math = std::find_if(mathCommandDefinition.begin(), mathCommandDefinition.end(),[&name](const MathCommand& cmd){ return cmd.name == name;});
            if (it_math == mathCommandDefinition.end()) {
                response = F("Error: Unknown command.");
                return false;
            }
            command.remove(findLastNotOf(command, " \n\r\t") + 1);
            if (command.length() == 0) {
                response = it_math->callback(stream, (it_math->value)->get(), MathOP::EMPTY);
                return true;
            }
            const auto e_c_p = findFirstOf(command,' ');
            if (e_c_p == -1) {
                response = F("Error: Invalid math command please add value.");
                return false;
            }
            String math_command = command.substring(0, e_c_p);
            if (e_c_p == -1) {
                response = F("Error: Invalid math command.");
                return false;
            }
            command.remove(0, e_c_p + 1);
            char *end;
            double value = std::strtod(command.c_str(), &end);
            if (end == command.c_str()) {
                response = F("Error: Invalid double argument.");
                return false;
            }
            auto mathOp = stringToMathOP(math_command);

            switch (mathOp) {
                case MathOP::ADD: {
                    it_math->value->set(it_math->value->get() + value);
                    //it_math->value += value;
                    break;
                }
                case MathOP::SUB: {
                    it_math->value->set(it_math->value->get() - value);
                    //it_math->value -= value;
                    break;
                }
                case MathOP::MUL: {
                    it_math->value->set(it_math->value->get() * value);
                    //it_math->value *= value;
                    break;
                }
                case MathOP::DIV: {
                    it_math->value->set(it_math->value->get() / value);
                    //it_math->value /= value;
                    break;
                }
                case MathOP::MOD: {
                    it_math->value->set(fmod(it_math->value->get(), value));
                    //it_math->value = fmod(it_math->value, value);
                    break;
                }
                case MathOP::POW: {
                    it_math->value->set(pow(it_math->value->get(), value));
                    // it_math->value = pow(it_math->value, value);
                    break;
                }
                case MathOP::SET: {
                    it_math->value->set(value);
                    // it_math->value = value;
                    break;
                }
                default: {
                    response = F("Unknown operator ! ") + math_command;
                    return false;
                }
            }
            response = it_math->callback(stream, it_math->value->get(), mathOp);
            return true;
        }

        const String &argTypes = it->argTypes;
        commandArgs.clear();

        for (char argType: argTypes) {
            Argument arg;
            switch (argType) {
                case 'd': {
                    char *end;
                    double value = std::strtod(command.c_str(), &end);
                    if (end == command.c_str()) {
                        response = F("Error: Invalid double argument.");
                        return false;
                    }
                    arg = Argument(value);
                    command.remove(0, end - command.c_str());
                    break;
                }
                case 'u': {
                    uint64_t value;
                    size_t bytesRead = strToInt<uint64_t>(command.c_str(), &value, 0,
                                                          std::numeric_limits<uint64_t>::max());
                    if (bytesRead == 0) {
                        response = F("Error: Invalid unsigned integer argument.");
                        return false;
                    }
                    arg = Argument(value);
                    command.remove(0, bytesRead);
                    if (command.length() > 0) {
                        command.remove(0, 1);
                    }
                    break;
                }
                case 'i': {
                    int64_t value;
                    size_t bytesRead = strToInt(command.c_str(), &value, std::numeric_limits<int64_t>::min(),
                                                std::numeric_limits<int64_t>::max());
                    if (bytesRead == 0) {
                        response = F("Error: Invalid integer argument.");
                        return false;
                    }
                    arg = Argument(value);
                    command.remove(0, bytesRead);
                    if (command.length() > 0) {
                        command.remove(0, 1);
                    }
                    break;
                }
                case 's': {
                    String value;
                    size_t bytesRead = parseString(command.c_str(), value);
                    if (bytesRead == 0) {
                        response = F("Error: Invalid string argument.");
                        return false;
                    }
                    arg = Argument(value);
                    command.remove(0, bytesRead);
                    break;
                }
            }
            commandArgs.push_back(arg);
        }
        command.remove(0, findFirstNotOf(command, " \t")); // Trim whitespace
        if (command.length() != 0) {
            response = F("Error: Too many arguments provided.");
            return false;
        }

        response = it->callback(commandArgs, stream);
        return true;
    }

    [[nodiscard]] std::vector<Command> command_definitions() const {
        return commandDefinitions;
    }
};

inline void clearline(Stream& stream, const TerminalIdentifier& identifier) {
    if(identifier.type == TERMINAL_END_LINE_WITH_CARRIAGE_RETURN || identifier.type == TERMINAL_END_LINE_WITH_LINE_FEED){
        stream.print("\r");
        String str(40, ' ');
        stream.print(str.c_str());
        stream.print("\r");
    }else{
        stream.println();
    }

}

//source chatgpt
inline String longestCommonPrefix(const std::vector<FlashString> &strs) {
    if (strs.empty()) return "";
    // Start by assuming the whole first string is the prefix
    FlashString prefix = strs[0];

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


inline void setCommand(String& cmd, const String &a, Stream& stream, const TerminalIdentifier& identifier) {
    if (a != "") {
        clearline(stream, identifier);
        cmd = a;
        stream.print(a.c_str());
    }
}



class CommandLineHandler {
    String cmd;
    FlashString response =F("");
    RoundArray array;
    StateMachine stateMachine;
    TerminalIdentifier id;
    CommandParser& parser;
    Stream& stream;

public:
    CommandLineHandler(CommandParser& parser, Stream& stream): parser(parser), stream(stream) {
        stateMachine.set(UP_LAST_CHAR, [&]{
            setCommand(cmd, array.go_up(), stream, id);
        });
        stateMachine.set(DOWN_LAST_CHAR, [&]{
            setCommand(cmd, array.go_down(), stream, id);
        });

    }
    void handle_commandline() {


        while (stream.available()) {
            char c = stream.read();
            if (c == 27) {
                stateMachine.begin();
                continue;
            }
            if (stateMachine.isStarted()) {
                auto result = stateMachine.append(c);
                if (result.empty())
                    continue;
                for (auto a: result) {
                    stream.print((char) a);
                }
            }
            if (c == 8) {
                if (cmd != 0) {
                    cmd.remove(cmd.length() - 1);
                    clearline(stream, id);
                    stream.print(cmd.c_str());
                }
            } else if (c == 9) {
                auto [args, argsStrings]= parser.tab_complete(cmd);
                if (args.empty()) {

                } else {
                    if (args.size() == 1) {
                        clearline(stream, id);
                        cmd = argsStrings.front();
                        stream.print(cmd.c_str());
                    } else {
                        stream.println();
                        for (auto &cmd: args) {
                            auto a = (cmd.name + F(": ") + (cmd.description.empty() ? FlashString(F("No description found"))
                                                                                       : cmd.description));
                            stream.println(String(a));
                        }
                        cmd = longestCommonPrefix(argsStrings);
                        stream.print(cmd.c_str());
                    }
                }
            } else {
                if(id.identifying){
                    if(c == '\n') {
                        id.type = TERMINAL_END_LINE_WITH_BOTH;
                        id.identifying = false;
                        id.identified = true;
                        continue;
                    }else {
                        id.type = TERMINAL_END_LINE_WITH_CARRIAGE_RETURN;
                        id.identifying = false;
                        id.identified = true;
                    }
                }

                if(!id.identified && c=='\n'){
                    id.type = TERMINAL_END_LINE_WITH_LINE_FEED;
                    id.identified = true;
                    id.identifying = false;
                }
                cmd += c;
                stream.write(c);
            }


            if ((c == '\r' && !id.identified) || (c == '\r' && id.type == TERMINAL_END_LINE_WITH_CARRIAGE_RETURN) || (c== '\n' && (id.type == TERMINAL_END_LINE_WITH_LINE_FEED || id.type == TERMINAL_END_LINE_WITH_BOTH))) {
                if(c=='\r'){
                    stream.println();
                }
                if(id.identified && id.type == TERMINAL_END_LINE_WITH_LINE_FEED){
                    stream.write('\r');
                }
                if(!id.identified){
                    id.identifying = true;
                }
                parser.processCommand(cmd, response, stream);
                array.add(cmd);
                cmd = "";
                if (response != F("")) {
                    stream.println(response.c_str());
                }
            }
        }
    }
};

#endif
