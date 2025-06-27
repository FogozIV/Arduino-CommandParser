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
#define MATHOP(name, str) #str,

inline static std::string mathOP_names[] = {
    MATHOPS
};

inline static std::string mathOPToString(MathOP op) {
    if (op >= MathOP::MathOPCount || op < 0) return "Unknown";
    return mathOP_names[static_cast<int>(op)];
}

constexpr static MathOP stringToMathOP(const std::string& str) {
    for (int i = 0; i < MathOPCount; ++i) {
        if (str == mathOP_names[i])
            return static_cast<MathOP>(i);
    }
    return MathOPCount;
}

template <size_t N1, size_t N2, size_t N3>
constexpr auto make_command_name(const char (&prefix)[N1], const char (&name)[N2], const char (&subname)[N3]) {
    std::array<char, N1 + N2 + N3 - 2> out = {};
    size_t i = 0;
    for (size_t j = 0; j < N1 - 1; ++j) out[i++] = prefix[j];
    for (size_t j = 0; j < N2 - 1; ++j) out[i++] = name[j];
    for (size_t j = 0; j < N3 - 1; ++j) out[i++] = subname[j];
    out[i] = '\0';
    return out;
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

class  CommandParser {
public:
    struct Argument {
        std::variant<double, uint64_t, int64_t, std::string> value;
        bool present = true;

        Argument() {
            present = false;
        };

        operator bool() const{
            return present;
        }

        operator double() const{
            return asDouble();
        }

        operator uint64_t() const{
            return asUInt64();
        }

        operator int64_t() const{
            return asInt64();
        }

        Argument(double d) : value(d) {}

        Argument(uint64_t u) : value(u) {}

        Argument(int64_t i) : value(i) {}

        Argument(const std::string &s) : value(s) {}

        double asDouble() const { return std::get<double>(value); }

        uint64_t asUInt64() const { return std::get<uint64_t>(value); }

        int64_t asInt64() const { return std::get<int64_t>(value); }

        const std::string &asString() const { return std::get<std::string>(value); }

        double asDoubleOr(double d) const {
            return present ? std::get<double>(value) : d;
        }

        uint64_t asUInt64Or(uint64_t u) const {
            return present ? std::get<uint64_t>(value) : u;
        }

        int64_t asInt64Or(int64_t i) const {
            return present ? std::get<int64_t>(value) : i;
        }

        const std::string &asStringOr(const std::string &s) const {
            return present ? std::get<std::string>(value) : s;
        }

    };

    struct BaseCommand {
        std::string name;
        std::string description;
        BaseCommand(const std::string &name, const std::string& description) : name(name), description(description) {}
    };

    struct  Command : public BaseCommand {
        std::string argTypes;
        std::function<std::string(std::vector<Argument>, Stream& stream)> callback;

        Command(const std::string &name, const std::string &argTypes,
                std::function<std::string(std::vector<Argument>, Stream& stream)> callback, const std::string& description = "")
                : BaseCommand(name, description), argTypes(argTypes), callback(callback) {}
    };


    struct MathCommand : public BaseCommand {
        std::shared_ptr<DoubleRef> value;
        std::function<std::string( Stream& stream, double value, MathOP op)> callback;
        template<typename T>
        MathCommand(const std::string &name, T& value,
                std::function<std::string( Stream& stream, double value, MathOP op)> callback, const std::string& description)
                    :BaseCommand(name, description), value(std::make_shared<DoubleRefImpl<T>>(value)),   callback(callback){}
    };

private:
    std::vector<Argument> commandArgs;
    std::vector<Command> commandDefinitions;
    std::vector<MathCommand> mathCommandDefinition;

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
                         std::function<std::string(std::vector<Argument>, Stream& stream)> callback, std::string description = "") {
        for (char type: argTypes) {
            if (type != 'd' && type != 'u' && type != 'i' && type != 's' && type!= 'o') return false;
        }
        std::string new_name;
        for (auto &c: name) {
            new_name += tolower(c);
        }
        commandDefinitions.emplace_back(new_name, argTypes, callback, description);
        return true;
    }
    template<typename T>
    bool registerMathCommand(std::string name, T& value, std::function<std::string(Stream& stream, double value, MathOP op)> callback, std::string description = "") {
        for (auto &c : name) {
            c = tolower(c);
        }
        mathCommandDefinition.emplace_back(name, value, callback, description);
        return true;
    }
    template<typename Container, typename T>
    bool callRemoveOn(Container& c, T a) {
        auto id = std::find_if(c.begin(), c.end(), a);
        if (id != c.end()) {
            c.erase(id);
            return true;
        }
        return false;
    }

    bool removeCommand(std::string name) {
        for (auto &c : name) {
            c = tolower(c);
        }
        return callRemoveOn(commandDefinitions, [name](auto a){return a.name == name;});
    }

    bool removeMathCommand(std::string name) {
        for (auto &c : name) {
            c = tolower(c);
        }
        return callRemoveOn(mathCommandDefinition, [name](auto a){
            return a.name == name;
        });
    }

    bool removeAllCommands(std::string name) {
        bool a = removeMathCommand(name);
        bool b = removeCommand(name);

        return a | b;
    }


    std::tuple<std::vector<std::string>, std::vector<std::string>> tab_complete(std::string cmd) {
        std::vector<std::string> description;
        std::vector<std::string> argsStrings;
        for (auto&c : cmd) {
            c = tolower(c);
        }
        for (auto &cmd_data: command_definitions()) {
            if (cmd_data.name.rfind(cmd, 0) == 0) {
                description.push_back(cmd_data.description);
                argsStrings.push_back(cmd_data.name);
            }
        }
        for (auto &cmd_data: mathCommandDefinition) {
            if (cmd_data.name.rfind(cmd, 0) == 0) {
                description.push_back(cmd_data.description);
                argsStrings.push_back(cmd_data.name + " ");
            }
        }
        if (description.empty()) {
            std::string command = cmd;
            auto of = command.find_first_of(PSTR("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRTSUVWXYZ"));
            if (of == std::string::npos) {
                return {description, argsStrings};
            }
            command.erase(0, of);
            const auto empty_char_pos = command.find_first_of(' ');
            if (empty_char_pos == std::string::npos) {
                return {description, argsStrings};
            }
            std::string name = command.substr(0, empty_char_pos);
            command.erase(0, empty_char_pos);
            command.erase(0, command.find_first_not_of(" \n\r\t"));
            auto it_math = std::find_if(mathCommandDefinition.begin(), mathCommandDefinition.end(),[&name](const MathCommand& cmd){ return cmd.name == name;});
            if (it_math == mathCommandDefinition.end()) {
                return {description, argsStrings};
            }
            for (auto &cmd_data : mathOP_names) {
                if (cmd_data == "")
                    continue;
                if (cmd_data.rfind(command, 0) == 0) {
                    argsStrings.push_back(it_math->name + " " + cmd_data);
                    description.push_back("Using the command " + it_math->name + " " + cmd_data + " to modify the value of " + it_math->name);
                }
            }
        }
        return {description, argsStrings};
    }

    bool processCommand(const std::string &commandStr, std::string &response, Stream& stream) {
        std::string command = commandStr;
        for (auto&c : command) {
            c = tolower(c);
        }
        command.erase(command.find_last_not_of(" \n\r\t") + 1);
        command.erase(0, command.find_first_of(PSTR("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRTSUVWXYZ")));
        const auto empty_char_pos = command.find_first_of(' ');
        std::string name = command.substr(0, empty_char_pos);
        if (empty_char_pos == std::string::npos) {
            command.clear();
        }else {
            command.erase(0, empty_char_pos + 1);
        }
        auto pos = command.find_first_not_of(" \n\r\t");
        if (pos != 0 && pos != std::string::npos) {
            command.erase(0, pos);
        }

        auto it = std::find_if(commandDefinitions.begin(), commandDefinitions.end(),
                               [&name](const Command &cmd) {
                                   return cmd.name == name;
                               });

        if (it == commandDefinitions.end()) {
            auto it_math = std::find_if(mathCommandDefinition.begin(), mathCommandDefinition.end(),[&name](const MathCommand& cmd){ return cmd.name == name;});
            if (it_math == mathCommandDefinition.end()) {
                response = PSTR("Error: Unknown command.");
                return false;
            }
            command.erase(command.find_last_not_of(" \n\r\t") + 1);
            if (command.empty()) {
                response = it_math->callback(stream, (it_math->value)->get(), MathOP::EMPTY);
                return true;
            }
            const auto e_c_p = command.find_first_of(' ');
            if (e_c_p == std::string::npos) {
                response = PSTR("Error: Invalid math command please add value.");
                return false;
            }
            std::string math_command = command.substr(0, e_c_p);
            if (e_c_p == std::string::npos) {
                response = PSTR("Error: Invalid math command.");
                return false;
            }
            command.erase(0, e_c_p + 1);
            char *end;
            double value = std::strtod(command.c_str(), &end);
            if (end == command.c_str()) {
                response = PSTR("Error: Invalid double argument.");
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
                    response = "Unknown operator ! " + math_command;
                    return false;
                }
            }
            response = it_math->callback(stream, it_math->value->get(), mathOp);
            return true;
        }

        const std::string &argTypes = it->argTypes;
        commandArgs.clear();
        bool optional = false;
        bool done = false;
        for (char argType: argTypes) {
            Argument arg;

            if (done) {
                arg = Argument();
            }else {
                switch (argType) {
                    case 'd': {
                        char *end;
                        double value = std::strtod(command.c_str(), &end);
                        if (end == command.c_str()) {
                            response = PSTR("Error: Invalid double argument.");
                            if (optional) {
                                done = true;
                                break;
                            }
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
                            response = PSTR("Error: Invalid unsigned integer argument.");
                            if (optional) {
                                done = true;
                                break;
                            }
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
                            response = PSTR("Error: Invalid integer argument.");
                            if (optional) {
                                done = true;
                                break;
                            }
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
                            response = PSTR("Error: Invalid string argument.");
                            if (optional) {
                                done = true;
                                break;
                            }
                            return false;
                        }
                        arg = Argument(value);
                        command.erase(0, bytesRead);
                        break;
                    }
                    case 'o': {
                        optional = true;
                    }
                }
                if (done) {
                    arg = Argument();
                }
            }
            if (argType != 'o') {
                commandArgs.push_back(arg);
            }
        }
        command.erase(0, command.find_first_not_of(" \t")); // Trim whitespace
        if (!command.empty()) {
            response = PSTR("Error: Too many arguments provided.");
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
        std::string str(40, ' ');
        stream.print(str.c_str());
        stream.print("\r");
    }else{
        stream.print("\r");
        std::string str(40, ' ');
        stream.print(str.c_str());
        stream.print("\r");
        //stream.println();
    }

}

inline std::string longestCommonPrefix(const std::vector<std::string> &strs) {
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



class CommandLineHandler {
    std::string cmd;
    std::string response;
    RoundArray array;
    StateMachine stateMachine;
    TerminalIdentifier id;
    CommandParser& parser;
    Stream& stream;
    size_t cursor = 0;

public:
    void setCommand(const std::string& a){
        if (a != "") {
            clearline(stream, id);
            cmd = a;
            cursor = cmd.size();
            stream.print(a.c_str());
        }
    }

    CommandLineHandler(CommandParser& parser, Stream& stream): parser(parser), stream(stream) {
        stateMachine.set(UP_LAST_CHAR, [&]{
            setCommand(array.go_up());
        });
        stateMachine.set(DOWN_LAST_CHAR, [&]{
            setCommand(array.go_down());
        });
        stateMachine.set(LEFT_LAST_CHAR, [&]{
            if (cursor > 0) {
                stream.write('\b');  // move cursor left
                cursor--;
            }
        });
        stateMachine.set(RIGHT_LAST_CHAR, [&]{
            if (cursor < cmd.length()) {
                stream.write(cmd[cursor]);  // reprint character under cursor
                cursor++;
            }
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
                if (!cmd.empty() && cursor > 0) {
                    cmd.erase(cmd.begin() + cursor - 1);
                    cursor--;
                    clearline(stream, id);
                    stream.print(cmd.c_str());
                    for (size_t i = cmd.size(); i > cursor; --i)
                        stream.write('\b');
                }
            } else if (c == 9) {
                auto [descriptions, commandNames]= parser.tab_complete(cmd);
                if (commandNames.empty()) {
                } else {
                    if (commandNames.size() == 1) {
                        clearline(stream, id);
                        stream.print((commandNames.front() + " : " + descriptions.front() + "\n").c_str());
                        cmd = commandNames.front();
                        cursor = cmd.size();
                        stream.print(cmd.c_str());
                    } else {
                        stream.println();
                        for (size_t i = 0; i < commandNames.size(); i++) {
                            stream.println((commandNames[i] + ": " + (descriptions[i].empty() ? "No description found" : descriptions[i])).c_str());
                        }
                        cmd = longestCommonPrefix(commandNames);
                        cursor = cmd.size();
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
                if (cursor == cmd.size()) {
                    cmd += c;
                    stream.write(c);
                } else {
                    cmd.insert(cmd.begin() + cursor, c);
                    clearline(stream, id);
                    stream.print(cmd.c_str());
                    // Move cursor back to correct position
                    for (size_t i = cmd.size(); i > cursor + 1; --i)
                        stream.write('\b');
                }
                cursor++;
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
                cursor = 0;
                if (!response.empty() && response != "") {
                    stream.println(response.c_str());
                }
            }
            stream.flush();
        }
    }
};

#endif
