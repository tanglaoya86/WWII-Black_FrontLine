// novalang.cpp - 完整实现（所有特性完善版）
// 编译（MinGW）：g++ -std=c++17 -o novalang.exe novalang.cpp -lws2_32
// 编译（Visual Studio）：cl /EHsc /std:c++17 novalang.cpp /Fe:novalang.exe /link ws2_32.lib
// 编译（Linux/macOS）：g++ -std=c++17 -o novalang novalang.cpp

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <functional>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <stdexcept>
#include <algorithm>
#include <cassert>
#include <cwchar>
#include <climits>
#include <thread>
#include <future>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <random>

#ifdef/*如果定义*/ _WIN32 //此处为Windows平台检测宏   _WIN32是Windows平台的预定义宏
    #include <winsock2.h>//编译阶段宏条件‘win32’未满足不予执行
    #include <windows.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")//非win32无法使用
#else/*如果逻辑非*/   //此处为POSIX标准平台检测宏
    #include <fcntl.h>
    #include <unistd.h>//编译阶段宏条件满足->执行
    #include <sys/socket.h>//linux和macOS的socket头文件
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

bool g_debugMode = false;

// ==================== 前向声明 ====================
class Value;
class ClassObject;
class InterfaceObject;
class InstanceObject;
class FutureObject;
class Environment;
struct UserFunction;
class NativeFunction;
class Expr;
class Stmt;

// ==================== 预处理（增强：支持带参数宏） ====================
struct Macro {
    std::vector<std::string> params;
    std::string body;
};

std::map<std::string, Macro> macros;

std::string preprocess(const std::string& source) {
    std::istringstream iss(source);
    std::ostringstream oss;
    std::string line;
    int lineNum = 0;
    while (std::getline(iss, line)) {
        lineNum++;
        size_t pos = line.find_first_not_of(" \t");
        if (pos != std::string::npos && line[pos] == '#') {
            std::istringstream linestream(line.substr(pos + 1));
            std::string directive;
            linestream >> directive;
            if (directive == "define") {
                std::string macroName, macroBody;
                linestream >> macroName;
                std::getline(linestream, macroBody);
                size_t start = macroBody.find_first_not_of(" \t");
                if (start != std::string::npos)
                    macroBody = macroBody.substr(start);
                size_t end = macroBody.find_last_not_of(" \t");
                if (end != std::string::npos)
                    macroBody = macroBody.substr(0, end + 1);

                // 检查是否有参数
                size_t lparen = macroName.find('(');
                if (lparen != std::string::npos && macroName.back() == ')') {
                    // 带参数宏
                    std::string name = macroName.substr(0, lparen);
                    std::string paramsStr = macroName.substr(lparen + 1, macroName.length() - lparen - 2);
                    std::vector<std::string> params;
                    std::istringstream ps(paramsStr);
                    std::string p;
                    while (std::getline(ps, p, ',')) {
                        size_t s = p.find_first_not_of(" \t");
                        size_t e = p.find_last_not_of(" \t");
                        if (s != std::string::npos)
                            p = p.substr(s, e - s + 1);
                        params.push_back(p);
                    }
                    macros[name] = {params, macroBody};
                } else {
                    // 无参数宏
                    macros[macroName] = {{}, macroBody};
                }
            } else {
                std::cerr << "Warning: unknown preprocessor directive '" << directive << "' at line " << lineNum << std::endl;
            }
        } else {
            oss << line << '\n';
        }
    }

    // 展开宏（简单替换，支持带参数宏需要递归扫描标识符）
    std::string result = oss.str();
    std::ostringstream final;
    std::string buffer;
    size_t i = 0;
    while (i < result.size()) {
        if (isalnum(result[i]) || result[i] == '_') {
            buffer += result[i++];
            while (i < result.size() && (isalnum(result[i]) || result[i] == '_'))
                buffer += result[i++];
            // 检查是否为宏
            auto it = macros.find(buffer);
            if (it != macros.end()) {
                // 检查后面是否有括号（参数）
                while (i < result.size() && isspace(result[i])) i++;
                if (i < result.size() && result[i] == '(' && !it->second.params.empty()) {
                    // 带参数宏调用
                    i++; // 跳过 '('
                    // 解析参数（简单处理，未考虑字符串、嵌套括号等复杂情况）
                    std::vector<std::string> args;
                    std::string arg;
                    int parenLevel = 1;
                    while (i < result.size()) {
                        if (result[i] == '(') parenLevel++;
                        else if (result[i] == ')') {
                            parenLevel--;
                            if (parenLevel == 0) break;
                        }
                        if (result[i] == ',' && parenLevel == 1) {
                            args.push_back(arg);
                            arg.clear();
                        } else {
                            arg += result[i];
                        }
                        i++;
                    }
                    args.push_back(arg);
                    // 展开宏体，替换参数
                    std::string expanded = it->second.body;
                    for (size_t j = 0; j < it->second.params.size(); ++j) {
                        std::string placeholder = it->second.params[j];
                        std::string replacement = args[j];
                        // 替换所有出现
                        size_t pos = 0;
                        while ((pos = expanded.find(placeholder, pos)) != std::string::npos) {
                            expanded.replace(pos, placeholder.length(), replacement);
                            pos += replacement.length();
                        }
                    }
                    final << expanded;
                } else {
                    // 无参数宏
                    final << it->second.body;
                }
            } else {
                final << buffer;
            }
            buffer.clear();
        } else {
            final << result[i++];
        }
    }
    return final.str();
}
extern bool g_debugMode;
// ==================== 词法分析（增强：支持 \uXXXX 转义） ====================
enum class TokenKind {
    TK_LET, TK_VAR, TK_AUTO, TK_FUNC, TK_IF, TK_ELSE, TK_WHILE, TK_FOR, TK_RETURN, TK_TRUE, TK_FALSE, TK_NULL_LIT,
    TK_BREAK, TK_CONTINUE,
    TK_INT, TK_FLOAT, TK_BOOL, TK_STRING, TK_CHAR, TK_WCHAR, TK_ANY, TK_VOID,
    TK_SWITCH, TK_CASE, TK_DEFAULT, TK_DO, TK_TRY, TK_CATCH, TK_THROW, TK_IS, TK_AS, TK_TYPEOF, TK_FOR_IN, TK_IN,
    TK_IMPORT, TK_NAMESPACE, TK_USING, TK_EXPORT,
    TK_DEFINE, TK_DEFINED,
    TK_IDENTIFIER, TK_NUMBER, TK_STRING_LITERAL, TK_CHAR_LITERAL, TK_WCHAR_LITERAL,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_INC, TK_DEC,TK_OPERATOR,
    TK_PLUS_EQUAL, TK_MINUS_EQUAL, TK_STAR_EQUAL, TK_SLASH_EQUAL, TK_PERCENT_EQUAL,
    TK_AMP_EQUAL, TK_PIPE_EQUAL, TK_CARET_EQUAL, TK_LEFT_SHIFT_EQUAL, TK_RIGHT_SHIFT_EQUAL, TK_UNSIGNED_RIGHT_SHIFT_EQUAL,
    TK_BIT_AND, TK_BIT_OR, TK_BIT_XOR, TK_BIT_NOT, TK_LEFT_SHIFT, TK_RIGHT_SHIFT, TK_UNSIGNED_RIGHT_SHIFT,
    TK_EQUAL, TK_EQUAL_EQUAL, TK_NOT_EQUAL, TK_LESS, TK_LESS_EQUAL, TK_GREATER, TK_GREATER_EQUAL,
    TK_AND, TK_OR, TK_NOT,
    TK_QUESTION, TK_COLON, TK_ARROW,
    TK_SCOPE,
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE, TK_LBRACKET, TK_RBRACKET, TK_SEMICOLON, TK_COMMA, TK_DOT,
    TK_HASH,
    TK_END_OF_FILE, TK_UNKNOWN,
    TK_CLASS, TK_INTERFACE, TK_EXTENDS, TK_IMPLEMENTS, TK_ABSTRACT, TK_VIRTUAL, TK_OVERRIDE, TK_FINAL,
    TK_STATIC, TK_THIS, TK_SUPER, TK_PUBLIC, TK_PRIVATE, TK_PROTECTED, TK_INTERNAL,
    TK_ASYNC, TK_AWAIT, TK_TASK, TK_FUTURE,
    TK_CONST, TK_SIZEOF, TK_NEW,
    TK_ENUM, TK_STRUCT, TK_EXTENSION,
    TK_AMP,    // 取地址（一元）
    TK_RANGE_CLOSED,    // ..
    TK_RANGE_HALF_OPEN  // ..<
};

struct Token {
    TokenKind type;
    std::string lexeme;
    int line;
    int column;
};

class Lexer {
    std::string source;
    size_t start = 0;
    size_t current = 0;
    int line = 1;
    int column = 1;
    std::vector<Token> tokens;
    std::map<std::string, TokenKind> keywords;

    bool isAtEnd() { return current >= source.length(); }
    char advance() { column++; return source[current++]; }
    char peek() { if (isAtEnd()) return '\0'; return source[current]; }
    char peekNext() { if (current + 1 >= source.length()) return '\0'; return source[current + 1]; }
    void addToken(TokenKind type) {
    tokens.push_back({type, source.substr(start, current - start), line, column - (int)(current - start)});
    if (g_debugMode) {
        std::cerr << "  TOKEN: type=" << (int)type << " lexeme='" << tokens.back().lexeme << "'" << std::endl;
    }
}
    bool match(char expected) {
        if (isAtEnd() || source[current] != expected) return false;
        current++; column++; return true;
    }
    void skipWhitespace() {
        while (!isAtEnd()) {
            char c = peek();
            if (c == ' ' || c == '\r' || c == '\t') advance();
            else if (c == '\n') { line++; column = 1; advance(); }
            else break;
        }
    }
    void skipComment() {
        if (peek() == '/' && peekNext() == '/') {
            while (!isAtEnd() && peek() != '\n') advance();
        } else if (peek() == '/' && peekNext() == '*') {
            advance(); advance();
            while (!isAtEnd() && !(peek() == '*' && peekNext() == '/')) {
                if (peek() == '\n') { line++; column = 1; }
                advance();
            }
            if (isAtEnd()) throw std::runtime_error("Unterminated comment");
            advance(); advance();
        }
    }

    char decodeEscape() {
        char c = advance();
        switch (c) {
            case 'n': return '\n';
            case 't': return '\t';
            case '\\': return '\\';
            case '\'': return '\'';
            case '"': return '"';
            case 'u': {
                // \uXXXX - 读取4位十六进制
                std::string hex;
                for (int i = 0; i < 4; ++i) {
                    if (isxdigit(peek())) hex += advance();
                    else throw std::runtime_error("Invalid \\u escape");
                }
                unsigned int code = std::stoul(hex, nullptr, 16);
                if (code <= 0x7F) return static_cast<char>(code);
                else throw std::runtime_error("Non-ASCII \\u escape not supported in char literals");
            }
            default: throw std::runtime_error("Unknown escape sequence");
        }
    }

    void string() {
        while (!isAtEnd() && peek() != '"') {
            if (peek() == '\n') line++;
            else if (peek() == '\\') {
                advance(); // 跳过 '\'
                decodeEscape(); // 但我们需要存储转义后的字符，这里简化：直接消费并保留原始转义序列，后续在解析时处理
                // 为了简化，我们保留原始字符串，在解析时再处理转义
                // 所以这里不特殊处理，继续原样读取
            }
            advance();
        }
        if (isAtEnd()) throw std::runtime_error("Unterminated string");
        advance();
        addToken(TokenKind::TK_STRING_LITERAL);
    }

    void number() {
        while (isdigit(peek())) advance();
        if (peek() == '.' && isdigit(peekNext())) {
            advance();
            while (isdigit(peek())) advance();
        }
        addToken(TokenKind::TK_NUMBER);
    }

    void character() {
        bool wide = false;
        if (peek() == 'W' && peekNext() == '\'') {
            wide = true;
            advance();
        }
        if (peek() != '\'') throw std::runtime_error("Expected '\\'' at start of character literal");
        advance(); // 跳过 '
        char32_t c;
        if (peek() == '\\') {
            advance(); // 跳过 \
            c = decodeEscape();
        } else {
            c = advance();
        }
        if (peek() != '\'') throw std::runtime_error("Expected closing '\\'' after character");
        advance(); // 跳过 '
        std::string lexeme = wide ? "W'" + std::string(1, static_cast<char>(c)) + "'" : "'" + std::string(1, static_cast<char>(c)) + "'";
        tokens.push_back({wide ? TokenKind::TK_WCHAR_LITERAL : TokenKind::TK_CHAR_LITERAL, lexeme, line, column - (int)lexeme.length()});
    }

void identifier() {
    while (isalnum(peek()) || peek() == '_') advance();
    std::string text = source.substr(start, current - start);
    if (g_debugMode) {
        std::cerr << "DEBUG: identifier text = '" << text << "'" << std::endl;
    }
    auto kit = keywords.find(text);
    if (kit != keywords.end()) {
        if (g_debugMode) std::cerr << "  -> keyword, type=" << (int)kit->second << std::endl;
        addToken(kit->second);
    } else {
        if (g_debugMode) std::cerr << "  -> identifier" << std::endl;
        addToken(TokenKind::TK_IDENTIFIER);
    }
}

    TokenKind checkMultiCharOperator(char c) {
        if (c == '+') {
            if (match('+')) return TokenKind::TK_INC;
            if (match('=')) return TokenKind::TK_PLUS_EQUAL;
            return TokenKind::TK_PLUS;
        }
        if (c == '-') {
            if (match('-')) return TokenKind::TK_DEC;
            if (match('=')) return TokenKind::TK_MINUS_EQUAL;
            if (match('>')) return TokenKind::TK_ARROW;
            return TokenKind::TK_MINUS;
        }
        if (c == '*') {
            if (match('=')) return TokenKind::TK_STAR_EQUAL;
            return TokenKind::TK_STAR;
        }
        if (c == '/') {
            if (match('=')) return TokenKind::TK_SLASH_EQUAL;
            return TokenKind::TK_SLASH;
        }
        if (c == '%') {
            if (match('=')) return TokenKind::TK_PERCENT_EQUAL;
            return TokenKind::TK_PERCENT;
        }
        if (c == '&') {
            if (match('&')) return TokenKind::TK_AND;
            if (match('=')) return TokenKind::TK_AMP_EQUAL;
            return TokenKind::TK_BIT_AND;
        }
        if (c == '|') {
            if (match('|')) return TokenKind::TK_OR;
            if (match('=')) return TokenKind::TK_PIPE_EQUAL;
            return TokenKind::TK_BIT_OR;
        }
        if (c == '^') {
            if (match('=')) return TokenKind::TK_CARET_EQUAL;
            return TokenKind::TK_BIT_XOR;
        }
        if (c == '~') {
            return TokenKind::TK_BIT_NOT;
        }
        if (c == '<') {
            if (match('<')) {
                if (match('<')) {
                    if (match('=')) return TokenKind::TK_UNSIGNED_RIGHT_SHIFT_EQUAL;
                    else return TokenKind::TK_UNSIGNED_RIGHT_SHIFT;
                }
                if (match('=')) return TokenKind::TK_LEFT_SHIFT_EQUAL;
                return TokenKind::TK_LEFT_SHIFT;
            }
            if (match('=')) return TokenKind::TK_LESS_EQUAL;
            return TokenKind::TK_LESS;
        }
        if (c == '>') {
            if (match('>')) {
                if (match('>')) {
                    if (match('=')) return TokenKind::TK_UNSIGNED_RIGHT_SHIFT_EQUAL;
                    else return TokenKind::TK_UNSIGNED_RIGHT_SHIFT;
                }
                if (match('=')) return TokenKind::TK_RIGHT_SHIFT_EQUAL;
                return TokenKind::TK_RIGHT_SHIFT;
            }
            if (match('=')) return TokenKind::TK_GREATER_EQUAL;
            return TokenKind::TK_GREATER;
        }
        if (c == '=') {
            if (match('=')) return TokenKind::TK_EQUAL_EQUAL;
            return TokenKind::TK_EQUAL;
        }
        if (c == '!') {
            if (match('=')) return TokenKind::TK_NOT_EQUAL;
            return TokenKind::TK_NOT;
        }
        if (c == '?') {
            return TokenKind::TK_QUESTION;
        }
        if (c == ':') {
            if (match(':')) return TokenKind::TK_SCOPE;
            return TokenKind::TK_COLON;
        }
        if (c == '#') {
            return TokenKind::TK_HASH;
        }
        if (c == '.') {
            if (match('.')) {
                if (match('<')) {
                    return TokenKind::TK_RANGE_HALF_OPEN;
                } else {
                    return TokenKind::TK_RANGE_CLOSED;
                }
            }
            return TokenKind::TK_DOT;
        }
        return TokenKind::TK_UNKNOWN;
    }

public:
    Lexer(const std::string& src) {
        source = preprocess(src);
        keywords = {
            {"let", TokenKind::TK_LET},
            {"var", TokenKind::TK_VAR},
            {"auto", TokenKind::TK_AUTO},
            {"func", TokenKind::TK_FUNC},
            {"if", TokenKind::TK_IF},
            {"else", TokenKind::TK_ELSE},
            {"while", TokenKind::TK_WHILE},
            {"for", TokenKind::TK_FOR},
            {"return", TokenKind::TK_RETURN},
            {"break", TokenKind::TK_BREAK},
            {"continue", TokenKind::TK_CONTINUE},
            {"true", TokenKind::TK_TRUE},
            {"false", TokenKind::TK_FALSE},
            {"null", TokenKind::TK_NULL_LIT},
            {"int", TokenKind::TK_INT},
            {"float", TokenKind::TK_FLOAT},
            {"bool", TokenKind::TK_BOOL},
            {"string", TokenKind::TK_STRING},
            {"char", TokenKind::TK_CHAR},
            {"wchar", TokenKind::TK_WCHAR},
            {"any", TokenKind::TK_ANY},
            {"void", TokenKind::TK_VOID},
            {"switch", TokenKind::TK_SWITCH},
            {"case", TokenKind::TK_CASE},
            {"default", TokenKind::TK_DEFAULT},
            {"do", TokenKind::TK_DO},
            {"try", TokenKind::TK_TRY},
            {"catch", TokenKind::TK_CATCH},
            {"throw", TokenKind::TK_THROW},
            {"is", TokenKind::TK_IS},
            {"as", TokenKind::TK_AS},
            {"typeof", TokenKind::TK_TYPEOF},
            {"in", TokenKind::TK_IN},
            {"import", TokenKind::TK_IMPORT},
            {"namespace", TokenKind::TK_NAMESPACE},
            {"using", TokenKind::TK_USING},
            {"export", TokenKind::TK_EXPORT},
            {"define", TokenKind::TK_DEFINE},
            {"defined", TokenKind::TK_DEFINED},
            {"class", TokenKind::TK_CLASS},
            {"interface", TokenKind::TK_INTERFACE},
            {"extends", TokenKind::TK_EXTENDS},
            {"implements", TokenKind::TK_IMPLEMENTS},
            {"abstract", TokenKind::TK_ABSTRACT},
            {"virtual", TokenKind::TK_VIRTUAL},
            {"override", TokenKind::TK_OVERRIDE},
            {"final", TokenKind::TK_FINAL},
            {"static", TokenKind::TK_STATIC},
            {"this", TokenKind::TK_THIS},
            {"super", TokenKind::TK_SUPER},
            {"public", TokenKind::TK_PUBLIC},
            {"private", TokenKind::TK_PRIVATE},
            {"protected", TokenKind::TK_PROTECTED},
            {"internal", TokenKind::TK_INTERNAL},
            {"async", TokenKind::TK_ASYNC},
            {"await", TokenKind::TK_AWAIT},
            {"task", TokenKind::TK_TASK},
            {"future", TokenKind::TK_FUTURE},
            {"const", TokenKind::TK_CONST},
            {"sizeof", TokenKind::TK_SIZEOF},
            {"new", TokenKind::TK_NEW},
            {"enum", TokenKind::TK_ENUM},
            {"struct", TokenKind::TK_STRUCT},
            {"extension", TokenKind::TK_EXTENSION},
            {"operator", TokenKind::TK_OPERATOR}
        };
    }

    std::vector<Token> scanTokens() {
        while (!isAtEnd()) {
            while (true) {
                skipWhitespace();
                if (isAtEnd()) break;
                if (peek() == '/' && (peekNext() == '/' || peekNext() == '*')) {
                    skipComment();
                    continue;
                }
                break;
            }
            if (isAtEnd()) break;

            start = current;
            char c = advance();

            TokenKind kind = checkMultiCharOperator(c);
            if (kind != TokenKind::TK_UNKNOWN) {
                addToken(kind);
                continue;
            }

            if (c == '\'' || (c == 'W' && peek() == '\'')) {
                current = start;
                column -= 1;
                character();
                continue;
            }

            switch (c) {
                case '(': addToken(TokenKind::TK_LPAREN); break;
                case ')': addToken(TokenKind::TK_RPAREN); break;
                case '{': addToken(TokenKind::TK_LBRACE); break;
                case '}': addToken(TokenKind::TK_RBRACE); break;
                case '[': addToken(TokenKind::TK_LBRACKET); break;
                case ']': addToken(TokenKind::TK_RBRACKET); break;
                case ',': addToken(TokenKind::TK_COMMA); break;
                case ';': addToken(TokenKind::TK_SEMICOLON); break;
                case '"': string(); break;
                default:
                    if (isdigit(c)) number();
                    else if (isalpha(c) || c == '_') identifier();
                    else addToken(TokenKind::TK_UNKNOWN);
                    break;
            }
        }
        tokens.push_back({TokenKind::TK_END_OF_FILE, "", line, column});
        return tokens;
    }
};

// ==================== 值系统（增强：支持多维数组、深拷贝结构体） ====================
// 前向声明已在开头提供

enum class ValueType {
    NULL_TYPE,
    INT,
    FLOAT,
    BOOL,
    STRING,
    CHAR,
    WCHAR,
    ANY,
    FUNCTION,
    NATIVE,
    ARRAY,
    CLASS,
    INTERFACE,
    INSTANCE,
    FUTURE,
    NAMESPACE,
    POINTER,
    STRUCT
};

// 全局地址映射（用于指针）
struct AddressEntry {
    std::weak_ptr<Environment> env;
    std::string varName;
    int64_t address;
};
std::map<int64_t, AddressEntry> addressMap;
std::mutex addressMutex;
std::atomic<int64_t> nextAddress{1}; 

class Value {
    ValueType type_;
    union Data {
        int64_t intVal;
        double floatVal;
        bool bool_;
        char charVal;
        char32_t wcharVal;
        std::string* string_ptr;
        std::shared_ptr<UserFunction>* func_ptr;
        std::shared_ptr<NativeFunction>* native_ptr;
        std::vector<Value>* array_ptr;
        std::shared_ptr<ClassObject>* class_ptr;
        std::shared_ptr<InterfaceObject>* interface_ptr;
        std::shared_ptr<InstanceObject>* instance_ptr;
        std::shared_ptr<FutureObject>* future_ptr;
        std::shared_ptr<Environment>* namespace_ptr;
        int64_t ptrVal; 
        std::shared_ptr<InstanceObject>* struct_ptr;
    } data_;

    void destroy();
    Value deepCopyStruct(const Value& other) const; 

public:
    Value();
    explicit Value(int64_t i);
    explicit Value(double d);
    explicit Value(bool b);
    explicit Value(char c);
    explicit Value(char32_t wc);
    explicit Value(const std::string& s);
    explicit Value(std::shared_ptr<UserFunction> f);
    explicit Value(std::shared_ptr<NativeFunction> n);
    explicit Value(const std::vector<Value>& arr);
    explicit Value(std::shared_ptr<ClassObject> c);
    explicit Value(std::shared_ptr<InterfaceObject> i);
    explicit Value(std::shared_ptr<InstanceObject> i);
    explicit Value(std::shared_ptr<FutureObject> f);
    explicit Value(std::shared_ptr<Environment> ns);
    explicit Value(int64_t addr, bool isPtr);  
    explicit Value(std::shared_ptr<InstanceObject> s, bool isStruct); 

    Value(const Value& other);
    Value& operator=(const Value& other);
    void swap(Value& other) noexcept;
    ~Value();

    Value deepCopy() const;

    ValueType type() const { return type_; }
    bool isNull() const { return type_ == ValueType::NULL_TYPE; }
    bool isInt() const { return type_ == ValueType::INT; }
    bool isFloat() const { return type_ == ValueType::FLOAT; }
    bool isNumber() const { return isInt() || isFloat(); }
    bool isBool() const { return type_ == ValueType::BOOL; }
    bool isChar() const { return type_ == ValueType::CHAR; }
    bool isWChar() const { return type_ == ValueType::WCHAR; }
    bool isString() const { return type_ == ValueType::STRING; }
    bool isFunction() const { return type_ == ValueType::FUNCTION; }
    bool isNative() const { return type_ == ValueType::NATIVE; }
    bool isArray() const { return type_ == ValueType::ARRAY; }
    bool isClass() const { return type_ == ValueType::CLASS; }
    bool isInterface() const { return type_ == ValueType::INTERFACE; }
    bool isInstance() const { return type_ == ValueType::INSTANCE; }
    bool isFuture() const { return type_ == ValueType::FUTURE; }
    bool isNamespace() const { return type_ == ValueType::NAMESPACE; }
    bool isPointer() const { return type_ == ValueType::POINTER; }
    bool isStruct() const { return type_ == ValueType::STRUCT; }

    int64_t asInt() const { assert(isInt()); return data_.intVal; }
    double asFloat() const { assert(isFloat()); return data_.floatVal; }
    bool asBool() const { assert(isBool()); return data_.bool_; }
    char asChar() const { assert(isChar()); return data_.charVal; }
    char32_t asWChar() const { assert(isWChar()); return data_.wcharVal; }
    const std::string& asString() const { assert(isString()); return *data_.string_ptr; }
    std::shared_ptr<UserFunction> asFunction() const { assert(isFunction()); return *data_.func_ptr; }
    std::shared_ptr<NativeFunction> asNative() const { assert(isNative()); return *data_.native_ptr; }
    const std::vector<Value>& asArray() const { assert(isArray()); return *data_.array_ptr; }
    std::vector<Value>& asArray() { assert(isArray()); return *data_.array_ptr; }
    std::shared_ptr<ClassObject> asClass() const { assert(isClass()); return *data_.class_ptr; }
    std::shared_ptr<InterfaceObject> asInterface() const { assert(isInterface()); return *data_.interface_ptr; }
    std::shared_ptr<InstanceObject> asInstance() const { assert(isInstance()); return *data_.instance_ptr; }
    std::shared_ptr<FutureObject> asFuture() const { assert(isFuture()); return *data_.future_ptr; }
    std::shared_ptr<Environment> asNamespace() const { assert(isNamespace()); return *data_.namespace_ptr; }
    int64_t asPointer() const { assert(isPointer()); return data_.ptrVal; }
    std::shared_ptr<InstanceObject> asStruct() const { assert(isStruct()); return *data_.struct_ptr; }

    double toDouble() const;
};

// ==================== 全局辅助函数 ====================
inline bool isNumber(const Value& v) { return v.isNumber(); }
inline bool isInt(const Value& v) { return v.isInt(); }
inline bool isFloat(const Value& v) { return v.isFloat(); }
inline bool isBool(const Value& v) { return v.isBool(); }
inline bool isChar(const Value& v) { return v.isChar(); }
inline bool isWChar(const Value& v) { return v.isWChar(); }
inline bool isString(const Value& v) { return v.isString(); }
inline bool isFunction(const Value& v) { return v.isFunction(); }
inline bool isNative(const Value& v) { return v.isNative(); }
inline bool isArray(const Value& v) { return v.isArray(); }
inline bool isClass(const Value& v) { return v.isClass(); }
inline bool isInterface(const Value& v) { return v.isInterface(); }
inline bool isInstance(const Value& v) { return v.isInstance(); }
inline bool isFuture(const Value& v) { return v.isFuture(); }
inline bool isNamespace(const Value& v) { return v.isNamespace(); }
inline bool isNull(const Value& v) { return v.isNull(); }
inline bool isPointer(const Value& v) { return v.isPointer(); }
inline bool isStruct(const Value& v) { return v.isStruct(); }

inline int64_t asInt(const Value& v) { return v.asInt(); }
inline double asFloat(const Value& v) { return v.asFloat(); }
inline bool asBool(const Value& v) { return v.asBool(); }
inline char asChar(const Value& v) { return v.asChar(); }
inline char32_t asWChar(const Value& v) { return v.asWChar(); }
inline const std::string& asString(const Value& v) { return v.asString(); }
inline const std::vector<Value>& asArray(const Value& v) { return v.asArray(); }
inline std::shared_ptr<UserFunction> asFunction(const Value& v) { return v.asFunction(); }
inline std::shared_ptr<NativeFunction> asNative(const Value& v) { return v.asNative(); }
inline std::shared_ptr<ClassObject> asClass(const Value& v) { return v.asClass(); }
inline std::shared_ptr<InterfaceObject> asInterface(const Value& v) { return v.asInterface(); }
inline std::shared_ptr<InstanceObject> asInstance(const Value& v) { return v.asInstance(); }
inline std::shared_ptr<FutureObject> asFuture(const Value& v) { return v.asFuture(); }
inline std::shared_ptr<Environment> asNamespace(const Value& v) { return v.asNamespace(); }
inline int64_t asPointer(const Value& v) { return v.asPointer(); }
inline std::shared_ptr<InstanceObject> asStruct(const Value& v) { return v.asStruct(); }

// ==================== 类型大小映射 ====================
static size_t typeSizeOf(const std::string& typeName) {
    if (typeName == "int") return 8;
    if (typeName == "float") return 8;
    if (typeName == "bool") return 1;
    if (typeName == "char") return 1;
    if (typeName == "wchar") return 4;
    if (typeName == "string") return sizeof(std::string);
    if (typeName == "array") return sizeof(std::vector<Value>);
    return 0;
}

static size_t valueSizeOf(const Value& val) {
    if (val.isInt()) return 8;
    if (val.isFloat()) return 8;
    if (val.isBool()) return 1;
    if (val.isChar()) return 1;
    if (val.isWChar()) return 4;
    if (val.isString()) return val.asString().size();
    if (val.isArray()) return val.asArray().size() * sizeof(Value);
    return 0;
}

// ==================== 接口定义 ====================
class InterfaceObject {
public:
    std::string name;
    std::vector<std::shared_ptr<InterfaceObject>> parentInterfaces;
    std::map<std::string, std::shared_ptr<UserFunction>> methods;
    bool isAbstract = true;
    InterfaceObject(const std::string& n) : name(n) {}
};

// ==================== 类定义 ====================
class ClassObject {
public:
    std::string name;
    std::shared_ptr<ClassObject> superClass;
    std::vector<std::shared_ptr<InterfaceObject>> interfaces;
    std::map<std::string, Value> staticFields;
    std::map<std::string, std::shared_ptr<UserFunction>> methods;
    std::map<std::string, std::shared_ptr<UserFunction>> staticMethods;
    std::vector<std::string> fieldNames;
    std::map<std::string, std::unique_ptr<Expr>> fieldInitializers;
    bool isAbstract = false;
    bool isInterface = false;
    bool isFinal = false;
    bool isStruct = false;
    ClassObject(const std::string& n) : name(n) {}
    ~ClassObject() = default;
};

// ==================== 实例对象 ====================
class InstanceObject {
public:
    std::shared_ptr<ClassObject> klass;
    std::map<std::string, Value> fields;
    InstanceObject(std::shared_ptr<ClassObject> k) : klass(k) {}
    std::shared_ptr<UserFunction> findMethod(const std::string& name); // 声明
};

// ==================== 异步 Future（使用 std::async） ====================
class FutureObject {
public:
    std::shared_future<Value> future;
    bool ready = false;
    Value result;
    std::mutex mtx;
    FutureObject(std::shared_future<Value> f) : future(f) {}
    bool isReady() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!ready && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            result = future.get();
            ready = true;
        }
        return ready;
    }
    Value get() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!ready) {
            result = future.get();
            ready = true;
        }
        return result;
    }
};

// ==================== valueToString 和 isTruthy ====================
std::string valueToString(const Value& val) {
    if (val.isNull()) return "null";
    if (val.isInt()) return std::to_string(asInt(val));
    if (val.isFloat()) return std::to_string(asFloat(val));
    if (val.isBool()) return asBool(val) ? "true" : "false";
    if (val.isChar()) return std::string(1, asChar(val));
    if (val.isWChar()) {
        char32_t wc = asWChar(val);
        if (wc <= 0x7F) return std::string(1, static_cast<char>(wc));
        char buf[16];
        snprintf(buf, sizeof(buf), "<U+%04X>", static_cast<unsigned>(wc));
        return buf;
    }
    if (val.isString()) return asString(val);
    if (val.isFunction()) return "<function>";
    if (val.isNative()) return "<native function>";
    if (val.isArray()) {
        std::string result = "[";
        const auto& arr = asArray(val);
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) result += ", ";
            result += valueToString(arr[i]);
        }
        result += "]";
        return result;
    }
    if (val.isClass()) return "<class " + val.asClass()->name + ">";
    if (val.isInterface()) return "<interface " + val.asInterface()->name + ">";
    if (val.isInstance()) return "<instance of " + val.asInstance()->klass->name + ">";
    if (val.isFuture()) return "<future>";
    if (val.isNamespace()) return "<namespace>";
    if (val.isPointer()) return "<pointer " + std::to_string(asPointer(val)) + ">";
    if (val.isStruct()) return "<struct " + val.asStruct()->klass->name + ">";
    return "<unknown>";
}

bool isTruthy(const Value& val) {
    if (val.isNull()) return false;
    if (val.isBool()) return asBool(val);
    if (val.isInt()) return asInt(val) != 0;
    if (val.isFloat()) return asFloat(val) != 0.0;
    if (val.isChar()) return asChar(val) != '\0';
    if (val.isWChar()) return asWChar(val) != 0;
    if (val.isString()) return !asString(val).empty();
    if (val.isFunction() || val.isNative()) return true;
    if (val.isArray()) return !asArray(val).empty();
    if (val.isClass() || val.isInterface() || val.isInstance() || val.isFuture() || val.isNamespace() || val.isPointer() || val.isStruct()) return true;
    return true;
}

// ==================== 原生函数包装 ====================
using NativeFn = std::function<Value(const std::vector<Value>&, std::shared_ptr<Environment>)>;

class NativeFunction {
public:
    NativeFn fn;
    int arity;
    std::string name;
    NativeFunction(NativeFn f, int a, const std::string& n) : fn(f), arity(a), name(n) {}
};

// ==================== 异常定义（移到 Value 之后） ====================
class ReturnException : public std::exception {
public:
    Value value;
    int line;
    explicit ReturnException(const Value& v, int l = 0) : value(v), line(l) {}
};

class BreakException : public std::exception {
public:
    std::string label;
    BreakException(const std::string& l = "") : label(l) {}
};

class ContinueException : public std::exception {
public:
    std::string label;
    ContinueException(const std::string& l = "") : label(l) {}
};

class ThrowException : public std::exception {
public:
    Value value;
    int line;
    ThrowException(const Value& v, int l) : value(v), line(l) {}
};

// ==================== AST 基础 ====================
struct UserFunction {
    std::vector<std::string> params;
    std::vector<Value> defaultValues;
    std::shared_ptr<Environment> closure;
    std::vector<std::unique_ptr<class Stmt>> body;
    bool isStatic = false;
    bool isFinal = false;
    bool isAbstract = false;
    bool isVirtual = false;
    bool isOverride = false;
    bool isExtension = false; // 是否为扩展方法
    std::string access;
    std::string returnType;
};

class Expr {
public:
    virtual ~Expr() = default;
    virtual Value evaluate(std::shared_ptr<Environment> env) = 0;
    virtual int getLine() const { return 0; }
};

class Stmt {
public:
    virtual ~Stmt() = default;
    virtual void execute(std::shared_ptr<Environment> env) = 0;
    virtual int getLine() const { return 0; }
};

// ==================== 环境（增强：支持地址映射、类型检查） ====================
class Environment : public std::enable_shared_from_this<Environment> {
    struct Binding {
        Value value;
        bool isMutable;
        bool isDynamic;
        std::string typeAnnotation;
        std::string access;
        std::shared_ptr<ClassObject> declaredInClass;
        int64_t address; // 如果被取地址，记录地址；否则为0
    };
    std::map<std::string, Binding> values;
    std::shared_ptr<Environment> parent;
    std::string namespaceName;
    
    std::shared_ptr<InstanceObject> currentInstance;
    std::shared_ptr<ClassObject> currentClass;
    std::string currentModule;
    std::set<int64_t> ownedAddresses; // 本环境中被取地址的变量地址，用于析构时清理

    bool hasAccess(const Binding& binding, std::shared_ptr<ClassObject> accessingClass, bool isExtension = false) const {
        (void)accessingClass; (void)isExtension; // 消除警告
        if (binding.access.empty()) return true;
        if (binding.access == "public") return true;
        if (binding.access == "private") {
            // 同一类内可访问，即使实例不同
            return (currentClass == binding.declaredInClass);
        }
        if (binding.access == "protected") {
            if (!currentClass) return false;
            if (currentClass == binding.declaredInClass) return true;
            auto cls = currentClass;
            while (cls) {
                if (cls == binding.declaredInClass) return true;
                cls = cls->superClass;
            }
            return false;
        }
        if (binding.access == "internal") {
            return (currentModule == binding.declaredInClass->name); // 简化
        }
        return false;
    }

    // 类型兼容性检查（公有）
public:
    bool isTypeCompatible(const Value& val, const std::string& expectedType) const {
        if (expectedType.empty() || expectedType == "any") return true;
        ValueType vt = val.type();
        if (expectedType == "int") return vt == ValueType::INT;
        if (expectedType == "float") return vt == ValueType::FLOAT;
        if (expectedType == "bool") return vt == ValueType::BOOL;
        if (expectedType == "char") return vt == ValueType::CHAR;
        if (expectedType == "wchar") return vt == ValueType::WCHAR;
        if (expectedType == "string") return vt == ValueType::STRING;
        if (expectedType == "array") return vt == ValueType::ARRAY;
        if (expectedType == "function") return vt == ValueType::FUNCTION;
        if (expectedType == "class") return vt == ValueType::CLASS;
        if (expectedType == "interface") return vt == ValueType::INTERFACE;
        if (expectedType == "instance") return vt == ValueType::INSTANCE;
        if (expectedType == "future") return vt == ValueType::FUTURE;
        if (expectedType == "namespace") return vt == ValueType::NAMESPACE;
        if (expectedType == "pointer") return vt == ValueType::POINTER;
        if (expectedType == "struct") return vt == ValueType::STRUCT;
        // 自定义类/接口
        if (vt == ValueType::INSTANCE) {
            auto inst = val.asInstance();
            // 检查 expectedType 是否为类名或接口名
            if (inst->klass->name == expectedType) return true;
            // 检查父类
            auto cls = inst->klass->superClass;
            while (cls) {
                if (cls->name == expectedType) return true;
                cls = cls->superClass;
            }
            // 检查实现的接口
            for (auto iface : inst->klass->interfaces) {
                if (iface->name == expectedType) return true;
            }
            return false;
        }
        if (vt == ValueType::STRUCT) {
            auto s = val.asStruct();
            if (s->klass->name == expectedType) return true;
            return false;
        }
        if (vt == ValueType::CLASS) {
            auto cls = val.asClass();
            if (cls->name == expectedType) return true;
            return false;
        }
        if (vt == ValueType::INTERFACE) {
            auto iface = val.asInterface();
            if (iface->name == expectedType) return true;
            return false;
        }
        return false;
    }

public:
    Environment() : parent(nullptr) {}
    Environment(std::shared_ptr<Environment> p, const std::string& ns = "")
        : parent(p), namespaceName(ns) {}
    
    ~Environment() {
        // 清理地址映射中本环境拥有的地址
        std::lock_guard<std::mutex> lock(addressMutex);
        for (auto addr : ownedAddresses) {
            addressMap.erase(addr);
        }
    }
    
    void setCurrentInstance(std::shared_ptr<InstanceObject> inst) { currentInstance = inst; }
    std::shared_ptr<InstanceObject> getCurrentInstance() const { return currentInstance; }
    
    void setCurrentClass(std::shared_ptr<ClassObject> klass) { currentClass = klass; }
    std::shared_ptr<ClassObject> getCurrentClass() const { return currentClass; }
    
    void setCurrentModule(const std::string& module) { currentModule = module; }
    
    void define(const std::string& name, const Value& value, bool isMutable = true,
                const std::string& typeAnnotation = "",
                const std::string& access = "",
                std::shared_ptr<ClassObject> declaredInClass = nullptr,
                bool isDynamic = false) {
        values[name] = {value.deepCopy(), isMutable, isDynamic, typeAnnotation, access, declaredInClass, 0};
    }
    
    Value get(const std::vector<std::string>& names) const {
        if (names.size() == 1) {
            return getSimple(names[0]);
        } else if (names.size() == 2) {
            auto it = values.find(names[0]);
            if (it != values.end() && it->second.value.isNamespace()) {
                return it->second.value.asNamespace()->getSimple(names[1]);
            }
            std::string fullName = names[0] + "::" + names[1];
            return getSimple(fullName);
        } else {
            throw std::runtime_error("Unsupported qualified name depth");
        }
    }
    
    Value getSimple(const std::string& name) const {
        auto it = values.find(name);
        if (it != values.end()) {
            if (!hasAccess(it->second, currentClass, false)) {
                throw std::runtime_error("Access denied to '" + name + "'");
            }
            return it->second.value;
        }
        if (parent) return parent->getSimple(name);
        throw std::runtime_error("Undefined variable '" + name + "'");
    }
    
    Value& getModifiable(const std::string& name) {
        auto it = values.find(name);
        if (it != values.end()) {
            if (!hasAccess(it->second, currentClass, false)) {
                throw std::runtime_error("Access denied to '" + name + "'");
            }
            return it->second.value;
        }
        if (parent) return parent->getModifiable(name);
        throw std::runtime_error("Undefined variable '" + name + "'");
    }
    
    void assign(const std::string& name, const Value& value) {
        auto it = values.find(name);
        if (it != values.end()) {
            if (!it->second.isMutable)
                throw std::runtime_error("Cannot assign to immutable variable '" + name + "'");
            if (!hasAccess(it->second, currentClass, false)) {
                throw std::runtime_error("Access denied to '" + name + "'");
            }
            // 类型检查
            if (!it->second.isDynamic && !it->second.typeAnnotation.empty() && it->second.typeAnnotation != "any") {
                if (!isTypeCompatible(value, it->second.typeAnnotation)) {
                    throw std::runtime_error("Type mismatch: cannot assign " + valueToString(value) +
                                             " to variable '" + name + "' of type " + it->second.typeAnnotation);
                }
            }
            it->second.value = value.deepCopy(); // 对于结构体，深拷贝
            return;
        }
        if (parent) { parent->assign(name, value); return; }
        throw std::runtime_error("Undefined variable '" + name + "'");
    }
    
    bool exists(const std::string& name) const {
        if (values.find(name) != values.end()) return true;
        if (parent) return parent->exists(name);
        return false;
    }
    
    std::shared_ptr<Environment> newChild(const std::string& ns = "") {
        auto child = std::make_shared<Environment>(shared_from_this(), ns);
        child->currentInstance = currentInstance;
        child->currentClass = currentClass;
        child->currentModule = currentModule;
        return child;
    }
    
    Value getMember(const Value& obj, const std::string& member) {
        if (obj.isInstance()) {
            auto inst = obj.asInstance();
            auto fit = inst->fields.find(member);
            if (fit != inst->fields.end()) return fit->second;
            auto meth = inst->findMethod(member);
            if (meth) return Value(meth);
            throw std::runtime_error("Instance has no member '" + member + "'");
        }
        else if (obj.isStruct()) {
            auto inst = obj.asStruct();
            auto fit = inst->fields.find(member);
            if (fit != inst->fields.end()) return fit->second;
            auto meth = inst->findMethod(member);
            if (meth) return Value(meth);
            throw std::runtime_error("Struct has no member '" + member + "'");
        }
        else if (obj.isClass()) {
            auto klass = obj.asClass();
            auto sit = klass->staticFields.find(member);
            if (sit != klass->staticFields.end()) return sit->second;
            auto mit = klass->staticMethods.find(member);
            if (mit != klass->staticMethods.end()) return Value(mit->second);
            throw std::runtime_error("Class has no static member '" + member + "'");
        }
        else if (obj.isNamespace()) {
            auto nsEnv = obj.asNamespace();
            return nsEnv->getSimple(member);
        }
        else {
            throw std::runtime_error("Cannot access member of non-object");
        }
    }
    
    void assignMember(Value& obj, const std::string& member, const Value& val) {
        if (obj.isInstance()) {
            auto inst = obj.asInstance();
            inst->fields[member] = val.deepCopy(); // 深拷贝
        } else if (obj.isStruct()) {
            auto inst = obj.asStruct();
            inst->fields[member] = val.deepCopy();
        } else if (obj.isClass()) {
            auto klass = obj.asClass();
            klass->staticFields[member] = val;
        } else {
            throw std::runtime_error("Cannot assign member of non-instance or non-class");
        }
    }
    
    bool isNamespace() const { return !namespaceName.empty(); }
    std::shared_ptr<Environment> asNamespaceEnv() const { return std::const_pointer_cast<Environment>(shared_from_this()); }
    
    std::map<std::string, Value> getExports() const {
        std::map<std::string, Value> exports;
        for (const auto& pair : values) {
            exports[pair.first] = pair.second.value;
        }
        return exports;
    }

    // 指针支持：为变量分配地址
    int64_t getAddress(const std::string& name) {
        auto it = values.find(name);
        if (it == values.end()) throw std::runtime_error("Variable '" + name + "' not found");
        if (it->second.address == 0) {
            std::lock_guard<std::mutex> lock(addressMutex);
            int64_t addr = nextAddress++;
            it->second.address = addr;
            ownedAddresses.insert(addr);
            addressMap[addr] = {shared_from_this(), name, addr};
        }
        return it->second.address;
    }

    // 根据地址获取变量引用
    static Value* getValueByAddress(int64_t addr) {
        std::lock_guard<std::mutex> lock(addressMutex);
        auto it = addressMap.find(addr);
        if (it == addressMap.end()) return nullptr;
        auto env = it->second.env.lock();
        if (!env) {
            addressMap.erase(it);
            return nullptr;
        }
        auto& binding = env->values[it->second.varName];
        return &binding.value;
    }

    // 根据地址修改变量
    static void assignByAddress(int64_t addr, const Value& val) {
        Value* ptr = getValueByAddress(addr);
        if (!ptr) throw std::runtime_error("Dangling pointer");
        *ptr = val.deepCopy();
    }
};

// ==================== Value 成员函数实现（需放在完整类型之后） ====================
// 现在 InstanceObject 已定义，可以实现 Value 的成员函数
void Value::destroy() {
    switch (type_) {
        case ValueType::STRING: delete data_.string_ptr; break;
        case ValueType::FUNCTION: delete data_.func_ptr; break;
        case ValueType::NATIVE: delete data_.native_ptr; break;
        case ValueType::ARRAY: delete data_.array_ptr; break;
        case ValueType::CLASS: delete data_.class_ptr; break;
        case ValueType::INTERFACE: delete data_.interface_ptr; break;
        case ValueType::INSTANCE: delete data_.instance_ptr; break;
        case ValueType::FUTURE: delete data_.future_ptr; break;
        case ValueType::NAMESPACE: delete data_.namespace_ptr; break;
        case ValueType::STRUCT: delete data_.struct_ptr; break;
        default: break;
    }
    type_ = ValueType::NULL_TYPE;
}

Value Value::deepCopyStruct(const Value& other) const {
    if (!other.isStruct()) return other;
    auto src = other.asStruct();
    auto dst = std::make_shared<InstanceObject>(src->klass);
    for (const auto& field : src->fields) {
        dst->fields[field.first] = field.second.deepCopy();
    }
    return Value(dst, true);
}

Value::Value() : type_(ValueType::NULL_TYPE) { data_.intVal = 0; }
Value::Value(int64_t i) : type_(ValueType::INT) { data_.intVal = i; }
Value::Value(double d) : type_(ValueType::FLOAT) { data_.floatVal = d; }
Value::Value(bool b) : type_(ValueType::BOOL) { data_.bool_ = b; }
Value::Value(char c) : type_(ValueType::CHAR) { data_.charVal = c; }
Value::Value(char32_t wc) : type_(ValueType::WCHAR) { data_.wcharVal = wc; }
Value::Value(const std::string& s) : type_(ValueType::STRING) { data_.string_ptr = new std::string(s); }
Value::Value(std::shared_ptr<UserFunction> f) : type_(ValueType::FUNCTION) { data_.func_ptr = new std::shared_ptr<UserFunction>(f); }
Value::Value(std::shared_ptr<NativeFunction> n) : type_(ValueType::NATIVE) { data_.native_ptr = new std::shared_ptr<NativeFunction>(n); }
Value::Value(const std::vector<Value>& arr) : type_(ValueType::ARRAY) { data_.array_ptr = new std::vector<Value>(arr); }
Value::Value(std::shared_ptr<ClassObject> c) : type_(ValueType::CLASS) { data_.class_ptr = new std::shared_ptr<ClassObject>(c); }
Value::Value(std::shared_ptr<InterfaceObject> i) : type_(ValueType::INTERFACE) { data_.interface_ptr = new std::shared_ptr<InterfaceObject>(i); }
Value::Value(std::shared_ptr<InstanceObject> i) : type_(ValueType::INSTANCE) { data_.instance_ptr = new std::shared_ptr<InstanceObject>(i); }
Value::Value(std::shared_ptr<FutureObject> f) : type_(ValueType::FUTURE) { data_.future_ptr = new std::shared_ptr<FutureObject>(f); }
Value::Value(std::shared_ptr<Environment> ns) : type_(ValueType::NAMESPACE) { data_.namespace_ptr = new std::shared_ptr<Environment>(ns); }
Value::Value(int64_t addr, bool /*isPtr*/) : type_(ValueType::POINTER) { data_.ptrVal = addr; }
Value::Value(std::shared_ptr<InstanceObject> s, bool /*isStruct*/) : type_(ValueType::STRUCT) { data_.struct_ptr = new std::shared_ptr<InstanceObject>(s); }

Value::Value(const Value& other) : type_(other.type_) {
    switch (type_) {
        case ValueType::INT: data_.intVal = other.data_.intVal; break;
        case ValueType::FLOAT: data_.floatVal = other.data_.floatVal; break;
        case ValueType::BOOL: data_.bool_ = other.data_.bool_; break;
        case ValueType::CHAR: data_.charVal = other.data_.charVal; break;
        case ValueType::WCHAR: data_.wcharVal = other.data_.wcharVal; break;
        case ValueType::STRING: data_.string_ptr = new std::string(*other.data_.string_ptr); break;
        case ValueType::FUNCTION: data_.func_ptr = new std::shared_ptr<UserFunction>(*other.data_.func_ptr); break;
        case ValueType::NATIVE: data_.native_ptr = new std::shared_ptr<NativeFunction>(*other.data_.native_ptr); break;
        case ValueType::ARRAY: data_.array_ptr = new std::vector<Value>(*other.data_.array_ptr); break;
        case ValueType::CLASS: data_.class_ptr = new std::shared_ptr<ClassObject>(*other.data_.class_ptr); break;
        case ValueType::INTERFACE: data_.interface_ptr = new std::shared_ptr<InterfaceObject>(*other.data_.interface_ptr); break;
        case ValueType::INSTANCE: data_.instance_ptr = new std::shared_ptr<InstanceObject>(*other.data_.instance_ptr); break;
        case ValueType::FUTURE: data_.future_ptr = new std::shared_ptr<FutureObject>(*other.data_.future_ptr); break;
        case ValueType::NAMESPACE: data_.namespace_ptr = new std::shared_ptr<Environment>(*other.data_.namespace_ptr); break;
        case ValueType::POINTER: data_.ptrVal = other.data_.ptrVal; break;
        case ValueType::STRUCT: {
            auto src = *other.data_.struct_ptr;
            auto dst = std::make_shared<InstanceObject>(src->klass);
            for (const auto& field : src->fields) {
                dst->fields[field.first] = field.second.deepCopy();
            }
            data_.struct_ptr = new std::shared_ptr<InstanceObject>(dst);
            break;
        }
        default: data_.intVal = 0; break;
    }
}

Value& Value::operator=(const Value& other) {
    if (this == &other) return *this;
    Value temp(other);
    swap(temp);
    return *this;
}

void Value::swap(Value& other) noexcept {
    std::swap(type_, other.type_);
    std::swap(data_, other.data_);
}

Value::~Value() { destroy(); }

Value Value::deepCopy() const {
    if (isStruct()) {
        return deepCopyStruct(*this);
    }
    return *this;
}

double Value::toDouble() const {
    if (isInt()) return static_cast<double>(asInt());
    if (isFloat()) return asFloat();
    throw std::runtime_error("Not a number");
}

// ==================== InstanceObject 成员函数实现 ====================
std::shared_ptr<UserFunction> InstanceObject::findMethod(const std::string& name) {
    auto it = klass->methods.find(name);
    if (it != klass->methods.end()) return it->second;
    if (klass->superClass) {
        return klass->superClass->methods[name];
    }
    return nullptr;
}

// ==================== 表达式节点声明 ====================
class MemberExpr : public Expr {
    std::unique_ptr<Expr> object;
    std::string member;
    bool isStatic;
    int line;
public:
    MemberExpr(std::unique_ptr<Expr> obj, const std::string& mem, bool stat, int l)
        : object(std::move(obj)), member(mem), isStatic(stat), line(l) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
    Expr* getObject() const { return object.get(); }
    const std::string& getMember() const { return member; }
    bool isStaticAccess() const { return isStatic; }
};

class NewExpr : public Expr {
    std::string className;
    std::vector<std::unique_ptr<Expr>> arguments;
    int line;
public:
    NewExpr(const std::string& cn, std::vector<std::unique_ptr<Expr>> args, int l)
        : className(cn), arguments(std::move(args)), line(l) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class AsyncExpr : public Expr {
    std::unique_ptr<Expr> expr;
    int line;
public:
    AsyncExpr(std::unique_ptr<Expr> e, int l) : expr(std::move(e)), line(l) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class AwaitExpr : public Expr {
    std::unique_ptr<Expr> futureExpr;
    int line;
public:
    AwaitExpr(std::unique_ptr<Expr> f, int l) : futureExpr(std::move(f)), line(l) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class SizeofExpr : public Expr {
    std::unique_ptr<Expr> expr;
    int line;
public:
    SizeofExpr(std::unique_ptr<Expr> e, int l) : expr(std::move(e)), line(l) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class ThisExpr : public Expr {
    int line;
public:
    ThisExpr(int l) : line(l) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class SuperExpr : public Expr {
    int line;
public:
    SuperExpr(int l) : line(l) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class RangeExpr : public Expr {
    std::unique_ptr<Expr> start;
    std::unique_ptr<Expr> end;
    bool inclusive;
    int line;
public:
    RangeExpr(std::unique_ptr<Expr> s, std::unique_ptr<Expr> e, bool inc, int l)
        : start(std::move(s)), end(std::move(e)), inclusive(inc), line(l) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class LambdaExpr : public Expr {
    std::vector<std::string> params;
    std::unique_ptr<Expr> body;
    int line;
public:
    LambdaExpr(std::vector<std::string> p, std::unique_ptr<Expr> b, int l)
        : params(std::move(p)), body(std::move(b)), line(l) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class AddressOfExpr : public Expr {
    std::unique_ptr<Expr> expr;
    int line;
public:
    AddressOfExpr(std::unique_ptr<Expr> e, int l) : expr(std::move(e)), line(l) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class DerefExpr : public Expr {
    std::unique_ptr<Expr> expr;
    int line;
public:
    DerefExpr(std::unique_ptr<Expr> e, int l) : expr(std::move(e)), line(l) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class OperatorCallExpr : public Expr {
    std::unique_ptr<Expr> left;
    std::string opName;
    std::unique_ptr<Expr> right;
    int line;
public:
    OperatorCallExpr(std::unique_ptr<Expr> l, const std::string& op, std::unique_ptr<Expr> r, int ln)
        : left(std::move(l)), opName(op), right(std::move(r)), line(ln) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class EnumDeclStmt : public Stmt {
public:
    std::string name;
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> enumerators;
    int line;
    EnumDeclStmt(const std::string& n, std::vector<std::pair<std::string, std::unique_ptr<Expr>>> e, int l)
        : name(n), enumerators(std::move(e)), line(l) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class StructDeclStmt : public Stmt {
public:
    std::string name;
    std::vector<std::string> modifiers;
    std::vector<std::unique_ptr<Stmt>> body;
    int line;
    StructDeclStmt(const std::string& n, std::vector<std::string> mod, std::vector<std::unique_ptr<Stmt>> b, int l)
        : name(n), modifiers(mod), body(std::move(b)), line(l) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class ExtensionDeclStmt : public Stmt {
public:
    std::string typeName;
    std::vector<std::unique_ptr<Stmt>> methods;
    int line;
    ExtensionDeclStmt(const std::string& tn, std::vector<std::unique_ptr<Stmt>> m, int l)
        : typeName(tn), methods(std::move(m)), line(l) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class ClassDeclStmt : public Stmt {
public:
    std::string name;
    std::vector<std::string> modifiers;
    std::string superClassName;
    std::vector<std::string> interfaceNames;
    std::vector<std::unique_ptr<Stmt>> body;
    int line;
    ClassDeclStmt(const std::string& n, std::vector<std::string> mod,
                  const std::string& sup, std::vector<std::string> ifs,
                  std::vector<std::unique_ptr<Stmt>> b, int l)
        : name(n), modifiers(mod), superClassName(sup), interfaceNames(ifs),
          body(std::move(b)), line(l) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class InterfaceDeclStmt : public Stmt {
public:
    std::string name;
    std::vector<std::string> modifiers;
    std::vector<std::string> parentInterfaceNames;
    std::vector<std::unique_ptr<Stmt>> body;
    int line;
    InterfaceDeclStmt(const std::string& n, std::vector<std::string> mod,
                      std::vector<std::string> parents,
                      std::vector<std::unique_ptr<Stmt>> b, int l)
        : name(n), modifiers(mod), parentInterfaceNames(parents), body(std::move(b)), line(l) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class MethodDeclStmt : public Stmt {
public:
    std::vector<std::string> modifiers;
    std::string name;
    std::vector<std::string> params;
    std::vector<std::unique_ptr<Expr>> defaultValues;
    std::string returnType;
    std::vector<std::unique_ptr<Stmt>> body;
    int line;
    MethodDeclStmt(std::vector<std::string> mod, const std::string& n,
                   std::vector<std::string> p,
                   std::vector<std::unique_ptr<Expr>> dv,
                   const std::string& rt,
                   std::vector<std::unique_ptr<Stmt>> b, int l)
        : modifiers(mod), name(n), params(p), defaultValues(std::move(dv)),
          returnType(rt), body(std::move(b)), line(l) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class FieldDeclStmt : public Stmt {
public:
    std::vector<std::string> modifiers;
    std::string name;
    std::unique_ptr<Expr> initializer;
    std::string type;
    int line;
    FieldDeclStmt(std::vector<std::string> mod, const std::string& n,
                  std::unique_ptr<Expr> init, const std::string& t, int l)
        : modifiers(mod), name(n), initializer(std::move(init)), type(t), line(l) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class LiteralExpr : public Expr {
    Value value;
    int line;
public:
    LiteralExpr(const Value& val, int l = 0) : value(val), line(l) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class VariableExpr : public Expr {
    std::vector<std::string> names;
    int line;
public:
    VariableExpr(const std::vector<std::string>& n, int l = 0) : names(n), line(l) {}
    const std::vector<std::string>& getNames() const { return names; }
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class BinaryExpr : public Expr {
    std::unique_ptr<Expr> left;
    TokenKind op;
    std::unique_ptr<Expr> right;
    int line;
public:
    BinaryExpr(std::unique_ptr<Expr> l, TokenKind o, std::unique_ptr<Expr> r, int ln = 0)
        : left(std::move(l)), op(o), right(std::move(r)), line(ln) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class UnaryExpr : public Expr {
    TokenKind op;
    std::unique_ptr<Expr> right;
    int line;
public:
    UnaryExpr(TokenKind o, std::unique_ptr<Expr> r, int ln = 0) : op(o), right(std::move(r)), line(ln) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class GroupingExpr : public Expr {
    std::unique_ptr<Expr> expr;
    int line;
public:
    GroupingExpr(std::unique_ptr<Expr> e, int ln = 0) : expr(std::move(e)), line(ln) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class CallExpr : public Expr {
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> arguments;
    int line;
public:
    CallExpr(std::unique_ptr<Expr> c, std::vector<std::unique_ptr<Expr>> args, int ln = 0)
        : callee(std::move(c)), arguments(std::move(args)), line(ln) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class AssignExpr : public Expr {
    std::unique_ptr<Expr> target;
    std::unique_ptr<Expr> value;
    TokenKind assignOp;
    int line;
public:
    AssignExpr(std::unique_ptr<Expr> t, std::unique_ptr<Expr> v, TokenKind op, int ln = 0)
        : target(std::move(t)), value(std::move(v)), assignOp(op), line(ln) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class PostfixExpr : public Expr {
    std::unique_ptr<Expr> left;
    TokenKind op;
    int line;
public:
    PostfixExpr(std::unique_ptr<Expr> l, TokenKind o, int ln = 0)
        : left(std::move(l)), op(o), line(ln) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class ArrayLiteralExpr : public Expr {
    std::vector<std::unique_ptr<Expr>> elements;
    int line;
public:
    ArrayLiteralExpr(std::vector<std::unique_ptr<Expr>> elems, int ln = 0)
        : elements(std::move(elems)), line(ln) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class IndexExpr : public Expr {
    std::unique_ptr<Expr> object;
    std::unique_ptr<Expr> index;
    int line;
public:
    IndexExpr(std::unique_ptr<Expr> obj, std::unique_ptr<Expr> idx, int ln = 0)
        : object(std::move(obj)), index(std::move(idx)), line(ln) {}
    Expr* getObject() const { return object.get(); }
    Expr* getIndex() const { return index.get(); }
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class TernaryExpr : public Expr {
    std::unique_ptr<Expr> cond;
    std::unique_ptr<Expr> thenExpr;
    std::unique_ptr<Expr> elseExpr;
    int line;
public:
    TernaryExpr(std::unique_ptr<Expr> c, std::unique_ptr<Expr> t, std::unique_ptr<Expr> e, int ln = 0)
        : cond(std::move(c)), thenExpr(std::move(t)), elseExpr(std::move(e)), line(ln) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class IsExpr : public Expr {
    std::unique_ptr<Expr> left;
    std::string typeName;
    int line;
public:
    IsExpr(std::unique_ptr<Expr> l, const std::string& tn, int ln = 0)
        : left(std::move(l)), typeName(tn), line(ln) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class AsExpr : public Expr {
    std::unique_ptr<Expr> left;
    std::string typeName;
    int line;
public:
    AsExpr(std::unique_ptr<Expr> l, const std::string& tn, int ln = 0)
        : left(std::move(l)), typeName(tn), line(ln) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class TypeofExpr : public Expr {
    std::unique_ptr<Expr> expr;
    int line;
public:
    TypeofExpr(std::unique_ptr<Expr> e, int ln = 0) : expr(std::move(e)), line(ln) {}
    Value evaluate(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class ExpressionStmt : public Stmt {
    std::unique_ptr<Expr> expr;
public:
    ExpressionStmt(std::unique_ptr<Expr> e) : expr(std::move(e)) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return expr->getLine(); }
};

// 修改后的 VarDeclStmt 支持多维数组
class VarDeclStmt : public Stmt {
    std::string name;
    std::unique_ptr<Expr> initializer;
    bool m_isMutable;
    bool m_isDynamic;
    std::string m_typeName;
    std::string access;
    int line;
    std::vector<std::unique_ptr<Expr>> dimensions; // 多维数组大小
public:
    VarDeclStmt(const std::string& n, std::unique_ptr<Expr> init, bool mut, bool dyn,
                const std::string& typeName, const std::string& acc = "",
                std::vector<std::unique_ptr<Expr>> dims = {}, int ln = 0)
        : name(n), initializer(std::move(init)), m_isMutable(mut), m_isDynamic(dyn),
          m_typeName(typeName), access(acc), line(ln), dimensions(std::move(dims)) {}
    const std::string& getName() const { return name; }
    std::unique_ptr<Expr> moveInitializer() { return std::move(initializer); }
    bool isMutable() const { return m_isMutable; }
    int getLine() const override { return line; }
    void execute(std::shared_ptr<Environment> env) override;
};

class BlockStmt : public Stmt {
    std::vector<std::unique_ptr<Stmt>> statements;
public:
    BlockStmt(std::vector<std::unique_ptr<Stmt>> stmts) : statements(std::move(stmts)) {}
    void execute(std::shared_ptr<Environment> env) override;
};

class IfStmt : public Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> thenBranch;
    std::unique_ptr<Stmt> elseBranch;
public:
    IfStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> thenS, std::unique_ptr<Stmt> elseS)
        : condition(std::move(cond)), thenBranch(std::move(thenS)), elseBranch(std::move(elseS)) {}
    void execute(std::shared_ptr<Environment> env) override;
};

class WhileStmt : public Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;
public:
    WhileStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> b) : condition(std::move(cond)), body(std::move(b)) {}
    void execute(std::shared_ptr<Environment> env) override;
};

class DoWhileStmt : public Stmt {
    std::unique_ptr<Stmt> body;
    std::unique_ptr<Expr> condition;
    int line;
public:
    DoWhileStmt(std::unique_ptr<Stmt> b, std::unique_ptr<Expr> c, int ln = 0)
        : body(std::move(b)), condition(std::move(c)), line(ln) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class ForStmt : public Stmt {
    std::unique_ptr<Stmt> init;
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Expr> increment;
    std::unique_ptr<Stmt> body;
public:
    ForStmt(std::unique_ptr<Stmt> i, std::unique_ptr<Expr> c, std::unique_ptr<Expr> inc, std::unique_ptr<Stmt> b)
        : init(std::move(i)), condition(std::move(c)), increment(std::move(inc)), body(std::move(b)) {}
    void execute(std::shared_ptr<Environment> env) override;
};

class ForInStmt : public Stmt {
    std::string varName;
    std::unique_ptr<Expr> iterable;
    std::unique_ptr<Stmt> body;
    int line;
public:
    ForInStmt(const std::string& v, std::unique_ptr<Expr> it, std::unique_ptr<Stmt> b, int ln = 0)
        : varName(v), iterable(std::move(it)), body(std::move(b)), line(ln) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class ReturnStmt : public Stmt {
    std::unique_ptr<Expr> value;
    int line;
public:
    ReturnStmt(std::unique_ptr<Expr> v, int ln = 0) : value(std::move(v)), line(ln) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class BreakStmt : public Stmt {
    std::string label;
    int line;
public:
    BreakStmt(const std::string& l = "", int ln = 0) : label(l), line(ln) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class ContinueStmt : public Stmt {
    std::string label;
    int line;
public:
    ContinueStmt(const std::string& l = "", int ln = 0) : label(l), line(ln) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class FunctionDeclStmt : public Stmt {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::unique_ptr<Expr>> defaultValues;
    std::vector<std::unique_ptr<Stmt>> body;
    std::vector<std::string> modifiers;
    std::string returnType;
public:
    FunctionDeclStmt(const std::string& n, std::vector<std::string> p,
                     std::vector<std::unique_ptr<Expr>> dv,
                     std::vector<std::unique_ptr<Stmt>> b,
                     std::vector<std::string> mod = {}, const std::string& rt = "any")
        : name(n), params(std::move(p)), defaultValues(std::move(dv)), body(std::move(b)), modifiers(std::move(mod)), returnType(rt) {}
    const std::string& getName() const { return name; }
    const std::vector<std::string>& getParams() const { return params; }
    std::vector<std::unique_ptr<Expr>> moveDefaultValues() { return std::move(defaultValues); }
    std::vector<std::unique_ptr<Stmt>> moveBody() { return std::move(body); }
    const std::vector<std::string>& getModifiers() const { return modifiers; }
    const std::string& getReturnType() const { return returnType; }
    int getLine() const override { return 0; }
    void execute(std::shared_ptr<Environment> env) override;
};

class SwitchStmt : public Stmt {
    std::unique_ptr<Expr> value;
    std::vector<std::pair<std::unique_ptr<Expr>, std::vector<std::unique_ptr<Stmt>>>> cases;
    std::vector<std::unique_ptr<Stmt>> defaultCase;
    int line;
public:
    SwitchStmt(std::unique_ptr<Expr> v, 
               std::vector<std::pair<std::unique_ptr<Expr>, std::vector<std::unique_ptr<Stmt>>>> c,
               std::vector<std::unique_ptr<Stmt>> d,
               int ln = 0)
        : value(std::move(v)), cases(std::move(c)), defaultCase(std::move(d)), line(ln) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class CatchClause {
public:
    std::string varName;
    std::string typeName;
    std::vector<std::unique_ptr<Stmt>> block;
};

class TryCatchStmt : public Stmt {
    std::vector<std::unique_ptr<Stmt>> tryBlock;
    std::vector<CatchClause> catches;
    int line;
public:
    TryCatchStmt(std::vector<std::unique_ptr<Stmt>> t, std::vector<CatchClause> c, int ln = 0)
        : tryBlock(std::move(t)), catches(std::move(c)), line(ln) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class ThrowStmt : public Stmt {
    std::unique_ptr<Expr> expr;
    int line;
public:
    ThrowStmt(std::unique_ptr<Expr> e, int ln = 0) : expr(std::move(e)), line(ln) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class ImportStmt : public Stmt {
    std::string path;
    bool isSystem;
    int line;
public:
    ImportStmt(const std::string& p, bool sys, int l = 0) : path(p), isSystem(sys), line(l) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
    const std::string& getPath() const { return path; }
    bool isSystemPath() const { return isSystem; }
};

class NamespaceStmt : public Stmt {
    std::string name;
    std::vector<std::unique_ptr<Stmt>> body;
    int line;
public:
    NamespaceStmt(const std::string& n, std::vector<std::unique_ptr<Stmt>> b, int l = 0)
        : name(n), body(std::move(b)), line(l) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class UsingStmt : public Stmt {
    std::vector<std::string> names;
    bool isNamespace;
    int line;
public:
    UsingStmt(const std::vector<std::string>& n, bool ns, int l = 0) : names(n), isNamespace(ns), line(l) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

class ExportStmt : public Stmt {
    std::unique_ptr<Stmt> declaration;
    int line;
public:
    ExportStmt(std::unique_ptr<Stmt> decl, int l = 0) : declaration(std::move(decl)), line(l) {}
    void execute(std::shared_ptr<Environment> env) override;
    int getLine() const override { return line; }
};

// ==================== 表达式实现（需在 Value 完整后） ====================

Value LiteralExpr::evaluate(std::shared_ptr<Environment> env) {
    (void)env;
    return value;
}

Value VariableExpr::evaluate(std::shared_ptr<Environment> env) {
    return env->get(names);
}

enum class NumericResult { INT, FLOAT };

static NumericResult getNumericResultType(const Value& a, const Value& b) {
    if (a.isFloat() || b.isFloat()) return NumericResult::FLOAT;
    return NumericResult::INT;
}

static int64_t bitwiseOperation(int64_t a, int64_t b, TokenKind op) {
    switch (op) {
        case TokenKind::TK_BIT_AND: return a & b;
        case TokenKind::TK_BIT_OR:  return a | b;
        case TokenKind::TK_BIT_XOR: return a ^ b;
        case TokenKind::TK_LEFT_SHIFT: {
            if (b < 0 || b >= 64) throw std::runtime_error("Shift amount out of range");
            return a << b;
        }
        case TokenKind::TK_RIGHT_SHIFT: {
            if (b < 0 || b >= 64) throw std::runtime_error("Shift amount out of range");
            return a >> b;
        }
        case TokenKind::TK_UNSIGNED_RIGHT_SHIFT: {
            if (b < 0 || b >= 64) throw std::runtime_error("Shift amount out of range");
            return static_cast<int64_t>(static_cast<uint64_t>(a) >> b);
        }
        default: throw std::runtime_error("Not a bitwise operator");
    }
}

static std::string operatorName(TokenKind op) {
    switch (op) {
        case TokenKind::TK_PLUS: return "operator+";
        case TokenKind::TK_MINUS: return "operator-";
        case TokenKind::TK_STAR: return "operator*";
        case TokenKind::TK_SLASH: return "operator/";
        case TokenKind::TK_PERCENT: return "operator%";
        case TokenKind::TK_EQUAL_EQUAL: return "operator==";
        case TokenKind::TK_NOT_EQUAL: return "operator!=";
        case TokenKind::TK_LESS: return "operator<";
        case TokenKind::TK_LESS_EQUAL: return "operator<=";
        case TokenKind::TK_GREATER: return "operator>";
        case TokenKind::TK_GREATER_EQUAL: return "operator>=";
        case TokenKind::TK_BIT_AND: return "operator&";
        case TokenKind::TK_BIT_OR: return "operator|";
        case TokenKind::TK_BIT_XOR: return "operator^";
        case TokenKind::TK_LEFT_SHIFT: return "operator<<";
        case TokenKind::TK_RIGHT_SHIFT: return "operator>>";
        case TokenKind::TK_UNSIGNED_RIGHT_SHIFT: return "operator>>>";
        default: return "";
    }
}

Value BinaryExpr::evaluate(std::shared_ptr<Environment> env) {
    Value leftVal = left->evaluate(env);
    if (op == TokenKind::TK_AND) {
        if (!isTruthy(leftVal)) return Value(false);
        Value rightVal = right->evaluate(env);
        return Value(isTruthy(rightVal));
    }
    if (op == TokenKind::TK_OR) {
        if (isTruthy(leftVal)) return Value(true);
        Value rightVal = right->evaluate(env);
        return Value(isTruthy(rightVal));
    }
    Value rightVal = right->evaluate(env);

    // 操作符重载检查：左操作数或右操作数是实例/结构体
    if (leftVal.isInstance() || leftVal.isStruct() || rightVal.isInstance() || rightVal.isStruct()) {
        std::string opName = operatorName(op);
        if (!opName.empty()) {
            // 优先左操作数
            if (leftVal.isInstance() || leftVal.isStruct()) {
                std::shared_ptr<InstanceObject> inst;
                if (leftVal.isInstance()) inst = leftVal.asInstance();
                else inst = leftVal.asStruct();
                auto method = inst->findMethod(opName);
                if (method) {
                    auto newEnv = method->closure->newChild();
                    newEnv->setCurrentInstance(inst);
                    if (method->params.size() != 1)
                        throw std::runtime_error("Operator method must have exactly one parameter");
                    newEnv->define(method->params[0], rightVal);
                    try {
                        for (auto& stmt : method->body)
                            stmt->execute(newEnv);
                    } catch (const ReturnException& ret) {
                        return ret.value;
                    }
                    return Value();
                }
            }
            // 尝试右操作数（如果左操作数没有重载）
            if (rightVal.isInstance() || rightVal.isStruct()) {
                std::shared_ptr<InstanceObject> inst;
                if (rightVal.isInstance()) inst = rightVal.asInstance();
                else inst = rightVal.asStruct();
                auto method = inst->findMethod(opName);
                if (method) {
                    auto newEnv = method->closure->newChild();
                    newEnv->setCurrentInstance(inst);
                    if (method->params.size() != 1)
                        throw std::runtime_error("Operator method must have exactly one parameter");
                    newEnv->define(method->params[0], leftVal);
                    try {
                        for (auto& stmt : method->body)
                            stmt->execute(newEnv);
                    } catch (const ReturnException& ret) {
                        return ret.value;
                    }
                    return Value();
                }
            }
        }
    }

    if (op == TokenKind::TK_PLUS && (leftVal.isString() || rightVal.isString())) {
        std::string l = leftVal.isString() ? leftVal.asString() : valueToString(leftVal);
        std::string r = rightVal.isString() ? rightVal.asString() : valueToString(rightVal);
        return Value(l + r);
    }

    if (leftVal.isNumber() && rightVal.isNumber() &&
        (op == TokenKind::TK_PLUS || op == TokenKind::TK_MINUS || op == TokenKind::TK_STAR ||
         op == TokenKind::TK_SLASH || op == TokenKind::TK_PERCENT)) {
        NumericResult resType = getNumericResultType(leftVal, rightVal);
        double l = leftVal.toDouble();
        double r = rightVal.toDouble();
        switch (op) {
            case TokenKind::TK_PLUS:
                if (resType == NumericResult::INT) return Value(static_cast<int64_t>(l + r));
                else return Value(l + r);
            case TokenKind::TK_MINUS:
                if (resType == NumericResult::INT) return Value(static_cast<int64_t>(l - r));
                else return Value(l - r);
            case TokenKind::TK_STAR:
                if (resType == NumericResult::INT) return Value(static_cast<int64_t>(l * r));
                else return Value(l * r);
            case TokenKind::TK_SLASH:
                if (r == 0) throw std::runtime_error("Division by zero at line " + std::to_string(line));
                return Value(l / r);
            case TokenKind::TK_PERCENT:
                if (r == 0) throw std::runtime_error("Modulo by zero at line " + std::to_string(line));
                if (resType == NumericResult::INT) return Value(static_cast<int64_t>(std::fmod(l, r)));
                else return Value(std::fmod(l, r));
            default: break;
        }
    }

    if (leftVal.isInt() && rightVal.isInt()) {
        int64_t l = leftVal.asInt();
        int64_t r = rightVal.asInt();
        switch (op) {
            case TokenKind::TK_BIT_AND:
            case TokenKind::TK_BIT_OR:
            case TokenKind::TK_BIT_XOR:
            case TokenKind::TK_LEFT_SHIFT:
            case TokenKind::TK_RIGHT_SHIFT:
            case TokenKind::TK_UNSIGNED_RIGHT_SHIFT:
                return Value(bitwiseOperation(l, r, op));
            default: break;
        }
    }

    switch (op) {
        case TokenKind::TK_EQUAL_EQUAL: {
            if (leftVal.type() != rightVal.type()) return Value(false);
            if (leftVal.isInt()) return Value(leftVal.asInt() == rightVal.asInt());
            if (leftVal.isFloat()) return Value(leftVal.asFloat() == rightVal.asFloat());
            if (leftVal.isBool()) return Value(leftVal.asBool() == rightVal.asBool());
            if (leftVal.isChar()) return Value(leftVal.asChar() == rightVal.asChar());
            if (leftVal.isWChar()) return Value(leftVal.asWChar() == rightVal.asWChar());
            if (leftVal.isString()) return Value(leftVal.asString() == rightVal.asString());
            if (leftVal.isNull()) return Value(true);
            if (leftVal.isInstance() && rightVal.isInstance())
                return Value(leftVal.asInstance() == rightVal.asInstance());
            if (leftVal.isStruct() && rightVal.isStruct())
                return Value(leftVal.asStruct() == rightVal.asStruct());
            if (leftVal.isClass() && rightVal.isClass())
                return Value(leftVal.asClass() == rightVal.asClass());
            if (leftVal.isInterface() && rightVal.isInterface())
                return Value(leftVal.asInterface() == rightVal.asInterface());
            if (leftVal.isFuture() && rightVal.isFuture())
                return Value(leftVal.asFuture() == rightVal.asFuture());
            if (leftVal.isNamespace() && rightVal.isNamespace())
                return Value(leftVal.asNamespace() == rightVal.asNamespace());
            if (leftVal.isPointer() && rightVal.isPointer())
                return Value(leftVal.asPointer() == rightVal.asPointer());
            return Value(false);
        }
        case TokenKind::TK_NOT_EQUAL: {
            if (leftVal.type() != rightVal.type()) return Value(true);
            if (leftVal.isInt()) return Value(leftVal.asInt() != rightVal.asInt());
            if (leftVal.isFloat()) return Value(leftVal.asFloat() != rightVal.asFloat());
            if (leftVal.isBool()) return Value(leftVal.asBool() != rightVal.asBool());
            if (leftVal.isChar()) return Value(leftVal.asChar() != rightVal.asChar());
            if (leftVal.isWChar()) return Value(leftVal.asWChar() != rightVal.asWChar());
            if (leftVal.isString()) return Value(leftVal.asString() != rightVal.asString());
            if (leftVal.isNull()) return Value(false);
            if (leftVal.isInstance() && rightVal.isInstance())
                return Value(leftVal.asInstance() != rightVal.asInstance());
            if (leftVal.isStruct() && rightVal.isStruct())
                return Value(leftVal.asStruct() != rightVal.asStruct());
            if (leftVal.isClass() && rightVal.isClass())
                return Value(leftVal.asClass() != rightVal.asClass());
            if (leftVal.isInterface() && rightVal.isInterface())
                return Value(leftVal.asInterface() != rightVal.asInterface());
            if (leftVal.isFuture() && rightVal.isFuture())
                return Value(leftVal.asFuture() != rightVal.asFuture());
            if (leftVal.isNamespace() && rightVal.isNamespace())
                return Value(leftVal.asNamespace() != rightVal.asNamespace());
            if (leftVal.isPointer() && rightVal.isPointer())
                return Value(leftVal.asPointer() != rightVal.asPointer());
            return Value(true);
        }
        case TokenKind::TK_LESS: {
            if (leftVal.isNumber() && rightVal.isNumber())
                return Value(leftVal.toDouble() < rightVal.toDouble());
            if (leftVal.isString() && rightVal.isString())
                return Value(leftVal.asString() < rightVal.asString());
            if (leftVal.isChar() && rightVal.isChar())
                return Value(leftVal.asChar() < rightVal.asChar());
            if (leftVal.isWChar() && rightVal.isWChar())
                return Value(leftVal.asWChar() < rightVal.asWChar());
            throw std::runtime_error("Operands must be numbers, strings, or chars for <");
        }
        case TokenKind::TK_LESS_EQUAL: {
            if (leftVal.isNumber() && rightVal.isNumber())
                return Value(leftVal.toDouble() <= rightVal.toDouble());
            if (leftVal.isString() && rightVal.isString())
                return Value(leftVal.asString() <= rightVal.asString());
            if (leftVal.isChar() && rightVal.isChar())
                return Value(leftVal.asChar() <= rightVal.asChar());
            if (leftVal.isWChar() && rightVal.isWChar())
                return Value(leftVal.asWChar() <= rightVal.asWChar());
            throw std::runtime_error("Operands must be numbers, strings, or chars for <=");
        }
        case TokenKind::TK_GREATER: {
            if (leftVal.isNumber() && rightVal.isNumber())
                return Value(leftVal.toDouble() > rightVal.toDouble());
            if (leftVal.isString() && rightVal.isString())
                return Value(leftVal.asString() > rightVal.asString());
            if (leftVal.isChar() && rightVal.isChar())
                return Value(leftVal.asChar() > rightVal.asChar());
            if (leftVal.isWChar() && rightVal.isWChar())
                return Value(leftVal.asWChar() > rightVal.asWChar());
            throw std::runtime_error("Operands must be numbers, strings, or chars for >");
        }
        case TokenKind::TK_GREATER_EQUAL: {
            if (leftVal.isNumber() && rightVal.isNumber())
                return Value(leftVal.toDouble() >= rightVal.toDouble());
            if (leftVal.isString() && rightVal.isString())
                return Value(leftVal.asString() >= rightVal.asString());
            if (leftVal.isChar() && rightVal.isChar())
                return Value(leftVal.asChar() >= rightVal.asChar());
            if (leftVal.isWChar() && rightVal.isWChar())
                return Value(leftVal.asWChar() >= rightVal.asWChar());
            throw std::runtime_error("Operands must be numbers, strings, or chars for >=");
        }
        default:
            throw std::runtime_error("Unknown binary operator at line " + std::to_string(line));
    }
}

Value UnaryExpr::evaluate(std::shared_ptr<Environment> env) {
    if (op == TokenKind::TK_INC || op == TokenKind::TK_DEC) {
        if (auto varExpr = dynamic_cast<VariableExpr*>(right.get())) {
            const std::vector<std::string>& names = varExpr->getNames();
            if (names.size() != 1) throw std::runtime_error("Cannot increment/decrement qualified name");
            const std::string& name = names[0];
            Value current = env->getSimple(name);
            if (current.isInt()) {
                int64_t delta = (op == TokenKind::TK_INC) ? 1 : -1;
                int64_t newVal = current.asInt() + delta;
                env->assign(name, Value(newVal));
                return Value(newVal);
            } else if (current.isFloat()) {
                double delta = (op == TokenKind::TK_INC) ? 1.0 : -1.0;
                double newVal = current.asFloat() + delta;
                env->assign(name, Value(newVal));
                return Value(newVal);
            } else {
                throw std::runtime_error("Operand of ++/-- must be a number at line " + std::to_string(line));
            }
        } else {
            throw std::runtime_error("Operand of ++/-- must be a variable at line " + std::to_string(line));
        }
    }

    if (op == TokenKind::TK_BIT_AND) {
        // 取地址
        if (auto varExpr = dynamic_cast<VariableExpr*>(right.get())) {
            const std::vector<std::string>& names = varExpr->getNames();
            if (names.size() != 1) throw std::runtime_error("Cannot take address of qualified name");
            const std::string& name = names[0];
            int64_t addr = env->getAddress(name);
            return Value(addr, true);
        } else {
            throw std::runtime_error("Can only take address of a variable");
        }
    }
    if (op == TokenKind::TK_STAR) {
        // 解引用
        Value ptrVal = right->evaluate(env);
        if (!ptrVal.isPointer())
            throw std::runtime_error("Cannot dereference non-pointer");
        int64_t addr = ptrVal.asPointer();
        Value* val = Environment::getValueByAddress(addr);
        if (!val) throw std::runtime_error("Dangling pointer");
        return *val;
    }

    Value rightVal = right->evaluate(env);

    // 一元操作符重载
    if (rightVal.isInstance() || rightVal.isStruct()) {
        std::string opName;
        if (op == TokenKind::TK_MINUS) opName = "operator-";
        else if (op == TokenKind::TK_NOT) opName = "operator!";
        else if (op == TokenKind::TK_BIT_NOT) opName = "operator~";
        if (!opName.empty()) {
            std::shared_ptr<InstanceObject> inst;
            if (rightVal.isInstance()) inst = rightVal.asInstance();
            else inst = rightVal.asStruct();
            auto method = inst->findMethod(opName);
            if (method) {
                auto newEnv = method->closure->newChild();
                newEnv->setCurrentInstance(inst);
                if (!method->params.empty())
                    throw std::runtime_error("Unary operator method must have no parameters");
                try {
                    for (auto& stmt : method->body)
                        stmt->execute(newEnv);
                } catch (const ReturnException& ret) {
                    return ret.value;
                }
                return Value();
            }
        }
    }

    switch (op) {
        case TokenKind::TK_MINUS:
            if (rightVal.isInt()) return Value(-rightVal.asInt());
            if (rightVal.isFloat()) return Value(-rightVal.asFloat());
            throw std::runtime_error("Operand must be a number for unary '-'");
        case TokenKind::TK_NOT:
            return Value(!isTruthy(rightVal));
        case TokenKind::TK_BIT_NOT:
            if (rightVal.isInt()) return Value(~rightVal.asInt());
            throw std::runtime_error("Operand must be an integer for bitwise NOT");
        default:
            throw std::runtime_error("Unknown unary operator");
    }
}

Value GroupingExpr::evaluate(std::shared_ptr<Environment> env) {
    return expr->evaluate(env);
}

Value CallExpr::evaluate(std::shared_ptr<Environment> env) {
    Value calleeVal = callee->evaluate(env);
    std::vector<Value> args;
    for (auto& arg : arguments) args.push_back(arg->evaluate(env));

    if (calleeVal.isFunction()) {
        auto func = asFunction(calleeVal);
        if (func->isAbstract && func->body.empty()) {
            throw std::runtime_error("Cannot call abstract method");
        }
        size_t paramCount = func->params.size();
        size_t argCount = args.size();
        if (argCount > paramCount)
            throw std::runtime_error("Too many arguments at line " + std::to_string(line));
        if (argCount < paramCount) {
            for (size_t i = argCount; i < paramCount; ++i) {
                if (i - argCount < func->defaultValues.size())
                    args.push_back(func->defaultValues[i - argCount]);
                else
                    throw std::runtime_error("Missing argument for parameter '" + func->params[i] + "' at line " + std::to_string(line));
            }
        }
        auto newEnv = func->closure->newChild();
        if (auto member = dynamic_cast<MemberExpr*>(callee.get())) {
            if (!member->isStaticAccess()) {
                Value objVal = member->getObject()->evaluate(env);
                if (objVal.isInstance() || objVal.isStruct()) {
                    newEnv->setCurrentInstance(objVal.isInstance() ? objVal.asInstance() : objVal.asStruct());
                    newEnv->setCurrentClass(objVal.isInstance() ? objVal.asInstance()->klass : objVal.asStruct()->klass);
                }
            }
        }
        for (size_t i = 0; i < paramCount; ++i)
            newEnv->define(func->params[i], args[i]);
        try {
            for (auto& stmt : func->body)
                stmt->execute(newEnv);
        } catch (const ReturnException& ret) {
            return ret.value;
        }
        return Value();
    }
    else if (calleeVal.isNative()) {
        auto native = asNative(calleeVal);
        if (native->arity >= 0 && static_cast<int>(args.size()) != native->arity)
            throw std::runtime_error("Native function '" + native->name + "' expects " + std::to_string(native->arity) + " arguments but got " + std::to_string(args.size()) + " at line " + std::to_string(line));
        return native->fn(args, env);
    }
    else {
        throw std::runtime_error("Can only call functions at line " + std::to_string(line));
    }
}

Value AssignExpr::evaluate(std::shared_ptr<Environment> env) {
    bool isCompound = (assignOp != TokenKind::TK_EQUAL);
    Value rhs = value->evaluate(env);

    // 处理赋值给变量
    if (auto varExpr = dynamic_cast<VariableExpr*>(target.get())) {
        const std::vector<std::string>& names = varExpr->getNames();
        if (names.size() != 1) throw std::runtime_error("Cannot assign to qualified name (yet)");
        const std::string& name = names[0];
        // 检查变量是否为实例，且定义了 operator=
        Value lhs = env->getSimple(name);
        if (lhs.isInstance() || lhs.isStruct()) {
            std::shared_ptr<InstanceObject> inst;
            if (lhs.isInstance()) inst = lhs.asInstance();
            else inst = lhs.asStruct();
            auto method = inst->findMethod("operator=");
            if (method) {
                auto newEnv = method->closure->newChild();
                newEnv->setCurrentInstance(inst);
                if (method->params.size() != 1)
                    throw std::runtime_error("operator= must have exactly one parameter");
                newEnv->define(method->params[0], rhs);
                try {
                    for (auto& stmt : method->body)
                        stmt->execute(newEnv);
                } catch (const ReturnException& ret) {
                    env->assign(name, ret.value); // 假设返回新值
                    return ret.value;
                }
                // 如果没有返回，认为 lhs 被修改，但我们需要重新获取 lhs
                Value newLhs = env->getSimple(name);
                return newLhs;
            }
        }
        if (!isCompound) {
            env->assign(name, rhs);
            return rhs;
        } else {
            Value lhs = env->getSimple(name);
            Value result;
            if (lhs.isNumber() && rhs.isNumber()) {
                double l = lhs.toDouble();
                double r = rhs.toDouble();
                NumericResult resType = getNumericResultType(lhs, rhs);
                switch (assignOp) {
                    case TokenKind::TK_PLUS_EQUAL:
                        if (resType == NumericResult::INT) result = Value(static_cast<int64_t>(l + r));
                        else result = Value(l + r);
                        break;
                    case TokenKind::TK_MINUS_EQUAL:
                        if (resType == NumericResult::INT) result = Value(static_cast<int64_t>(l - r));
                        else result = Value(l - r);
                        break;
                    case TokenKind::TK_STAR_EQUAL:
                        if (resType == NumericResult::INT) result = Value(static_cast<int64_t>(l * r));
                        else result = Value(l * r);
                        break;
                    case TokenKind::TK_SLASH_EQUAL:
                        if (r == 0) throw std::runtime_error("Division by zero at line " + std::to_string(line));
                        result = Value(l / r);
                        break;
                    case TokenKind::TK_PERCENT_EQUAL:
                        if (r == 0) throw std::runtime_error("Modulo by zero at line " + std::to_string(line));
                        if (resType == NumericResult::INT) result = Value(static_cast<int64_t>(std::fmod(l, r)));
                        else result = Value(std::fmod(l, r));
                        break;
                    default:
                        if (!lhs.isInt() || !rhs.isInt())
                            throw std::runtime_error("Bitwise assignment requires integer operands");
                        result = Value(bitwiseOperation(lhs.asInt(), rhs.asInt(),
                            assignOp == TokenKind::TK_AMP_EQUAL ? TokenKind::TK_BIT_AND :
                            assignOp == TokenKind::TK_PIPE_EQUAL ? TokenKind::TK_BIT_OR :
                            assignOp == TokenKind::TK_CARET_EQUAL ? TokenKind::TK_BIT_XOR :
                            assignOp == TokenKind::TK_LEFT_SHIFT_EQUAL ? TokenKind::TK_LEFT_SHIFT :
                            assignOp == TokenKind::TK_RIGHT_SHIFT_EQUAL ? TokenKind::TK_RIGHT_SHIFT :
                            assignOp == TokenKind::TK_UNSIGNED_RIGHT_SHIFT_EQUAL ? TokenKind::TK_UNSIGNED_RIGHT_SHIFT :
                            throw std::runtime_error("Unknown compound assignment")));
                        break;
                }
            } else {
                throw std::runtime_error("Operands must be numbers for compound assignment");
            }
            env->assign(name, result);
            return result;
        }
    }
    else if (auto indexExpr = dynamic_cast<IndexExpr*>(target.get())) {
        Expr* objExpr = indexExpr->getObject();
        Expr* idxExpr = indexExpr->getIndex();

        Value idxVal = idxExpr->evaluate(env);
        if (!idxVal.isInt())
            throw std::runtime_error("Array index must be an integer at line " + std::to_string(line));
        int64_t i = idxVal.asInt();

        if (auto varObj = dynamic_cast<VariableExpr*>(objExpr)) {
            const std::vector<std::string>& names = varObj->getNames();
            if (names.size() != 1) throw std::runtime_error("Array variable must be simple");
            const std::string& arrName = names[0];
            Value& varRef = env->getModifiable(arrName);
            if (!varRef.isArray())
                throw std::runtime_error("Variable is not an array at line " + std::to_string(line));
            std::vector<Value>& arrRef = varRef.asArray();
            if (i < 0 || i >= static_cast<int64_t>(arrRef.size()))
                throw std::runtime_error("Array index out of bounds at line " + std::to_string(line));

            if (!isCompound) {
                arrRef[i] = rhs;
                return rhs;
            } else {
                Value lhs = arrRef[i];
                Value result;
                if (lhs.isNumber() && rhs.isNumber()) {
                    double l = lhs.toDouble();
                    double r = rhs.toDouble();
                    NumericResult resType = getNumericResultType(lhs, rhs);
                    switch (assignOp) {
                        case TokenKind::TK_PLUS_EQUAL:
                            if (resType == NumericResult::INT) result = Value(static_cast<int64_t>(l + r));
                            else result = Value(l + r);
                            break;
                        case TokenKind::TK_MINUS_EQUAL:
                            if (resType == NumericResult::INT) result = Value(static_cast<int64_t>(l - r));
                            else result = Value(l - r);
                            break;
                        case TokenKind::TK_STAR_EQUAL:
                            if (resType == NumericResult::INT) result = Value(static_cast<int64_t>(l * r));
                            else result = Value(l * r);
                            break;
                        case TokenKind::TK_SLASH_EQUAL:
                            if (r == 0) throw std::runtime_error("Division by zero");
                            result = Value(l / r);
                            break;
                        case TokenKind::TK_PERCENT_EQUAL:
                            if (r == 0) throw std::runtime_error("Modulo by zero");
                            if (resType == NumericResult::INT) result = Value(static_cast<int64_t>(std::fmod(l, r)));
                            else result = Value(std::fmod(l, r));
                            break;
                        default:
                            if (!lhs.isInt() || !rhs.isInt())
                                throw std::runtime_error("Bitwise assignment requires integer operands");
                            result = Value(bitwiseOperation(lhs.asInt(), rhs.asInt(),
                                assignOp == TokenKind::TK_AMP_EQUAL ? TokenKind::TK_BIT_AND :
                                assignOp == TokenKind::TK_PIPE_EQUAL ? TokenKind::TK_BIT_OR :
                                assignOp == TokenKind::TK_CARET_EQUAL ? TokenKind::TK_BIT_XOR :
                                assignOp == TokenKind::TK_LEFT_SHIFT_EQUAL ? TokenKind::TK_LEFT_SHIFT :
                                assignOp == TokenKind::TK_RIGHT_SHIFT_EQUAL ? TokenKind::TK_RIGHT_SHIFT :
                                assignOp == TokenKind::TK_UNSIGNED_RIGHT_SHIFT_EQUAL ? TokenKind::TK_UNSIGNED_RIGHT_SHIFT :
                                throw std::runtime_error("Unknown compound assignment")));
                            break;
                    }
                } else {
                    throw std::runtime_error("Operands must be numbers for compound assignment");
                }
                arrRef[i] = result;
                return result;
            }
        } else {
            throw std::runtime_error("Complex array assignment not supported yet");
        }
    }
    else if (auto memberExpr = dynamic_cast<MemberExpr*>(target.get())) {
        Value objVal = memberExpr->getObject()->evaluate(env);
        const std::string& member = memberExpr->getMember();
        if (memberExpr->isStaticAccess()) {
            if (!objVal.isClass()) throw std::runtime_error("Static member access requires class");
            if (!isCompound) {
                env->assignMember(objVal, member, rhs);
                return rhs;
            } else {
                Value lhs = env->getMember(objVal, member);
                Value result;
                if (lhs.isNumber() && rhs.isNumber()) {
                    double l = lhs.toDouble();
                    double r = rhs.toDouble();
                    NumericResult resType = getNumericResultType(lhs, rhs);
                    switch (assignOp) {
                        case TokenKind::TK_PLUS_EQUAL:
                            if (resType == NumericResult::INT) result = Value(static_cast<int64_t>(l + r));
                            else result = Value(l + r);
                            break;
                        case TokenKind::TK_MINUS_EQUAL:
                            if (resType == NumericResult::INT) result = Value(static_cast<int64_t>(l - r));
                            else result = Value(l - r);
                            break;
                        case TokenKind::TK_STAR_EQUAL:
                            if (resType == NumericResult::INT) result = Value(static_cast<int64_t>(l * r));
                            else result = Value(l * r);
                            break;
                        case TokenKind::TK_SLASH_EQUAL:
                            if (r == 0) throw std::runtime_error("Division by zero");
                            result = Value(l / r);
                            break;
                        case TokenKind::TK_PERCENT_EQUAL:
                            if (r == 0) throw std::runtime_error("Modulo by zero");
                            if (resType == NumericResult::INT) result = Value(static_cast<int64_t>(std::fmod(l, r)));
                            else result = Value(std::fmod(l, r));
                            break;
                        default:
                            if (!lhs.isInt() || !rhs.isInt())
                                throw std::runtime_error("Bitwise assignment requires integer operands");
                            result = Value(bitwiseOperation(lhs.asInt(), rhs.asInt(),
                                assignOp == TokenKind::TK_AMP_EQUAL ? TokenKind::TK_BIT_AND :
                                assignOp == TokenKind::TK_PIPE_EQUAL ? TokenKind::TK_BIT_OR :
                                assignOp == TokenKind::TK_CARET_EQUAL ? TokenKind::TK_BIT_XOR :
                                assignOp == TokenKind::TK_LEFT_SHIFT_EQUAL ? TokenKind::TK_LEFT_SHIFT :
                                assignOp == TokenKind::TK_RIGHT_SHIFT_EQUAL ? TokenKind::TK_RIGHT_SHIFT :
                                assignOp == TokenKind::TK_UNSIGNED_RIGHT_SHIFT_EQUAL ? TokenKind::TK_UNSIGNED_RIGHT_SHIFT :
                                throw std::runtime_error("Unknown compound assignment")));
                            break;
                    }
                    env->assignMember(objVal, member, result);
                    return result;
                } else {
                    throw std::runtime_error("Operands must be numbers for compound assignment");
                }
            }
        } else {
            if (!objVal.isInstance() && !objVal.isStruct())
                throw std::runtime_error("Cannot assign to member of non-instance/struct");
            if (!isCompound) {
                env->assignMember(objVal, member, rhs);
                return rhs;
            } else {
                Value lhs = env->getMember(objVal, member);
                Value result;
                if (lhs.isNumber() && rhs.isNumber()) {
                    double l = lhs.toDouble();
                    double r = rhs.toDouble();
                    NumericResult resType = getNumericResultType(lhs, rhs);
                    switch (assignOp) {
                        case TokenKind::TK_PLUS_EQUAL:
                            if (resType == NumericResult::INT) result = Value(static_cast<int64_t>(l + r));
                            else result = Value(l + r);
                            break;
                        case TokenKind::TK_MINUS_EQUAL:
                            if (resType == NumericResult::INT) result = Value(static_cast<int64_t>(l - r));
                            else result = Value(l - r);
                            break;
                        case TokenKind::TK_STAR_EQUAL:
                            if (resType == NumericResult::INT) result = Value(static_cast<int64_t>(l * r));
                            else result = Value(l * r);
                            break;
                        case TokenKind::TK_SLASH_EQUAL:
                            if (r == 0) throw std::runtime_error("Division by zero");
                            result = Value(l / r);
                            break;
                        case TokenKind::TK_PERCENT_EQUAL:
                            if (r == 0) throw std::runtime_error("Modulo by zero");
                            if (resType == NumericResult::INT) result = Value(static_cast<int64_t>(std::fmod(l, r)));
                            else result = Value(std::fmod(l, r));
                            break;
                        default:
                            if (!lhs.isInt() || !rhs.isInt())
                                throw std::runtime_error("Bitwise assignment requires integer operands");
                            result = Value(bitwiseOperation(lhs.asInt(), rhs.asInt(),
                                assignOp == TokenKind::TK_AMP_EQUAL ? TokenKind::TK_BIT_AND :
                                assignOp == TokenKind::TK_PIPE_EQUAL ? TokenKind::TK_BIT_OR :
                                assignOp == TokenKind::TK_CARET_EQUAL ? TokenKind::TK_BIT_XOR :
                                assignOp == TokenKind::TK_LEFT_SHIFT_EQUAL ? TokenKind::TK_LEFT_SHIFT :
                                assignOp == TokenKind::TK_RIGHT_SHIFT_EQUAL ? TokenKind::TK_RIGHT_SHIFT :
                                assignOp == TokenKind::TK_UNSIGNED_RIGHT_SHIFT_EQUAL ? TokenKind::TK_UNSIGNED_RIGHT_SHIFT :
                                throw std::runtime_error("Unknown compound assignment")));
                            break;
                    }
                    env->assignMember(objVal, member, result);
                    return result;
                } else {
                    throw std::runtime_error("Operands must be numbers for compound assignment");
                }
            }
        }
    }
    else {
        throw std::runtime_error("Invalid assignment target at line " + std::to_string(line));
    }
}

Value PostfixExpr::evaluate(std::shared_ptr<Environment> env) {
    if (auto varExpr = dynamic_cast<VariableExpr*>(left.get())) {
        const std::vector<std::string>& names = varExpr->getNames();
        if (names.size() != 1) throw std::runtime_error("Cannot postfix increment/decrement qualified name");
        const std::string& name = names[0];
        Value current = env->getSimple(name);
        Value old = current;
        if (current.isInt()) {
            int64_t delta = (op == TokenKind::TK_INC) ? 1 : -1;
            int64_t newVal = current.asInt() + delta;
            env->assign(name, Value(newVal));
        } else if (current.isFloat()) {
            double delta = (op == TokenKind::TK_INC) ? 1.0 : -1.0;
            double newVal = current.asFloat() + delta;
            env->assign(name, Value(newVal));
        } else {
            throw std::runtime_error("Operand of ++/-- must be a number");
        }
        return old;
    } else {
        throw std::runtime_error("Operand of ++/-- must be a variable");
    }
}

Value ArrayLiteralExpr::evaluate(std::shared_ptr<Environment> env) {
    std::vector<Value> values;
    for (auto& elem : elements) {
        values.push_back(elem->evaluate(env));
    }
    return Value(values);
}

Value IndexExpr::evaluate(std::shared_ptr<Environment> env) {
    Value objVal = object->evaluate(env);
    Value idxVal = index->evaluate(env);
    if (!objVal.isArray())
        throw std::runtime_error("Index operator applied to non-array at line " + std::to_string(line));
    if (!idxVal.isInt())
        throw std::runtime_error("Array index must be an integer at line " + std::to_string(line));
    int64_t i = idxVal.asInt();
    const auto& arr = objVal.asArray();
    if (i < 0 || i >= static_cast<int64_t>(arr.size()))
        throw std::runtime_error("Array index out of bounds at line " + std::to_string(line));
    return arr[i];
}

Value TernaryExpr::evaluate(std::shared_ptr<Environment> env) {
    Value condVal = cond->evaluate(env);
    if (isTruthy(condVal)) {
        return thenExpr->evaluate(env);
    } else {
        return elseExpr->evaluate(env);
    }
}

Value IsExpr::evaluate(std::shared_ptr<Environment> env) {
    Value val = left->evaluate(env);
    if (typeName == "int") return Value(val.isInt());
    if (typeName == "float") return Value(val.isFloat());
    if (typeName == "bool") return Value(val.isBool());
    if (typeName == "char") return Value(val.isChar());
    if (typeName == "wchar") return Value(val.isWChar());
    if (typeName == "string") return Value(val.isString());
    if (typeName == "array") return Value(val.isArray());
    if (typeName == "function") return Value(val.isFunction());
    if (typeName == "null") return Value(val.isNull());
    if (typeName == "class") return Value(val.isClass());
    if (typeName == "interface") return Value(val.isInterface());
    if (typeName == "instance") return Value(val.isInstance());
    if (typeName == "future") return Value(val.isFuture());
    if (typeName == "namespace") return Value(val.isNamespace());
    if (typeName == "pointer") return Value(val.isPointer());
    if (typeName == "struct") return Value(val.isStruct());
    return Value(false);
}

Value AsExpr::evaluate(std::shared_ptr<Environment> env) {
    Value val = left->evaluate(env);
    if (typeName == "int" && val.isInt()) return val;
    if (typeName == "float" && val.isFloat()) return val;
    if (typeName == "bool" && val.isBool()) return val;
    if (typeName == "char" && val.isChar()) return val;
    if (typeName == "wchar" && val.isWChar()) return val;
    if (typeName == "string" && val.isString()) return val;
    if (typeName == "array" && val.isArray()) return val;
    if (typeName == "function" && val.isFunction()) return val;
    if (typeName == "class" && val.isClass()) return val;
    if (typeName == "interface" && val.isInterface()) return val;
    if (typeName == "instance" && val.isInstance()) return val;
    if (typeName == "future" && val.isFuture()) return val;
    if (typeName == "namespace" && val.isNamespace()) return val;
    if (typeName == "pointer" && val.isPointer()) return val;
    if (typeName == "struct" && val.isStruct()) return val;
    return Value();
}

Value TypeofExpr::evaluate(std::shared_ptr<Environment> env) {
    Value val = expr->evaluate(env);
    switch (val.type()) {
        case ValueType::NULL_TYPE: return Value(std::string("null"));
        case ValueType::INT: return Value(std::string("int"));
        case ValueType::FLOAT: return Value(std::string("float"));
        case ValueType::BOOL: return Value(std::string("bool"));
        case ValueType::CHAR: return Value(std::string("char"));
        case ValueType::WCHAR: return Value(std::string("wchar"));
        case ValueType::STRING: return Value(std::string("string"));
        case ValueType::FUNCTION: return Value(std::string("function"));
        case ValueType::NATIVE: return Value(std::string("native"));
        case ValueType::ARRAY: return Value(std::string("array"));
        case ValueType::CLASS: return Value(std::string("class"));
        case ValueType::INTERFACE: return Value(std::string("interface"));
        case ValueType::INSTANCE: return Value(std::string("instance"));
        case ValueType::FUTURE: return Value(std::string("future"));
        case ValueType::NAMESPACE: return Value(std::string("namespace"));
        case ValueType::POINTER: return Value(std::string("pointer"));
        case ValueType::STRUCT: return Value(std::string("struct"));
        default: return Value(std::string("unknown"));
    }
}

// ==================== 新增表达式实现 ====================

Value MemberExpr::evaluate(std::shared_ptr<Environment> env) {
    if (isStatic) {
        if (auto varExpr = dynamic_cast<VariableExpr*>(object.get())) {
            auto names = varExpr->getNames();
            if (names.size() != 1) throw std::runtime_error("Invalid class name");
            Value classVal = env->get(names);
            if (!classVal.isClass()) throw std::runtime_error("Not a class");
            return env->getMember(classVal, member);
        } else {
            throw std::runtime_error("Static member access requires class name");
        }
    } else {
        Value objVal = object->evaluate(env);
        return env->getMember(objVal, member);
    }
}

Value NewExpr::evaluate(std::shared_ptr<Environment> env) {
    Value classVal = env->get({className});
    if (!classVal.isClass() && !classVal.isStruct())
        throw std::runtime_error("Class/struct not found: " + className);
    bool isStruct = classVal.isStruct();
    auto klass = isStruct ? classVal.asStruct()->klass : classVal.asClass();
    
    if (!isStruct && klass->isAbstract) {
        throw std::runtime_error("Cannot instantiate abstract class");
    }
    
    auto instance = std::make_shared<InstanceObject>(klass);
    
    for (const auto& fieldName : klass->fieldNames) {
        auto it = klass->fieldInitializers.find(fieldName);
        if (it != klass->fieldInitializers.end()) {
            auto fieldEnv = env->newChild();
            fieldEnv->setCurrentInstance(instance);
            instance->fields[fieldName] = it->second->evaluate(fieldEnv);
        } else {
            instance->fields[fieldName] = Value();
        }
    }
    
    auto ctor = klass->methods.find("constructor");
    if (ctor != klass->methods.end()) {
        std::vector<Value> args;
        for (auto& arg : arguments) args.push_back(arg->evaluate(env));
        auto func = ctor->second;
        auto newEnv = func->closure->newChild();
        newEnv->setCurrentInstance(instance);
        for (size_t i = 0; i < func->params.size(); ++i) {
            Value argVal = (i < args.size()) ? args[i] : func->defaultValues[i];
            newEnv->define(func->params[i], argVal);
        }
        try {
            for (auto& stmt : func->body) stmt->execute(newEnv);
        } catch (const ReturnException&) {}
    }
    
    if (isStruct) {
        return Value(instance, true);
    } else {
        return Value(instance);
    }
}

Value AsyncExpr::evaluate(std::shared_ptr<Environment> env) {
    // 使用 std::async 启动任务
    std::shared_future<Value> future = std::async(std::launch::async, [env, this]() {
        try {
            return expr->evaluate(env);
        } catch (...) {
            return Value();
        }
    });
    auto futObj = std::make_shared<FutureObject>(future);
    return Value(futObj);
}

Value AwaitExpr::evaluate(std::shared_ptr<Environment> env) {
    Value futVal = futureExpr->evaluate(env);
    if (!futVal.isFuture()) throw std::runtime_error("await requires a future");
    auto futObj = futVal.asFuture();
    return futObj->get();
}

Value SizeofExpr::evaluate(std::shared_ptr<Environment> env) {
    Value val = expr->evaluate(env);
    return Value(static_cast<int64_t>(valueSizeOf(val)));
}

Value ThisExpr::evaluate(std::shared_ptr<Environment> env) {
    auto inst = env->getCurrentInstance();
    if (!inst) throw std::runtime_error("'this' used outside of instance method");
    return Value(inst);
}

Value SuperExpr::evaluate(std::shared_ptr<Environment> env) {
    auto inst = env->getCurrentInstance();
    if (!inst) throw std::runtime_error("'super' used outside of instance method");
    auto superClass = inst->klass->superClass;
    if (!superClass) throw std::runtime_error("No superclass");
    auto superInstance = std::make_shared<InstanceObject>(superClass);
    return Value(superInstance);
}

Value RangeExpr::evaluate(std::shared_ptr<Environment> env) {
    Value startVal = start->evaluate(env);
    Value endVal = end->evaluate(env);
    if (!startVal.isInt() || !endVal.isInt())
        throw std::runtime_error("Range bounds must be integers at line " + std::to_string(line));
    int64_t s = startVal.asInt();
    int64_t e = endVal.asInt();
    std::vector<Value> result;
    if (inclusive) {
        for (int64_t i = s; i <= e; ++i)
            result.push_back(Value(i));
    } else {
        for (int64_t i = s; i < e; ++i)
            result.push_back(Value(i));
    }
    return Value(result);
}

Value LambdaExpr::evaluate(std::shared_ptr<Environment> env) {
    auto func = std::make_shared<UserFunction>();
    func->params = params;
    auto returnStmt = std::make_unique<ReturnStmt>(std::move(body), line);
    std::vector<std::unique_ptr<Stmt>> bodyStmts;
    bodyStmts.push_back(std::move(returnStmt));
    func->body = std::move(bodyStmts);
    func->closure = env;
    return Value(func);
}

Value AddressOfExpr::evaluate(std::shared_ptr<Environment> env) {
    if (auto varExpr = dynamic_cast<VariableExpr*>(expr.get())) {
        const std::vector<std::string>& names = varExpr->getNames();
        if (names.size() != 1) throw std::runtime_error("Cannot take address of qualified name");
        const std::string& name = names[0];
        int64_t addr = env->getAddress(name);
        return Value(addr, true);
    } else {
        throw std::runtime_error("Can only take address of a variable");
    }
}

Value DerefExpr::evaluate(std::shared_ptr<Environment> env) {
    Value ptrVal = expr->evaluate(env);
    if (!ptrVal.isPointer())
        throw std::runtime_error("Cannot dereference non-pointer");
    int64_t addr = ptrVal.asPointer();
    Value* val = Environment::getValueByAddress(addr);
    if (!val) throw std::runtime_error("Dangling pointer");
    return *val;
}

Value OperatorCallExpr::evaluate(std::shared_ptr<Environment> env) {
    (void)env;
    throw std::runtime_error("OperatorCallExpr should not be used directly");
}

// ==================== 语句实现 ====================

void ExpressionStmt::execute(std::shared_ptr<Environment> env) {
    expr->evaluate(env);
}

// 辅助函数：递归构建多维数组
static Value buildMultiDimArray(const std::vector<int64_t>& dims, size_t index, const std::vector<Value>* initData = nullptr, size_t* dataPos = nullptr) {
    if (index == dims.size()) {
        // 到达最内层，返回元素值（如果有初始化数据）
        if (initData && dataPos && *dataPos < initData->size()) {
            return (*initData)[(*dataPos)++].deepCopy();
        } else {
            return Value(); // 默认零值
        }
    }
    int64_t size = dims[index];
    std::vector<Value> arr;
    arr.reserve(size);
    for (int64_t i = 0; i < size; ++i) {
        arr.push_back(buildMultiDimArray(dims, index + 1, initData, dataPos));
    }
    return Value(arr);
}

void VarDeclStmt::execute(std::shared_ptr<Environment> env) {
    Value val;
    if (!dimensions.empty()) {
        // 数组声明
        std::vector<int64_t> dimSizes;
        for (auto& dimExpr : dimensions) {
            if (dimExpr) {
                Value sizeVal = dimExpr->evaluate(env);
                if (!sizeVal.isInt())
                    throw std::runtime_error("Array dimension must be an integer at line " + std::to_string(line));
                int64_t sz = sizeVal.asInt();
                if (sz < 0)
                    throw std::runtime_error("Array dimension cannot be negative at line " + std::to_string(line));
                dimSizes.push_back(sz);
            } else {
                // 空维度 [] 表示由初始化列表推断
                dimSizes.push_back(-1); // 标记为待推断
            }
        }

        std::vector<Value> initFlat;
        if (initializer) {
            Value initVal = initializer->evaluate(env);
            if (!initVal.isArray())
                throw std::runtime_error("Array initializer must be an array literal at line " + std::to_string(line));
            // 将嵌套数组展平
            std::function<void(const Value&)> flatten = [&](const Value& v) {
                if (v.isArray()) {
                    for (const auto& elem : v.asArray()) flatten(elem);
                } else {
                    initFlat.push_back(v);
                }
            };
            flatten(initVal);
        }

        // 处理推断维度
        for (size_t i = 0; i < dimSizes.size(); ++i) {
            if (dimSizes[i] == -1) {
                // 根据初始化列表推断该维度大小
                if (initFlat.empty())
                    throw std::runtime_error("Cannot infer array dimension without initializer at line " + std::to_string(line));
                // 计算该维度应有多少个元素
                int64_t total = 1;
                for (size_t j = i; j < dimSizes.size(); ++j) {
                    if (dimSizes[j] != -1) total *= dimSizes[j];
                }
                int64_t inferred = static_cast<int64_t>(initFlat.size()) / total;
                if (inferred * total != static_cast<int64_t>(initFlat.size()))
                    throw std::runtime_error("Initializer size does not match array dimensions at line " + std::to_string(line));
                dimSizes[i] = inferred;
            }
        }

        size_t dataPos = 0;
        val = buildMultiDimArray(dimSizes, 0, &initFlat, &dataPos);
        m_typeName = "array";
    } else {
        // 普通变量
        if (initializer) {
            val = initializer->evaluate(env);
        } else {
            // 根据类型生成默认值
            if (m_typeName == "int") val = Value(static_cast<int64_t>(0));
            else if (m_typeName == "float") val = Value(0.0);
            else if (m_typeName == "bool") val = Value(false);
            else if (m_typeName == "char") val = Value('\0');
            else if (m_typeName == "wchar") val = Value(char32_t(0));
            else if (m_typeName == "string") val = Value(std::string(""));
            else if (m_typeName == "array") val = Value(std::vector<Value>());
            else val = Value(); // null
        }
    }
    // const 变量必须有初始化（已在解析时检查）
    env->define(name, val, m_isMutable, m_typeName, access, env->getCurrentClass(), m_isDynamic);
}

void BlockStmt::execute(std::shared_ptr<Environment> env) {
    auto blockEnv = env->newChild();
    for (auto& stmt : statements) {
        stmt->execute(blockEnv);
    }
}

void IfStmt::execute(std::shared_ptr<Environment> env) {
    if (isTruthy(condition->evaluate(env)))
        thenBranch->execute(env);
    else if (elseBranch)
        elseBranch->execute(env);
}

void WhileStmt::execute(std::shared_ptr<Environment> env) {
    while (isTruthy(condition->evaluate(env))) {
        try {
            body->execute(env);
        } catch (const BreakException& be) {
            if (be.label.empty()) break;
            else throw;
        } catch (const ContinueException& ce) {
            if (ce.label.empty()) continue;
            else throw;
        }
    }
}

void DoWhileStmt::execute(std::shared_ptr<Environment> env) {
    do {
        try {
            body->execute(env);
        } catch (const BreakException& be) {
            if (be.label.empty()) break;
            else throw;
        } catch (const ContinueException& ce) {
            if (ce.label.empty()) continue;
            else throw;
        }
    } while (isTruthy(condition->evaluate(env)));
}

void ForStmt::execute(std::shared_ptr<Environment> env) {
    auto loopEnv = env->newChild();
    if (init) init->execute(loopEnv);
    while (true) {
        if (condition) {
            Value condVal = condition->evaluate(loopEnv);
            if (!isTruthy(condVal)) break;
        }
        try {
            body->execute(loopEnv);
        } catch (const BreakException& be) {
            if (be.label.empty()) break;
            else throw;
        } catch (const ContinueException& ce) {
            if (ce.label.empty()) {
                if (increment) increment->evaluate(loopEnv);
                continue;
            } else throw;
        }
        if (increment) increment->evaluate(loopEnv);
    }
}

void ForInStmt::execute(std::shared_ptr<Environment> env) {
    Value iterVal = iterable->evaluate(env);
    if (!iterVal.isArray()) {
        throw std::runtime_error("For-in loop requires array");
    }
    const auto& arr = iterVal.asArray();
    auto loopEnv = env->newChild();
    for (size_t i = 0; i < arr.size(); ++i) {
        loopEnv->define(varName, arr[i]);
        try {
            body->execute(loopEnv);
        } catch (const BreakException& be) {
            if (be.label.empty()) break;
            else throw;
        } catch (const ContinueException& ce) {
            if (ce.label.empty()) continue;
            else throw;
        }
    }
}

void ReturnStmt::execute(std::shared_ptr<Environment> env) {
    if (value)
        throw ReturnException(value->evaluate(env), getLine());
    else
        throw ReturnException(Value(), getLine());
}

void BreakStmt::execute(std::shared_ptr<Environment> env) {
    (void)env;
    throw BreakException(label);
}

void ContinueStmt::execute(std::shared_ptr<Environment> env) {
    (void)env;
    throw ContinueException(label);
}

void FunctionDeclStmt::execute(std::shared_ptr<Environment> env) {
    auto func = std::make_shared<UserFunction>();
    func->params = params;
    for (auto& defExpr : defaultValues) {
        if (defExpr) {
            func->defaultValues.push_back(defExpr->evaluate(env));
        } else {
            func->defaultValues.push_back(Value());
        }
    }
    func->body = std::move(body);
    func->closure = env;
    func->returnType = returnType;
    
    for (const auto& mod : modifiers) {
        if (mod == "static") func->isStatic = true;
        else if (mod == "final") func->isFinal = true;
        else if (mod == "abstract") func->isAbstract = true;
        else if (mod == "virtual") func->isVirtual = true;
        else if (mod == "override") func->isOverride = true;
        else if (mod == "public" || mod == "private" || mod == "protected" || mod == "internal")
            func->access = mod;
    }
    
    if (func->isAbstract && !func->body.empty()) {
        throw std::runtime_error("Abstract method cannot have a body");
    }
    
    env->define(name, Value(func), true, "", func->access, env->getCurrentClass());
}

void SwitchStmt::execute(std::shared_ptr<Environment> env) {
    Value switchVal = value->evaluate(env);
    bool matched = false;
    for (auto& casePair : cases) {
        Value caseVal = casePair.first->evaluate(env);
        bool equal = false;
        if (switchVal.type() == caseVal.type()) {
            if (switchVal.isInt()) equal = (switchVal.asInt() == caseVal.asInt());
            else if (switchVal.isFloat()) equal = (switchVal.asFloat() == caseVal.asFloat());
            else if (switchVal.isBool()) equal = (switchVal.asBool() == caseVal.asBool());
            else if (switchVal.isChar()) equal = (switchVal.asChar() == caseVal.asChar());
            else if (switchVal.isWChar()) equal = (switchVal.asWChar() == caseVal.asWChar());
            else if (switchVal.isString()) equal = (switchVal.asString() == caseVal.asString());
            else if (switchVal.isClass()) equal = (switchVal.asClass() == caseVal.asClass());
            else if (switchVal.isInterface()) equal = (switchVal.asInterface() == caseVal.asInterface());
            else if (switchVal.isInstance()) equal = (switchVal.asInstance() == caseVal.asInstance());
            else if (switchVal.isStruct()) equal = (switchVal.asStruct() == caseVal.asStruct());
            else if (switchVal.isFuture()) equal = (switchVal.asFuture() == caseVal.asFuture());
            else if (switchVal.isNamespace()) equal = (switchVal.asNamespace() == caseVal.asNamespace());
            else if (switchVal.isPointer()) equal = (switchVal.asPointer() == caseVal.asPointer());
        }
        if (equal || matched) {
            matched = true;
            for (auto& stmt : casePair.second) {
                stmt->execute(env);
            }
        }
    }
    if (!matched && !defaultCase.empty()) {
        for (auto& stmt : defaultCase) {
            stmt->execute(env);
        }
    }
}

void TryCatchStmt::execute(std::shared_ptr<Environment> env) {
    try {
        auto tryEnv = env->newChild();
        for (auto& stmt : tryBlock) {
            stmt->execute(tryEnv);
        }
    } catch (const ThrowException& e) {
        bool caught = false;
        for (auto& catchClause : catches) {
            // 检查类型是否匹配
            if (catchClause.typeName.empty() || env->isTypeCompatible(e.value, catchClause.typeName)) {
                auto catchEnv = env->newChild();
                catchEnv->define(catchClause.varName, e.value);
                for (auto& stmt : catchClause.block) {
                    stmt->execute(catchEnv);
                }
                caught = true;
                break;
            }
        }
        if (!caught) throw; // 重新抛出
    } catch (const std::exception& e) {
        // 处理原生异常
        Value exVal = Value(std::string(e.what()));
        for (auto& catchClause : catches) {
            if (catchClause.typeName.empty() || env->isTypeCompatible(exVal, catchClause.typeName)) {
                auto catchEnv = env->newChild();
                catchEnv->define(catchClause.varName, exVal);
                for (auto& stmt : catchClause.block) {
                    stmt->execute(catchEnv);
                }
                return;
            }
        }
        throw;
    } catch (...) {
        Value exVal = Value(std::string("Unknown native exception"));
        for (auto& catchClause : catches) {
            if (catchClause.typeName.empty() || env->isTypeCompatible(exVal, catchClause.typeName)) {
                auto catchEnv = env->newChild();
                catchEnv->define(catchClause.varName, exVal);
                for (auto& stmt : catchClause.block) {
                    stmt->execute(catchEnv);
                }
                return;
            }
        }
        throw;
    }
}

void ThrowStmt::execute(std::shared_ptr<Environment> env) {
    Value val = expr->evaluate(env);
    throw ThrowException(val, getLine());
}

void ImportStmt::execute(std::shared_ptr<Environment> env) {
    (void)env;
    throw std::runtime_error("ImportStmt should be handled by interpreter");
}

void NamespaceStmt::execute(std::shared_ptr<Environment> env) {
    auto nsEnv = std::make_shared<Environment>(env, name);
    for (auto& stmt : body) {
        stmt->execute(nsEnv);
    }
    env->define(name, Value(nsEnv), false, "", "public");
}

void UsingStmt::execute(std::shared_ptr<Environment> env) {
    if (isNamespace) {
        if (names.size() != 1) throw std::runtime_error("using namespace expects a single name");
        Value nsVal = env->get({names[0]});
        if (!nsVal.isNamespace()) throw std::runtime_error("Not a namespace");
        auto nsEnv = nsVal.asNamespace();
        auto exports = nsEnv->getExports();
        for (const auto& pair : exports) {
            env->define(pair.first, pair.second, true, "", "public");
        }
    } else {
        throw std::runtime_error("using declaration not implemented");
    }
}

void ExportStmt::execute(std::shared_ptr<Environment> env) {
    declaration->execute(env);
}

// ==================== 新增语句节点实现 ====================

void EnumDeclStmt::execute(std::shared_ptr<Environment> env) {
    auto enumObj = std::make_shared<ClassObject>(name);
    enumObj->isFinal = true;
    enumObj->isAbstract = false;
    int64_t currentVal = 0;
    for (auto& [id, expr] : enumerators) {
        Value val;
        if (expr) {
            val = expr->evaluate(env);
            if (!val.isInt()) throw std::runtime_error("Enumerator value must be integer");
            currentVal = val.asInt();
        } else {
            val = Value(currentVal);
        }
        enumObj->staticFields[id] = val;
        currentVal++;
    }
    env->define(name, Value(enumObj), true, "", "public");
}

void StructDeclStmt::execute(std::shared_ptr<Environment> env) {
    auto klass = std::make_shared<ClassObject>(name);
    klass->isStruct = true;
    for (auto& mod : modifiers) {
        if (mod == "final") klass->isFinal = true;
    }
    auto structEnv = env->newChild();
    structEnv->setCurrentClass(klass);
    
    std::map<std::string, std::shared_ptr<UserFunction>> declaredMethods;
    
    for (auto& stmt : body) {
        if (auto field = dynamic_cast<FieldDeclStmt*>(stmt.get())) {
            bool isStatic = false;
            std::string access = "public";
            for (auto& mod : field->modifiers) {
                if (mod == "static") isStatic = true;
                else if (mod == "public" || mod == "private" || mod == "protected") access = mod;
            }
            if (isStatic) {
                Value initVal = field->initializer ? field->initializer->evaluate(structEnv) : Value();
                klass->staticFields[field->name] = initVal;
            } else {
                klass->fieldNames.push_back(field->name);
                if (field->initializer) {
                    klass->fieldInitializers[field->name] = std::move(field->initializer);
                }
            }
        } else if (auto method = dynamic_cast<FunctionDeclStmt*>(stmt.get())) {
            bool isStatic = false;
            bool isFinal = false;
            bool isAbstract = false;
            bool isVirtual = false;
            bool isOverride = false;
            std::string access = "public";
            for (auto& mod : method->getModifiers()) {
                if (mod == "static") isStatic = true;
                else if (mod == "final") isFinal = true;
                else if (mod == "abstract") isAbstract = true;
                else if (mod == "virtual") isVirtual = true;
                else if (mod == "override") isOverride = true;
                else if (mod == "public" || mod == "private" || mod == "protected") access = mod;
            }
            
            auto func = std::make_shared<UserFunction>();
            func->params = method->getParams();
            auto defaultExprs = method->moveDefaultValues();
            for (auto& defExpr : defaultExprs) {
                if (defExpr) {
                    func->defaultValues.push_back(defExpr->evaluate(structEnv));
                } else {
                    func->defaultValues.push_back(Value());
                }
            }
            func->body = method->moveBody();
            func->closure = structEnv;
            func->isStatic = isStatic;
            func->isFinal = isFinal;
            func->isAbstract = isAbstract;
            func->isVirtual = isVirtual;
            func->isOverride = isOverride;
            func->access = access;
            func->returnType = method->getReturnType();
            
            declaredMethods[method->getName()] = func;
        }
    }
    
    for (auto& pair : declaredMethods) {
        if (pair.second->isStatic) {
            klass->staticMethods[pair.first] = pair.second;
        } else {
            klass->methods[pair.first] = pair.second;
        }
    }
    
    env->define(name, Value(klass), true, "", "public");
}

void ExtensionDeclStmt::execute(std::shared_ptr<Environment> env) {
    Value targetVal = env->get({typeName});
    if (!targetVal.isClass() && !targetVal.isStruct())
        throw std::runtime_error("Cannot extend non-class/struct type: " + typeName);
    std::shared_ptr<ClassObject> klass;
    if (targetVal.isClass()) klass = targetVal.asClass();
    else klass = targetVal.asStruct()->klass;
    
    auto extEnv = env->newChild();
    extEnv->setCurrentClass(klass);
    
    for (auto& stmt : methods) {
        if (auto method = dynamic_cast<FunctionDeclStmt*>(stmt.get())) {
            auto func = std::make_shared<UserFunction>();
            func->params = method->getParams();
            auto defaultExprs = method->moveDefaultValues();
            for (auto& defExpr : defaultExprs) {
                if (defExpr) {
                    func->defaultValues.push_back(defExpr->evaluate(extEnv));
                } else {
                    func->defaultValues.push_back(Value());
                }
            }
            func->body = method->moveBody();
            func->closure = extEnv;
            func->isStatic = false;
            func->isFinal = false;
            func->isAbstract = false;
            func->isVirtual = true;
            func->isOverride = false;
            func->access = "public";
            func->returnType = method->getReturnType();
            func->isExtension = true; // 标记为扩展方法
            
            klass->methods[method->getName()] = func;
        } else {
            throw std::runtime_error("Extension can only contain methods");
        }
    }
}

void ClassDeclStmt::execute(std::shared_ptr<Environment> env) {
    auto klass = std::make_shared<ClassObject>(name);
    
    for (auto& mod : modifiers) {
        if (mod == "abstract") klass->isAbstract = true;
        if (mod == "final") klass->isFinal = true;
    }
    
    if (!superClassName.empty()) {
        Value superVal = env->get({superClassName});
        if (!superVal.isClass()) throw std::runtime_error("Superclass not found");
        auto superClass = superVal.asClass();
        if (superClass->isFinal) {
            throw std::runtime_error("Cannot inherit from final class '" + superClassName + "'");
        }
        klass->superClass = superClass;
        // 复制父类的方法和字段
        klass->methods = superClass->methods;
        klass->fieldNames = superClass->fieldNames;
    }
    
    std::vector<std::shared_ptr<InterfaceObject>> interfaces;
    for (const auto& ifaceName : interfaceNames) {
        Value ifaceVal = env->get({ifaceName});
        if (!ifaceVal.isInterface()) throw std::runtime_error("Interface not found: " + ifaceName);
        auto iface = ifaceVal.asInterface();
        interfaces.push_back(iface);
    }
    klass->interfaces = interfaces;
    
    auto classEnv = env->newChild();
    classEnv->setCurrentClass(klass);
    
    std::map<std::string, std::shared_ptr<UserFunction>> declaredMethods;
    
    for (auto& stmt : body) {
        if (auto field = dynamic_cast<FieldDeclStmt*>(stmt.get())) {
            bool isStatic = false;
            std::string access = "public";
            for (auto& mod : field->modifiers) {
                if (mod == "static") isStatic = true;
                else if (mod == "public" || mod == "private" || mod == "protected") access = mod;
            }
            if (isStatic) {
                Value initVal = field->initializer ? field->initializer->evaluate(classEnv) : Value();
                klass->staticFields[field->name] = initVal;
            } else {
                klass->fieldNames.push_back(field->name);
                if (field->initializer) {
                    klass->fieldInitializers[field->name] = std::move(field->initializer);
                }
            }
        } else if (auto method = dynamic_cast<FunctionDeclStmt*>(stmt.get())) {
            bool isStatic = false;
            bool isFinal = false;
            bool isAbstract = false;
            bool isVirtual = false;
            bool isOverride = false;
            std::string access = "public";
            for (auto& mod : method->getModifiers()) {
                if (mod == "static") isStatic = true;
                else if (mod == "final") isFinal = true;
                else if (mod == "abstract") isAbstract = true;
                else if (mod == "virtual") isVirtual = true;
                else if (mod == "override") isOverride = true;
                else if (mod == "public" || mod == "private" || mod == "protected") access = mod;
            }
            
            if (isOverride) {
                bool found = false;
                if (klass->superClass) {
                    auto it = klass->superClass->methods.find(method->getName());
                    if (it != klass->superClass->methods.end() && it->second->isVirtual) {
                        found = true;
                    }
                }
                if (!found) {
                    throw std::runtime_error("Method '" + method->getName() + "' marked override but does not override any virtual method");
                }
            }
            
            auto func = std::make_shared<UserFunction>();
            func->params = method->getParams();
            auto defaultExprs = method->moveDefaultValues();
            for (auto& defExpr : defaultExprs) {
                if (defExpr) {
                    func->defaultValues.push_back(defExpr->evaluate(classEnv));
                } else {
                    func->defaultValues.push_back(Value());
                }
            }
            func->body = method->moveBody();
            func->closure = classEnv;
            func->isStatic = isStatic;
            func->isFinal = isFinal;
            func->isAbstract = isAbstract;
            func->isVirtual = isVirtual;
            func->isOverride = isOverride;
            func->access = access;
            func->returnType = method->getReturnType();
            
            declaredMethods[method->getName()] = func;
        }
    }
    
    // 接口方法签名检查
    for (auto iface : interfaces) {
        for (const auto& ifaceMethod : iface->methods) {
            auto it = declaredMethods.find(ifaceMethod.first);
            if (it == declaredMethods.end()) {
                throw std::runtime_error("Class '" + name + "' does not implement interface method '" + ifaceMethod.first + "'");
            }
            auto method = it->second;
            if (method->params.size() != ifaceMethod.second->params.size()) {
                throw std::runtime_error("Method '" + ifaceMethod.first + "' parameter count mismatch");
            }
            if (method->returnType != ifaceMethod.second->returnType) {
                throw std::runtime_error("Method '" + ifaceMethod.first + "' return type mismatch");
            }
        }
    }
    
    for (auto& pair : declaredMethods) {
        if (pair.second->isStatic) {
            klass->staticMethods[pair.first] = pair.second;
        } else {
            klass->methods[pair.first] = pair.second;
        }
    }
    
    env->define(name, Value(klass), true, "", "public");
}

void InterfaceDeclStmt::execute(std::shared_ptr<Environment> env) {
    auto iface = std::make_shared<InterfaceObject>(name);
    
    for (const auto& parentName : parentInterfaceNames) {
        Value parentVal = env->get({parentName});
        if (!parentVal.isInterface()) throw std::runtime_error("Parent interface not found: " + parentName);
        iface->parentInterfaces.push_back(parentVal.asInterface());
    }
    
    for (auto& stmt : body) {
        if (auto method = dynamic_cast<FunctionDeclStmt*>(stmt.get())) {
            auto func = std::make_shared<UserFunction>();
            func->params = method->getParams();
            auto defaultExprs = method->moveDefaultValues();
            for (auto& defExpr : defaultExprs) {
                (void)defExpr;
                func->defaultValues.push_back(Value());
            }
            func->body.clear();
            func->closure = env;
            func->isStatic = false;
            func->isFinal = false;
            func->isAbstract = true;
            func->isVirtual = true;
            func->isOverride = false;
            func->access = "public";
            func->returnType = method->getReturnType();
            
            iface->methods[method->getName()] = func;
        } else {
            throw std::runtime_error("Interface can only contain method declarations");
        }
    }
    
    env->define(name, Value(iface));
}

void MethodDeclStmt::execute(std::shared_ptr<Environment> env) {
    throw std::runtime_error("Method declaration outside class");
}

void FieldDeclStmt::execute(std::shared_ptr<Environment> env) {
    throw std::runtime_error("Field declaration outside class");
}

// ==================== 解析器 ====================
class Parser {
    std::vector<Token> tokens;
    size_t current = 0;
    std::string systemLibPath;

    Token peek() { return tokens[current]; }
    Token previous() { return tokens[current - 1]; }
    bool isAtEnd() { return peek().type == TokenKind::TK_END_OF_FILE; }
    Token advance() { if (!isAtEnd()) current++; return previous(); }
    bool check(TokenKind kind) { return !isAtEnd() && peek().type == kind; }
    bool match(std::initializer_list<TokenKind> kinds) {
        for (auto k : kinds) {
            if (check(k)) { advance(); return true; }
        }
        return false;
    }
    Token consume(TokenKind kind, const std::string& message) {
        if (check(kind)) return advance();
        throw std::runtime_error(message + " at line " + std::to_string(peek().line));
    }
    void synchronize() {
        advance();
        while (!isAtEnd()) {
            if (previous().type == TokenKind::TK_SEMICOLON) return;
            switch (peek().type) {
                case TokenKind::TK_FUNC: case TokenKind::TK_LET: case TokenKind::TK_VAR:
                case TokenKind::TK_IF: case TokenKind::TK_WHILE: case TokenKind::TK_FOR:
                case TokenKind::TK_RETURN: case TokenKind::TK_BREAK: case TokenKind::TK_CONTINUE:
                case TokenKind::TK_SWITCH: case TokenKind::TK_DO: case TokenKind::TK_TRY:
                case TokenKind::TK_THROW: case TokenKind::TK_NAMESPACE: case TokenKind::TK_IMPORT:
                case TokenKind::TK_EXPORT: case TokenKind::TK_USING:
                case TokenKind::TK_CLASS: case TokenKind::TK_INTERFACE:
                case TokenKind::TK_ENUM: case TokenKind::TK_STRUCT: case TokenKind::TK_EXTENSION:
                case TokenKind::TK_INT: case TokenKind::TK_FLOAT: case TokenKind::TK_BOOL:
                case TokenKind::TK_STRING: case TokenKind::TK_CHAR: case TokenKind::TK_WCHAR:
                case TokenKind::TK_ANY: case TokenKind::TK_VOID: case TokenKind::TK_CONST:
                    return;
                default: advance();
            }
        }
    }

    std::vector<std::string> parseQualifiedName() {
        std::vector<std::string> parts;
        parts.push_back(consume(TokenKind::TK_IDENTIFIER, "Expected identifier").lexeme);
        while (match({TokenKind::TK_SCOPE})) {
            parts.push_back(consume(TokenKind::TK_IDENTIFIER, "Expected identifier after '::'").lexeme);
        }
        return parts;
    }

    std::string parseTypeAnnotation() {
        std::string typeName;
        if (match({TokenKind::TK_COLON})) {
            Token typeToken = consume(TokenKind::TK_IDENTIFIER, "Expected type name after ':'");
            typeName = typeToken.lexeme;
        }
        return typeName;
    }

    std::unique_ptr<Expr> expression() { return assignment(); }

    std::unique_ptr<Expr> assignment() {
        auto expr = range();
        if (match({TokenKind::TK_EQUAL, TokenKind::TK_PLUS_EQUAL, TokenKind::TK_MINUS_EQUAL,
                   TokenKind::TK_STAR_EQUAL, TokenKind::TK_SLASH_EQUAL, TokenKind::TK_PERCENT_EQUAL,
                   TokenKind::TK_AMP_EQUAL, TokenKind::TK_PIPE_EQUAL, TokenKind::TK_CARET_EQUAL,
                   TokenKind::TK_LEFT_SHIFT_EQUAL, TokenKind::TK_RIGHT_SHIFT_EQUAL,
                   TokenKind::TK_UNSIGNED_RIGHT_SHIFT_EQUAL})) {
            TokenKind op = previous().type;
            auto value = assignment();
            if (dynamic_cast<VariableExpr*>(expr.get()) || dynamic_cast<IndexExpr*>(expr.get()) || dynamic_cast<MemberExpr*>(expr.get())) {
                return std::make_unique<AssignExpr>(std::move(expr), std::move(value), op, previous().line);
            }
            throw std::runtime_error("Invalid assignment target");
        }
        return expr;
    }

    std::unique_ptr<Expr> range() {
        auto expr = logicalOr();
        while (true) {
            if (match({TokenKind::TK_RANGE_CLOSED})) {
                auto right = logicalOr();
                expr = std::make_unique<RangeExpr>(std::move(expr), std::move(right), true, previous().line);
            } else if (match({TokenKind::TK_RANGE_HALF_OPEN})) {
                auto right = logicalOr();
                expr = std::make_unique<RangeExpr>(std::move(expr), std::move(right), false, previous().line);
            } else {
                break;
            }
        }
        return expr;
    }

    std::unique_ptr<Expr> logicalOr() {
        auto expr = logicalAnd();
        while (match({TokenKind::TK_OR})) {
            TokenKind op = previous().type;
            auto right = logicalAnd();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), previous().line);
        }
        return expr;
    }

    std::unique_ptr<Expr> logicalAnd() {
        auto expr = bitwiseOr();
        while (match({TokenKind::TK_AND})) {
            TokenKind op = previous().type;
            auto right = bitwiseOr();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), previous().line);
        }
        return expr;
    }

    std::unique_ptr<Expr> bitwiseOr() {
        auto expr = bitwiseXor();
        while (match({TokenKind::TK_BIT_OR})) {
            TokenKind op = previous().type;
            auto right = bitwiseXor();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), previous().line);
        }
        return expr;
    }

    std::unique_ptr<Expr> bitwiseXor() {
        auto expr = bitwiseAnd();
        while (match({TokenKind::TK_BIT_XOR})) {
            TokenKind op = previous().type;
            auto right = bitwiseAnd();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), previous().line);
        }
        return expr;
    }

    std::unique_ptr<Expr> bitwiseAnd() {
        auto expr = equality();
        while (match({TokenKind::TK_BIT_AND})) {
            TokenKind op = previous().type;
            auto right = equality();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), previous().line);
        }
        return expr;
    }

    std::unique_ptr<Expr> equality() {
        auto expr = comparison();
        while (match({TokenKind::TK_EQUAL_EQUAL, TokenKind::TK_NOT_EQUAL})) {
            TokenKind op = previous().type;
            auto right = comparison();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), previous().line);
        }
        return expr;
    }

    std::unique_ptr<Expr> comparison() {
        auto expr = shift();
        while (match({TokenKind::TK_LESS, TokenKind::TK_LESS_EQUAL, TokenKind::TK_GREATER, TokenKind::TK_GREATER_EQUAL,
                      TokenKind::TK_IS, TokenKind::TK_AS})) {
            TokenKind op = previous().type;
            if (op == TokenKind::TK_IS || op == TokenKind::TK_AS) {
                Token typeToken = consume(TokenKind::TK_IDENTIFIER, "Expected type name after is/as");
                if (op == TokenKind::TK_IS) {
                    expr = std::make_unique<IsExpr>(std::move(expr), typeToken.lexeme, previous().line);
                } else {
                    expr = std::make_unique<AsExpr>(std::move(expr), typeToken.lexeme, previous().line);
                }
            } else {
                auto right = shift();
                expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), previous().line);
            }
        }
        return expr;
    }

    std::unique_ptr<Expr> shift() {
        auto expr = addition();
        while (match({TokenKind::TK_LEFT_SHIFT, TokenKind::TK_RIGHT_SHIFT, TokenKind::TK_UNSIGNED_RIGHT_SHIFT})) {
            TokenKind op = previous().type;
            auto right = addition();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), previous().line);
        }
        return expr;
    }

    std::unique_ptr<Expr> addition() {
        auto expr = multiplication();
        while (match({TokenKind::TK_PLUS, TokenKind::TK_MINUS})) {
            TokenKind op = previous().type;
            auto right = multiplication();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), previous().line);
        }
        return expr;
    }

    std::unique_ptr<Expr> multiplication() {
        auto expr = unary();
        while (match({TokenKind::TK_STAR, TokenKind::TK_SLASH, TokenKind::TK_PERCENT})) {
            TokenKind op = previous().type;
            auto right = unary();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), previous().line);
        }
        return expr;
    }

    std::unique_ptr<Expr> unary() {
        if (match({TokenKind::TK_NOT, TokenKind::TK_MINUS, TokenKind::TK_BIT_NOT, TokenKind::TK_INC, TokenKind::TK_DEC,
                   TokenKind::TK_TYPEOF, TokenKind::TK_SIZEOF, TokenKind::TK_ASYNC, TokenKind::TK_AWAIT})) {
            TokenKind op = previous().type;
            auto expr = unary();
            if (op == TokenKind::TK_TYPEOF) {
                return std::make_unique<TypeofExpr>(std::move(expr), previous().line);
            } else if (op == TokenKind::TK_SIZEOF) {
                return std::make_unique<SizeofExpr>(std::move(expr), previous().line);
            } else if (op == TokenKind::TK_ASYNC) {
                return std::make_unique<AsyncExpr>(std::move(expr), previous().line);
            } else if (op == TokenKind::TK_AWAIT) {
                return std::make_unique<AwaitExpr>(std::move(expr), previous().line);
            }
            return std::make_unique<UnaryExpr>(op, std::move(expr), previous().line);
        }
        if (match({TokenKind::TK_BIT_AND})) {
            auto expr = unary();
            return std::make_unique<AddressOfExpr>(std::move(expr), previous().line);
        }
        if (match({TokenKind::TK_STAR})) {
            auto expr = unary();
            return std::make_unique<DerefExpr>(std::move(expr), previous().line);
        }
        return postfix();
    }

    std::unique_ptr<Expr> postfix() {
        auto expr = primary();
        while (true) {
            if (match({TokenKind::TK_INC, TokenKind::TK_DEC})) {
                TokenKind op = previous().type;
                expr = std::make_unique<PostfixExpr>(std::move(expr), op, previous().line);
            }
            else if (match({TokenKind::TK_LPAREN})) {
                std::vector<std::unique_ptr<Expr>> args;
                if (!check(TokenKind::TK_RPAREN)) {
                    do {
                        args.push_back(expression());
                    } while (match({TokenKind::TK_COMMA}));
                }
                consume(TokenKind::TK_RPAREN, "Expected ')' after arguments");
                expr = std::make_unique<CallExpr>(std::move(expr), std::move(args), previous().line);
            }
            else if (match({TokenKind::TK_LBRACKET})) {
                auto index = expression();
                consume(TokenKind::TK_RBRACKET, "Expected ']' after index");
                expr = std::make_unique<IndexExpr>(std::move(expr), std::move(index), previous().line);
            }
            else if (match({TokenKind::TK_QUESTION})) {
                auto thenExpr = expression();
                consume(TokenKind::TK_COLON, "Expected ':' after then expression in ternary");
                auto elseExpr = assignment();
                expr = std::make_unique<TernaryExpr>(std::move(expr), std::move(thenExpr), std::move(elseExpr), previous().line);
            }
            else if (match({TokenKind::TK_DOT}) || match({TokenKind::TK_SCOPE})) {
                break;
            }
            else {
                break;
            }
        }
        expr = memberAccess(std::move(expr));
        return expr;
    }

    std::unique_ptr<Expr> memberAccess(std::unique_ptr<Expr> expr) {
        while (true) {
            if (match({TokenKind::TK_DOT})) {
                Token name = consume(TokenKind::TK_IDENTIFIER, "Expected member name after '.'");
                expr = std::make_unique<MemberExpr>(std::move(expr), name.lexeme, false, name.line);
            } else if (match({TokenKind::TK_SCOPE})) {
                Token name = consume(TokenKind::TK_IDENTIFIER, "Expected member name after '::'");
                expr = std::make_unique<MemberExpr>(std::move(expr), name.lexeme, true, name.line);
            } else {
                break;
            }
        }
        return expr;
    }

    std::unique_ptr<Expr> lambda() {
        consume(TokenKind::TK_LPAREN, "Expected '(' before lambda parameters");
        std::vector<std::string> params;
        if (!check(TokenKind::TK_RPAREN)) {
            do {
                Token param = consume(TokenKind::TK_IDENTIFIER, "Expected parameter name");
                params.push_back(param.lexeme);
            } while (match({TokenKind::TK_COMMA}));
        }
        consume(TokenKind::TK_RPAREN, "Expected ')' after lambda parameters");
        consume(TokenKind::TK_ARROW, "Expected '->' after lambda parameters");
        auto body = expression();
        return std::make_unique<LambdaExpr>(std::move(params), std::move(body), previous().line);
    }

    std::unique_ptr<Expr> primary() {
        if (match({TokenKind::TK_TRUE})) return std::make_unique<LiteralExpr>(Value(true), previous().line);
        if (match({TokenKind::TK_FALSE})) return std::make_unique<LiteralExpr>(Value(false), previous().line);
        if (match({TokenKind::TK_NULL_LIT})) return std::make_unique<LiteralExpr>(Value(), previous().line);
        if (match({TokenKind::TK_NUMBER})) {
            const std::string& lex = previous().lexeme;
            bool isInt = lex.find('.') == std::string::npos && lex.find('e') == std::string::npos && lex.find('E') == std::string::npos;
            if (isInt) {
                int64_t val = std::stoll(lex);
                return std::make_unique<LiteralExpr>(Value(val), previous().line);
            } else {
                double val = std::stod(lex);
                return std::make_unique<LiteralExpr>(Value(val), previous().line);
            }
        }
        if (match({TokenKind::TK_STRING_LITERAL})) {
            std::string val = previous().lexeme.substr(1, previous().lexeme.length() - 2);
            // 处理转义字符
            std::string unescaped;
            for (size_t i = 0; i < val.size(); ++i) {
                if (val[i] == '\\' && i + 1 < val.size()) {
                    char esc = val[++i];
                    switch (esc) {
                        case 'n': unescaped += '\n'; break;
                        case 't': unescaped += '\t'; break;
                        case '\\': unescaped += '\\'; break;
                        case '"': unescaped += '"'; break;
                        case '\'': unescaped += '\''; break;
                        default: unescaped += esc; break;
                    }
                } else {
                    unescaped += val[i];
                }
            }
            return std::make_unique<LiteralExpr>(Value(unescaped), previous().line);
        }
        if (match({TokenKind::TK_CHAR_LITERAL})) {
            std::string lex = previous().lexeme;
            char c = lex[1];
            if (c == '\\') {
                char esc = lex[2];
                switch (esc) {
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case '\\': c = '\\'; break;
                    case '\'': c = '\''; break;
                    default: throw std::runtime_error("Unknown escape sequence");
                }
            }
            return std::make_unique<LiteralExpr>(Value(c), previous().line);
        }
        if (match({TokenKind::TK_WCHAR_LITERAL})) {
            std::string lex = previous().lexeme;
            char32_t wc = 0;
            if (lex[2] == '\\') {
                char esc = lex[3];
                switch (esc) {
                    case 'n': wc = '\n'; break;
                    case 't': wc = '\t'; break;
                    case '\\': wc = '\\'; break;
                    case '\'': wc = '\''; break;
                    default: throw std::runtime_error("Unknown escape sequence");
                }
            } else {
                wc = static_cast<char32_t>(lex[2]);
            }
            return std::make_unique<LiteralExpr>(Value(wc), previous().line);
        }
        if (match({TokenKind::TK_IDENTIFIER})) {
            if (previous().lexeme == "this") {
                return std::make_unique<ThisExpr>(previous().line);
            }
            if (previous().lexeme == "super") {
                return std::make_unique<SuperExpr>(previous().line);
            }
            auto names = parseQualifiedName();
            return std::make_unique<VariableExpr>(names, previous().line);
        }
        if (match({TokenKind::TK_LPAREN})) {
            if (check(TokenKind::TK_IDENTIFIER) || check(TokenKind::TK_RPAREN)) {
                size_t saved = current;
                try {
                    return lambda();
                } catch (...) {
                    current = saved;
                }
            }
            auto expr = expression();
            consume(TokenKind::TK_RPAREN, "Expected ')' after expression");
            return std::make_unique<GroupingExpr>(std::move(expr), previous().line);
        }
        if (match({TokenKind::TK_LBRACKET})) {
            std::vector<std::unique_ptr<Expr>> elements;
            if (!check(TokenKind::TK_RBRACKET)) {
                do {
                    elements.push_back(expression());
                } while (match({TokenKind::TK_COMMA}));
            }
            consume(TokenKind::TK_RBRACKET, "Expected ']' after array elements");
            return std::make_unique<ArrayLiteralExpr>(std::move(elements), previous().line);
        }
        if (match({TokenKind::TK_NEW})) {
            Token className = consume(TokenKind::TK_IDENTIFIER, "Expected class name after new");
            consume(TokenKind::TK_LPAREN, "Expected '(' after class name");
            std::vector<std::unique_ptr<Expr>> args;
            if (!check(TokenKind::TK_RPAREN)) {
                do {
                    args.push_back(expression());
                } while (match({TokenKind::TK_COMMA}));
            }
            consume(TokenKind::TK_RPAREN, "Expected ')' after arguments");
            return std::make_unique<NewExpr>(className.lexeme, std::move(args), className.line);
        }
        throw std::runtime_error("Unexpected token: " + peek().lexeme + " at line " + std::to_string(peek().line));
    }

    std::unique_ptr<Stmt> methodDeclStmt() {
        std::vector<std::string> modifiers;
        while (match({TokenKind::TK_PUBLIC, TokenKind::TK_PRIVATE, TokenKind::TK_PROTECTED,
                      TokenKind::TK_STATIC, TokenKind::TK_ABSTRACT, TokenKind::TK_FINAL,
                      TokenKind::TK_VIRTUAL, TokenKind::TK_OVERRIDE})) {
            modifiers.push_back(previous().lexeme);
        }
        Token name = consume(TokenKind::TK_IDENTIFIER, "Expected method name");
        consume(TokenKind::TK_LPAREN, "Expected '(' after method name");
        std::vector<std::string> params;
        std::vector<std::unique_ptr<Expr>> defaultValues;
        if (!check(TokenKind::TK_RPAREN)) {
            do {
                Token param = consume(TokenKind::TK_IDENTIFIER, "Expected parameter name");
                params.push_back(param.lexeme);
                if (match({TokenKind::TK_EQUAL})) {
                    defaultValues.push_back(expression());
                } else {
                    defaultValues.push_back(nullptr);
                }
            } while (match({TokenKind::TK_COMMA}));
        }
        consume(TokenKind::TK_RPAREN, "Expected ')' after parameters");
        std::string returnType = "any";
        if (match({TokenKind::TK_COLON})) {
            Token type = consume(TokenKind::TK_IDENTIFIER, "Expected return type");
            returnType = type.lexeme;
        }
        consume(TokenKind::TK_LBRACE, "Expected '{' before method body");
        std::vector<std::unique_ptr<Stmt>> body;
        while (!check(TokenKind::TK_RBRACE) && !isAtEnd()) {
            body.push_back(statement());
        }
        consume(TokenKind::TK_RBRACE, "Expected '}' after method body");
        return std::make_unique<FunctionDeclStmt>(name.lexeme, params, std::move(defaultValues), std::move(body), modifiers, returnType);
    }

    std::unique_ptr<Stmt> fieldDeclStmt() {
        std::vector<std::string> modifiers;
        while (match({TokenKind::TK_PUBLIC, TokenKind::TK_PRIVATE, TokenKind::TK_PROTECTED, TokenKind::TK_STATIC})) {
            modifiers.push_back(previous().lexeme);
        }
        Token name = consume(TokenKind::TK_IDENTIFIER, "Expected field name");
        std::string typeName = parseTypeAnnotation();
        std::unique_ptr<Expr> init = nullptr;
        if (match({TokenKind::TK_EQUAL})) {
            init = expression();
        }
        consume(TokenKind::TK_SEMICOLON, "Expected ';' after field declaration");
        return std::make_unique<FieldDeclStmt>(modifiers, name.lexeme, std::move(init), typeName, name.line);
    }

    // C 风格变量/数组声明： [const] 类型名 标识符 [ [数组大小] ] [ = 初始化表达式 ] ;
    std::unique_ptr<Stmt> cStyleVarDeclStmt() {
        bool isConst = false;
        if (match({TokenKind::TK_CONST})) {
            isConst = true;
        }

        // 消费类型关键字
        Token typeToken;
        if (check(TokenKind::TK_INT)) typeToken = advance();
        else if (check(TokenKind::TK_FLOAT)) typeToken = advance();
        else if (check(TokenKind::TK_BOOL)) typeToken = advance();
        else if (check(TokenKind::TK_STRING)) typeToken = advance();
        else if (check(TokenKind::TK_CHAR)) typeToken = advance();
        else if (check(TokenKind::TK_WCHAR)) typeToken = advance();
        else if (check(TokenKind::TK_ANY)) typeToken = advance();
        else if (check(TokenKind::TK_VOID)) typeToken = advance();
        else {
            throw std::runtime_error("Expected a type name at line " + std::to_string(peek().line));
        }

        Token name = consume(TokenKind::TK_IDENTIFIER, "Expected variable name");

        std::vector<std::unique_ptr<Expr>> dims;
        while (match({TokenKind::TK_LBRACKET})) {
            if (!check(TokenKind::TK_RBRACKET)) {
                dims.push_back(expression());
            } else {
                dims.push_back(nullptr); // 空维度，表示由初始化列表推断
            }
            consume(TokenKind::TK_RBRACKET, "Expected ']' after array dimension");
        }

        std::unique_ptr<Expr> init = nullptr;
        if (match({TokenKind::TK_EQUAL})) {
            init = expression();
        }

        if (isConst && !init && dims.empty()) {
            throw std::runtime_error("const variable must be initialized at line " + std::to_string(name.line));
        }

        consume(TokenKind::TK_SEMICOLON, "Expected ';' after declaration");

        return std::make_unique<VarDeclStmt>(
            name.lexeme,
            std::move(init),
            !isConst,
            false,
            typeToken.lexeme,
            "",
            std::move(dims),
            name.line
        );
    }

    std::unique_ptr<Stmt> statement() {
    // 模块和命名空间相关语句
    if (match({TokenKind::TK_IMPORT})) {
        bool isSystem = false;
        std::string path;
        if (check(TokenKind::TK_STRING_LITERAL)) {
            Token pathToken = advance();
            path = pathToken.lexeme.substr(1, pathToken.lexeme.length() - 2);
            isSystem = false;
        } else if (match({TokenKind::TK_LESS})) {
            isSystem = true;
            std::ostringstream oss;
            while (!check(TokenKind::TK_GREATER) && !isAtEnd()) {
                if (peek().type == TokenKind::TK_IDENTIFIER || peek().type == TokenKind::TK_SLASH || peek().type == TokenKind::TK_DOT) {
                    oss << advance().lexeme;
                } else {
                    oss << advance().lexeme;
                }
            }
            consume(TokenKind::TK_GREATER, "Expected '>' after system import path");
            path = oss.str();
        } else {
            throw std::runtime_error("Expected string literal or <path> after import");
        }
        consume(TokenKind::TK_SEMICOLON, "Expected ';' after import");
        return std::make_unique<ImportStmt>(path, isSystem, previous().line);
    }
    if (match({TokenKind::TK_EXPORT})) {
        auto decl = statement();
        return std::make_unique<ExportStmt>(std::move(decl), previous().line);
    }
    if (match({TokenKind::TK_NAMESPACE})) {
        Token name = consume(TokenKind::TK_IDENTIFIER, "Expected namespace name");
        consume(TokenKind::TK_LBRACE, "Expected '{' after namespace name");
        std::vector<std::unique_ptr<Stmt>> body;
        while (!check(TokenKind::TK_RBRACE) && !isAtEnd()) {
            body.push_back(statement());
        }
        consume(TokenKind::TK_RBRACE, "Expected '}' after namespace body");
        return std::make_unique<NamespaceStmt>(name.lexeme, std::move(body), name.line);
    }
    if (match({TokenKind::TK_USING})) {
        if (match({TokenKind::TK_NAMESPACE})) {
            auto names = parseQualifiedName();
            consume(TokenKind::TK_SEMICOLON, "Expected ';' after using namespace");
            return std::make_unique<UsingStmt>(names, true, previous().line);
        } else {
            auto names = parseQualifiedName();
            consume(TokenKind::TK_SEMICOLON, "Expected ';' after using");
            return std::make_unique<UsingStmt>(names, false, previous().line);
        }
    }
    if (match({TokenKind::TK_ENUM})) {
        Token name = consume(TokenKind::TK_IDENTIFIER, "Expected enum name");
        consume(TokenKind::TK_LBRACE, "Expected '{' after enum name");
        std::vector<std::pair<std::string, std::unique_ptr<Expr>>> enumerators;
        while (!check(TokenKind::TK_RBRACE) && !isAtEnd()) {
            Token id = consume(TokenKind::TK_IDENTIFIER, "Expected enumerator name");
            std::unique_ptr<Expr> value = nullptr;
            if (match({TokenKind::TK_EQUAL})) {
                value = expression();
            }
            enumerators.push_back({id.lexeme, std::move(value)});
            if (!check(TokenKind::TK_RBRACE)) {
                consume(TokenKind::TK_COMMA, "Expected ',' or '}' after enumerator");
            }
        }
        consume(TokenKind::TK_RBRACE, "Expected '}' after enum body");
        consume(TokenKind::TK_SEMICOLON, "Expected ';' after enum declaration");
        return std::make_unique<EnumDeclStmt>(name.lexeme, std::move(enumerators), name.line);
    }
    if (match({TokenKind::TK_STRUCT})) {
        std::vector<std::string> modifiers;
        while (match({TokenKind::TK_FINAL})) {
            modifiers.push_back(previous().lexeme);
        }
        Token name = consume(TokenKind::TK_IDENTIFIER, "Expected struct name");
        consume(TokenKind::TK_LBRACE, "Expected '{' before struct body");
        std::vector<std::unique_ptr<Stmt>> body;
        while (!check(TokenKind::TK_RBRACE) && !isAtEnd()) {
            if (match({TokenKind::TK_FUNC})) {
                body.push_back(methodDeclStmt());
            } else {
                body.push_back(fieldDeclStmt());
            }
        }
        consume(TokenKind::TK_RBRACE, "Expected '}' after struct body");
        return std::make_unique<StructDeclStmt>(name.lexeme, modifiers, std::move(body), name.line);
    }
    if (match({TokenKind::TK_EXTENSION})) {
        Token typeName = consume(TokenKind::TK_IDENTIFIER, "Expected type name to extend");
        consume(TokenKind::TK_LBRACE, "Expected '{' before extension body");
        std::vector<std::unique_ptr<Stmt>> methods;
        while (!check(TokenKind::TK_RBRACE) && !isAtEnd()) {
            if (match({TokenKind::TK_FUNC})) {
                methods.push_back(methodDeclStmt());
            } else {
                throw std::runtime_error("Extension can only contain methods");
            }
        }
        consume(TokenKind::TK_RBRACE, "Expected '}' after extension body");
        return std::make_unique<ExtensionDeclStmt>(typeName.lexeme, std::move(methods), typeName.line);
    }
    if (match({TokenKind::TK_INTERFACE})) {
        std::vector<std::string> modifiers;
        Token name = consume(TokenKind::TK_IDENTIFIER, "Expected interface name");
        std::vector<std::string> parentInterfaces;
        if (match({TokenKind::TK_EXTENDS})) {
            do {
                Token iface = consume(TokenKind::TK_IDENTIFIER, "Expected interface name");
                parentInterfaces.push_back(iface.lexeme);
            } while (match({TokenKind::TK_COMMA}));
        }
        consume(TokenKind::TK_LBRACE, "Expected '{' before interface body");
        std::vector<std::unique_ptr<Stmt>> body;
        while (!check(TokenKind::TK_RBRACE) && !isAtEnd()) {
            if (match({TokenKind::TK_FUNC})) {
                body.push_back(methodDeclStmt());
            } else {
                throw std::runtime_error("Interface can only contain method declarations");
            }
        }
        consume(TokenKind::TK_RBRACE, "Expected '}' after interface body");
        return std::make_unique<InterfaceDeclStmt>(name.lexeme, modifiers, parentInterfaces, std::move(body), name.line);
    }
    if (match({TokenKind::TK_CLASS})) {
        std::vector<std::string> modifiers;
        while (match({TokenKind::TK_ABSTRACT, TokenKind::TK_FINAL})) {
            modifiers.push_back(previous().lexeme);
        }
        Token name = consume(TokenKind::TK_IDENTIFIER, "Expected class name");
        std::string superName;
        if (match({TokenKind::TK_EXTENDS})) {
            Token super = consume(TokenKind::TK_IDENTIFIER, "Expected superclass name");
            superName = super.lexeme;
        }
        std::vector<std::string> interfaces;
        if (match({TokenKind::TK_IMPLEMENTS})) {
            do {
                Token iface = consume(TokenKind::TK_IDENTIFIER, "Expected interface name");
                interfaces.push_back(iface.lexeme);
            } while (match({TokenKind::TK_COMMA}));
        }
        consume(TokenKind::TK_LBRACE, "Expected '{' before class body");
        std::vector<std::unique_ptr<Stmt>> body;
        while (!check(TokenKind::TK_RBRACE) && !isAtEnd()) {
            if (match({TokenKind::TK_FUNC})) {
                body.push_back(methodDeclStmt());
            } else {
                body.push_back(fieldDeclStmt());
            }
        }
        consume(TokenKind::TK_RBRACE, "Expected '}' after class body");
        return std::make_unique<ClassDeclStmt>(name.lexeme, modifiers, superName, interfaces, std::move(body), name.line);
    }

    // Nova 风格变量声明 (let, var, auto, const) —— 优先于 C 风格
    if (match({TokenKind::TK_LET, TokenKind::TK_VAR, TokenKind::TK_AUTO, TokenKind::TK_ANY, TokenKind::TK_CONST}))
        return varDeclStmt();

    if (match({TokenKind::TK_FUNC})) return funcDeclStmt();
    if (match({TokenKind::TK_IF})) return ifStmt();
    if (match({TokenKind::TK_WHILE})) return whileStmt();
    if (match({TokenKind::TK_DO})) return doWhileStmt();
    if (match({TokenKind::TK_FOR})) {
        if (check(TokenKind::TK_LPAREN)) {
            return forStmt();
        } else {
            return forInStmt();
        }
    }
    if (match({TokenKind::TK_SWITCH})) return switchStmt();
    if (match({TokenKind::TK_TRY})) return tryCatchStmt();
    if (match({TokenKind::TK_THROW})) return throwStmt();
    if (match({TokenKind::TK_RETURN})) return returnStmt();
    if (match({TokenKind::TK_BREAK})) {
        std::string label;
        if (check(TokenKind::TK_IDENTIFIER)) {
            Token labelToken = advance();
            label = labelToken.lexeme;
        }
        consume(TokenKind::TK_SEMICOLON, "Expected ';' after break");
        return std::make_unique<BreakStmt>(label, previous().line);
    }
    if (match({TokenKind::TK_CONTINUE})) {
        std::string label;
        if (check(TokenKind::TK_IDENTIFIER)) {
            Token labelToken = advance();
            label = labelToken.lexeme;
        }
        consume(TokenKind::TK_SEMICOLON, "Expected ';' after continue");
        return std::make_unique<ContinueStmt>(label, previous().line);
    }
    if (match({TokenKind::TK_LBRACE})) return blockStmt();

    // C 风格变量声明（最后尝试）
    if (check(TokenKind::TK_CONST) ||
        check(TokenKind::TK_INT) ||
        check(TokenKind::TK_FLOAT) ||
        check(TokenKind::TK_BOOL) ||
        check(TokenKind::TK_STRING) ||
        check(TokenKind::TK_CHAR) ||
        check(TokenKind::TK_WCHAR) ||
        check(TokenKind::TK_ANY) ||
        check(TokenKind::TK_VOID)) {
        return cStyleVarDeclStmt();
    }

    return expressionStmt();
}

    std::unique_ptr<Stmt> varDeclStmt() {
        TokenKind declType = previous().type;
        bool isMutable = (declType == TokenKind::TK_VAR || declType == TokenKind::TK_AUTO || declType == TokenKind::TK_ANY);
        bool isDynamic = (declType == TokenKind::TK_AUTO);
        if (declType == TokenKind::TK_CONST) {
            isMutable = false;
            isDynamic = false;
        }
        Token name = consume(TokenKind::TK_IDENTIFIER, "Expected variable name");
        std::string typeName = parseTypeAnnotation();
        std::unique_ptr<Expr> init = nullptr;
        if (match({TokenKind::TK_EQUAL})) {
            init = expression();
        } else if (!isMutable && (declType == TokenKind::TK_LET || declType == TokenKind::TK_CONST)) {
            throw std::runtime_error("Let/const declaration must have initializer");
        }
        consume(TokenKind::TK_SEMICOLON, "Expected ';' after variable declaration");
        return std::make_unique<VarDeclStmt>(name.lexeme, std::move(init), isMutable, isDynamic, typeName, "", std::vector<std::unique_ptr<Expr>>(), name.line);
    }

    std::unique_ptr<Stmt> funcDeclStmt() {
        Token name = consume(TokenKind::TK_IDENTIFIER, "Expected function name");
        consume(TokenKind::TK_LPAREN, "Expected '(' after function name");
        std::vector<std::string> params;
        std::vector<std::unique_ptr<Expr>> defaultValues;
        if (!check(TokenKind::TK_RPAREN)) {
            do {
                Token param = consume(TokenKind::TK_IDENTIFIER, "Expected parameter name");
                params.push_back(param.lexeme);
                parseTypeAnnotation();
                if (match({TokenKind::TK_EQUAL})) {
                    defaultValues.push_back(expression());
                } else {
                    defaultValues.push_back(nullptr);
                }
            } while (match({TokenKind::TK_COMMA}));
        }
        consume(TokenKind::TK_RPAREN, "Expected ')' after parameters");
        std::string returnType = "any";
        if (match({TokenKind::TK_COLON})) {
            Token type = consume(TokenKind::TK_IDENTIFIER, "Expected return type");
            returnType = type.lexeme;
        }
        consume(TokenKind::TK_LBRACE, "Expected '{' before function body");
        std::vector<std::unique_ptr<Stmt>> body;
        while (!check(TokenKind::TK_RBRACE) && !isAtEnd()) {
            body.push_back(statement());
        }
        consume(TokenKind::TK_RBRACE, "Expected '}' after function body");
        return std::make_unique<FunctionDeclStmt>(name.lexeme, params, std::move(defaultValues), std::move(body), std::vector<std::string>(), returnType);
    }

    std::unique_ptr<Stmt> ifStmt() {
        consume(TokenKind::TK_LPAREN, "Expected '(' after 'if'");
        auto cond = expression();
        consume(TokenKind::TK_RPAREN, "Expected ')' after condition");
        auto thenBranch = statement();
        std::unique_ptr<Stmt> elseBranch = nullptr;
        if (match({TokenKind::TK_ELSE})) {
            elseBranch = statement();
        }
        return std::make_unique<IfStmt>(std::move(cond), std::move(thenBranch), std::move(elseBranch));
    }

    std::unique_ptr<Stmt> whileStmt() {
        consume(TokenKind::TK_LPAREN, "Expected '(' after 'while'");
        auto cond = expression();
        consume(TokenKind::TK_RPAREN, "Expected ')' after condition");
        auto body = statement();
        return std::make_unique<WhileStmt>(std::move(cond), std::move(body));
    }

    std::unique_ptr<Stmt> doWhileStmt() {
        int line = previous().line;
        auto body = statement();
        consume(TokenKind::TK_WHILE, "Expected 'while' after do body");
        consume(TokenKind::TK_LPAREN, "Expected '(' after 'while'");
        auto cond = expression();
        consume(TokenKind::TK_RPAREN, "Expected ')' after condition");
        consume(TokenKind::TK_SEMICOLON, "Expected ';' after do-while");
        return std::make_unique<DoWhileStmt>(std::move(body), std::move(cond), line);
    }

    std::unique_ptr<Stmt> forStmt() {
        consume(TokenKind::TK_LPAREN, "Expected '(' after 'for'");
        std::unique_ptr<Stmt> init;
        if (match({TokenKind::TK_LET, TokenKind::TK_VAR, TokenKind::TK_AUTO, TokenKind::TK_ANY, TokenKind::TK_CONST})) {
            init = varDeclStmt();
        } else if (!check(TokenKind::TK_SEMICOLON)) {
            auto expr = expression();
            consume(TokenKind::TK_SEMICOLON, "Expected ';' after for init expression");
            init = std::make_unique<ExpressionStmt>(std::move(expr));
        } else {
            consume(TokenKind::TK_SEMICOLON, "Expected ';' after for init");
            init = nullptr;
        }
        std::unique_ptr<Expr> cond;
        if (!check(TokenKind::TK_SEMICOLON)) {
            cond = expression();
        }
        consume(TokenKind::TK_SEMICOLON, "Expected ';' after for condition");
        std::unique_ptr<Expr> inc;
        if (!check(TokenKind::TK_RPAREN)) {
            inc = expression();
        }
        consume(TokenKind::TK_RPAREN, "Expected ')' after for clauses");
        auto body = statement();
        return std::make_unique<ForStmt>(std::move(init), std::move(cond), std::move(inc), std::move(body));
    }

    std::unique_ptr<Stmt> forInStmt() {
        consume(TokenKind::TK_FOR, "Expected 'for'");
        consume(TokenKind::TK_LPAREN, "Expected '(' after 'for'");
        bool isVar = match({TokenKind::TK_LET, TokenKind::TK_VAR, TokenKind::TK_AUTO});
        if (!isVar) {
            throw std::runtime_error("For-in loop requires 'let' or 'var' before variable name");
        }
        Token varToken = consume(TokenKind::TK_IDENTIFIER, "Expected variable name in for-in");
        std::string varName = varToken.lexeme;
        consume(TokenKind::TK_IN, "Expected 'in' in for-in loop");
        auto iterable = expression();
        consume(TokenKind::TK_RPAREN, "Expected ')' after iterable in for-in");
        auto body = statement();
        return std::make_unique<ForInStmt>(varName, std::move(iterable), std::move(body), varToken.line);
    }

    std::unique_ptr<Stmt> switchStmt() {
        int line = previous().line;
        consume(TokenKind::TK_LPAREN, "Expected '(' after 'switch'");
        auto value = expression();
        consume(TokenKind::TK_RPAREN, "Expected ')' after switch value");
        consume(TokenKind::TK_LBRACE, "Expected '{' before switch cases");

        std::vector<std::pair<std::unique_ptr<Expr>, std::vector<std::unique_ptr<Stmt>>>> cases;
        std::vector<std::unique_ptr<Stmt>> defaultCase;

        while (!check(TokenKind::TK_RBRACE) && !isAtEnd()) {
            if (match({TokenKind::TK_CASE})) {
                auto caseExpr = expression();
                consume(TokenKind::TK_COLON, "Expected ':' after case expression");
                std::vector<std::unique_ptr<Stmt>> caseStmts;
                while (!check(TokenKind::TK_CASE) && !check(TokenKind::TK_DEFAULT) && !check(TokenKind::TK_RBRACE) && !isAtEnd()) {
                    caseStmts.push_back(statement());
                }
                cases.push_back(std::make_pair(std::move(caseExpr), std::move(caseStmts)));
            } else if (match({TokenKind::TK_DEFAULT})) {
                consume(TokenKind::TK_COLON, "Expected ':' after default");
                while (!check(TokenKind::TK_CASE) && !check(TokenKind::TK_DEFAULT) && !check(TokenKind::TK_RBRACE) && !isAtEnd()) {
                    defaultCase.push_back(statement());
                }
            } else {
                throw std::runtime_error("Expected case or default in switch");
            }
        }
        consume(TokenKind::TK_RBRACE, "Expected '}' after switch");
        return std::make_unique<SwitchStmt>(std::move(value), std::move(cases), std::move(defaultCase), line);
    }

    std::unique_ptr<Stmt> tryCatchStmt() {
        int line = previous().line;
        consume(TokenKind::TK_LBRACE, "Expected '{' after try");
        std::vector<std::unique_ptr<Stmt>> tryBlock;
        while (!check(TokenKind::TK_CATCH) && !check(TokenKind::TK_RBRACE) && !isAtEnd()) {
            tryBlock.push_back(statement());
        }
        std::vector<CatchClause> catches;
        while (check(TokenKind::TK_CATCH)) {
            advance();
            consume(TokenKind::TK_LPAREN, "Expected '(' after catch");
            Token varToken = consume(TokenKind::TK_IDENTIFIER, "Expected exception variable name in catch");
            std::string typeName;
            if (match({TokenKind::TK_COLON})) {
                Token type = consume(TokenKind::TK_IDENTIFIER, "Expected type name after ':'");
                typeName = type.lexeme;
            }
            consume(TokenKind::TK_RPAREN, "Expected ')' after catch variable");
            consume(TokenKind::TK_LBRACE, "Expected '{' after catch");
            std::vector<std::unique_ptr<Stmt>> catchBlock;
            while (!check(TokenKind::TK_RBRACE) && !isAtEnd()) {
                catchBlock.push_back(statement());
            }
            consume(TokenKind::TK_RBRACE, "Expected '}' after catch block");
            catches.push_back({varToken.lexeme, typeName, std::move(catchBlock)});
        }
        return std::make_unique<TryCatchStmt>(std::move(tryBlock), std::move(catches), line);
    }

    std::unique_ptr<Stmt> throwStmt() {
        int line = previous().line;
        auto expr = expression();
        consume(TokenKind::TK_SEMICOLON, "Expected ';' after throw expression");
        return std::make_unique<ThrowStmt>(std::move(expr), line);
    }

    std::unique_ptr<Stmt> returnStmt() {
        int line = previous().line;
        std::unique_ptr<Expr> value = nullptr;
        if (!check(TokenKind::TK_SEMICOLON)) {
            value = expression();
        }
        consume(TokenKind::TK_SEMICOLON, "Expected ';' after return");
        return std::make_unique<ReturnStmt>(std::move(value), line);
    }

    std::unique_ptr<Stmt> blockStmt() {
        std::vector<std::unique_ptr<Stmt>> statements;
        while (!check(TokenKind::TK_RBRACE) && !isAtEnd()) {
            statements.push_back(statement());
        }
        consume(TokenKind::TK_RBRACE, "Expected '}' after block");
        return std::make_unique<BlockStmt>(std::move(statements));
    }

    std::unique_ptr<Stmt> expressionStmt() {
        auto expr = expression();
        consume(TokenKind::TK_SEMICOLON, "Expected ';' after expression");
        return std::make_unique<ExpressionStmt>(std::move(expr));
    }

public:
    Parser(std::vector<Token> t, const std::string& libPath) : tokens(std::move(t)), systemLibPath(libPath) {}

    std::vector<std::unique_ptr<Stmt>> parse() {
        std::vector<std::unique_ptr<Stmt>> statements;
        while (!isAtEnd()) {
            try {
                statements.push_back(statement());
            } catch (const std::runtime_error& e) {
                std::cerr << "Parse error: " << e.what() << std::endl;
                synchronize();
            }
        }
        return statements;
    }
};

// ==================== 解释器（增强：模块搜索路径） ====================
class Interpreter {
    std::shared_ptr<Environment> globalEnv;
    std::set<std::string> importedModules;
    std::vector<std::string> searchPaths;

    void initSearchPaths(const std::string& exeDir) {
        searchPaths.push_back("."); // 当前目录
        searchPaths.push_back(exeDir + "/stdlib");
        const char* envPath = getenv("NOVA_PATH");
        if (envPath) {
            std::string paths = envPath;
            size_t start = 0;
            size_t end;
            while ((end = paths.find_first_of(";:", start)) != std::string::npos) {
                searchPaths.push_back(paths.substr(start, end - start));
                start = end + 1;
            }
            if (start < paths.length())
                searchPaths.push_back(paths.substr(start));
        }
    }

    std::string findModule(const std::string& moduleName, bool isSystem) {
        if (isSystem) {
            for (const auto& base : searchPaths) {
                std::string full = base + "/" + moduleName;
                std::ifstream f(full);
                if (f.good()) return full;
                // 尝试添加扩展名
                std::vector<std::string> exts = {".nova", ".h", ".hpp", ""};
                for (const auto& ext : exts) {
                    std::string test = full + ext;
                    std::ifstream f2(test);
                    if (f2.good()) return test;
                }
            }
        } else {
            // 用户路径直接使用
            std::ifstream f(moduleName);
            if (f.good()) return moduleName;
            for (const auto& ext : {".nova", ".h", ".hpp", ""}) {
                std::string test = moduleName + ext;
                std::ifstream f2(test);
                if (f2.good()) return test;
            }
        }
        return "";
    }

    void importModule(const std::string& moduleName, bool isSystem, int line) {
        std::string fullPath = findModule(moduleName, isSystem);
        if (fullPath.empty()) {
            throw std::runtime_error("Cannot open module: " + moduleName + " at line " + std::to_string(line));
        }

        char resolved[PATH_MAX];
#ifdef _WIN32
        if (_fullpath(resolved, fullPath.c_str(), PATH_MAX)) {
#else
        if (realpath(fullPath.c_str(), resolved)) {
#endif
            fullPath = resolved;
        }
        if (importedModules.find(fullPath) != importedModules.end()) return;
        importedModules.insert(fullPath);

        std::ifstream file(fullPath);
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();

        Lexer lexer(source);
        auto tokens = lexer.scanTokens();
        Parser parser(tokens, searchPaths[1]); // 传递 stdlib 路径
        auto stmts = parser.parse();

        auto moduleEnv = globalEnv->newChild();
        moduleEnv->setCurrentModule(fullPath);
        
        for (auto& stmt : stmts) {
            stmt->execute(moduleEnv);
        }
        
        std::string moduleNamespace = moduleName;
        size_t lastSlash = moduleName.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            moduleNamespace = moduleName.substr(lastSlash + 1);
        }
        size_t dotPos = moduleNamespace.find('.');
        if (dotPos != std::string::npos) {
            moduleNamespace = moduleNamespace.substr(0, dotPos);
        }
        globalEnv->define(moduleNamespace, Value(moduleEnv), false, "", "public");
    }

    void registerNatives() {
        globalEnv->define("native_random", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (!args.empty()) throw std::runtime_error("native_random() expects no arguments");
                return Value(static_cast<double>(rand()) / RAND_MAX);
            }, 0, "native_random"
        )));

        globalEnv->define("println", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                for (size_t i = 0; i < args.size(); ++i) {
                    if (i > 0) std::cout << " ";
                    std::cout << valueToString(args[i]);
                }
                std::cout << std::endl;
                return Value();
            }, -1, "println"
        )));

        globalEnv->define("print", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                for (size_t i = 0; i < args.size(); ++i) {
                    if (i > 0) std::cout << " ";
                    std::cout << valueToString(args[i]);
                }
                return Value();
            }, -1, "print"
        )));

        globalEnv->define("len", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (args.size() != 1) throw std::runtime_error("len() expects exactly one argument");
                const Value& v = args[0];
                if (v.isArray()) return Value(static_cast<int64_t>(v.asArray().size()));
                if (v.isString()) return Value(static_cast<int64_t>(v.asString().length()));
                throw std::runtime_error("len() argument must be array or string");
            }, 1, "len"
        )));

        globalEnv->define("str", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (args.size() != 1) throw std::runtime_error("str() expects exactly one argument");
                return Value(valueToString(args[0]));
            }, 1, "str"
        )));

        globalEnv->define("int", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (args.size() != 1) throw std::runtime_error("int() expects exactly one argument");
                const Value& v = args[0];
                if (v.isInt()) return v;
                if (v.isFloat()) return Value(static_cast<int64_t>(v.asFloat()));
                if (v.isString()) {
                    try { return Value(static_cast<int64_t>(std::stoll(v.asString()))); }
                    catch (...) { return Value(static_cast<int64_t>(0)); }
                }
                if (v.isBool()) return Value(v.asBool() ? static_cast<int64_t>(1) : static_cast<int64_t>(0));
                return Value(static_cast<int64_t>(0));
            }, 1, "int"
        )));

        globalEnv->define("float", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (args.size() != 1) throw std::runtime_error("float() expects exactly one argument");
                const Value& v = args[0];
                if (v.isFloat()) return v;
                if (v.isInt()) return Value(static_cast<double>(v.asInt()));
                if (v.isString()) {
                    try { return Value(std::stod(v.asString())); }
                    catch (...) { return Value(0.0); }
                }
                if (v.isBool()) return Value(v.asBool() ? 1.0 : 0.0);
                return Value(0.0);
            }, 1, "float"
        )));

        globalEnv->define("type", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (args.size() != 1) throw std::runtime_error("type() expects exactly one argument");
                const Value& v = args[0];
                switch (v.type()) {
                    case ValueType::NULL_TYPE: return Value(std::string("null"));
                    case ValueType::INT: return Value(std::string("int"));
                    case ValueType::FLOAT: return Value(std::string("float"));
                    case ValueType::BOOL: return Value(std::string("bool"));
                    case ValueType::CHAR: return Value(std::string("char"));
                    case ValueType::WCHAR: return Value(std::string("wchar"));
                    case ValueType::STRING: return Value(std::string("string"));
                    case ValueType::FUNCTION: return Value(std::string("function"));
                    case ValueType::NATIVE: return Value(std::string("native"));
                    case ValueType::ARRAY: return Value(std::string("array"));
                    case ValueType::CLASS: return Value(std::string("class"));
                    case ValueType::INTERFACE: return Value(std::string("interface"));
                    case ValueType::INSTANCE: return Value(std::string("instance"));
                    case ValueType::FUTURE: return Value(std::string("future"));
                    case ValueType::NAMESPACE: return Value(std::string("namespace"));
                    case ValueType::POINTER: return Value(std::string("pointer"));
                    case ValueType::STRUCT: return Value(std::string("struct"));
                    default: return Value(std::string("unknown"));
                }
            }, 1, "type"
        )));

        globalEnv->define("readln", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (!args.empty()) throw std::runtime_error("readln() expects no arguments");
                std::string line;
                std::getline(std::cin, line);
                return Value(line);
            }, 0, "readln"
        )));

        // 新增：read() 读取整数
        globalEnv->define("read", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (!args.empty()) throw std::runtime_error("read() expects no arguments");
                int64_t val;
                std::cin >> val;
                if (std::cin.fail()) {
                    std::cin.clear();
                    std::string dummy;
                    std::cin >> dummy;
                    throw std::runtime_error("Invalid integer input");
                }
                return Value(val);
            }, 0, "read"
        )));

        // 新增：readFloat() 读取浮点数
        globalEnv->define("readFloat", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (!args.empty()) throw std::runtime_error("readFloat() expects no arguments");
                double val;
                std::cin >> val;
                if (std::cin.fail()) {
                    std::cin.clear();
                    std::string dummy;
                    std::cin >> dummy;
                    throw std::runtime_error("Invalid float input");
                }
                return Value(val);
            }, 0, "readFloat"
        )));

        globalEnv->define("range", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (args.size() == 1) {
                    if (!args[0].isInt()) throw std::runtime_error("range() argument must be integer");
                    int64_t end = args[0].asInt();
                    std::vector<Value> result;
                    for (int64_t i = 0; i < end; ++i) result.push_back(Value(i));
                    return Value(result);
                } else if (args.size() == 2) {
                    if (!args[0].isInt() || !args[1].isInt()) throw std::runtime_error("range() arguments must be integers");
                    int64_t start = args[0].asInt();
                    int64_t end = args[1].asInt();
                    std::vector<Value> result;
                    for (int64_t i = start; i < end; ++i) result.push_back(Value(i));
                    return Value(result);
                } else {
                    throw std::runtime_error("range() expects 1 or 2 arguments");
                }
            }, -1, "range"
        )));

        globalEnv->define("push", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment> env) -> Value {
                (void)env;
                if (args.size() != 2) throw std::runtime_error("push expects two arguments");
                if (!args[0].isArray()) throw std::runtime_error("First argument must be an array");
                return Value();
            }, 2, "push"
        )));

        globalEnv->define("pop", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment> env) -> Value {
                (void)env;
                if (args.size() != 1) throw std::runtime_error("pop expects one argument");
                if (!args[0].isArray()) throw std::runtime_error("Argument must be an array");
                return Value();
            }, 1, "pop"
        )));

        globalEnv->define("slice", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (args.size() != 3) throw std::runtime_error("slice() expects three arguments: array, start, end");
                const Value& arr = args[0];
                if (!arr.isArray()) throw std::runtime_error("slice() first argument must be an array");
                if (!args[1].isInt() || !args[2].isInt()) throw std::runtime_error("slice() start and end must be integers");
                int64_t start = args[1].asInt();
                int64_t end = args[2].asInt();
                const auto& orig = arr.asArray();
                if (start < 0) start = 0;
                if (end > static_cast<int64_t>(orig.size())) end = orig.size();
                if (start >= end) return Value(std::vector<Value>());
                std::vector<Value> newArr(orig.begin() + start, orig.begin() + end);
                return Value(newArr);
            }, 3, "slice"
        )));

        globalEnv->define("substr", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (args.size() != 3) throw std::runtime_error("substr() expects three arguments: string, start, end");
                const Value& s = args[0];
                if (!s.isString()) throw std::runtime_error("substr() first argument must be a string");
                if (!args[1].isInt() || !args[2].isInt()) throw std::runtime_error("substr() start and end must be integers");
                int64_t start = args[1].asInt();
                int64_t end = args[2].asInt();
                const std::string& str = s.asString();
                if (start < 0) start = 0;
                if (end > static_cast<int64_t>(str.length())) end = str.length();
                if (start >= end) return Value(std::string());
                return Value(str.substr(start, end - start));
            }, 3, "substr"
        )));

        globalEnv->define("toString", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (args.size() != 1) throw std::runtime_error("toString() expects exactly one argument");
                return Value(valueToString(args[0]));
            }, 1, "toString"
        )));

        globalEnv->define("sys_open", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (args.size() != 2) throw std::runtime_error("sys_open expects 2 arguments: path (string), mode (string)");
                if (!args[0].isString() || !args[1].isString())
                    throw std::runtime_error("sys_open arguments must be strings");

                std::string path = args[0].asString();
                std::string mode = args[1].asString();

#ifdef _WIN32
                DWORD dwDesiredAccess = 0;
                DWORD dwCreationDisposition = OPEN_EXISTING;
                if (mode == "r") {
                    dwDesiredAccess = GENERIC_READ;
                    dwCreationDisposition = OPEN_EXISTING;
                } else if (mode == "w") {
                    dwDesiredAccess = GENERIC_WRITE;
                    dwCreationDisposition = CREATE_ALWAYS;
                } else if (mode == "a") {
                    dwDesiredAccess = GENERIC_WRITE;
                    dwCreationDisposition = OPEN_ALWAYS;
                } else if (mode == "r+") {
                    dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
                    dwCreationDisposition = OPEN_EXISTING;
                } else if (mode == "w+") {
                    dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
                    dwCreationDisposition = CREATE_ALWAYS;
                } else if (mode == "a+") {
                    dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
                    dwCreationDisposition = OPEN_ALWAYS;
                } else {
                    throw std::runtime_error("Unsupported file mode: " + mode);
                }

                HANDLE hFile = CreateFileA(path.c_str(), dwDesiredAccess, FILE_SHARE_READ, NULL,
                                           dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile == INVALID_HANDLE_VALUE) {
                    return Value(static_cast<int64_t>(-1));
                }

                if (mode == "a" || mode == "a+") {
                    SetFilePointer(hFile, 0, NULL, FILE_END);
                }

                return Value(static_cast<int64_t>(reinterpret_cast<intptr_t>(hFile)));
#else
                int flags = 0;
                if (mode == "r") flags = O_RDONLY;
                else if (mode == "w") flags = O_WRONLY | O_CREAT | O_TRUNC;
                else if (mode == "a") flags = O_WRONLY | O_CREAT | O_APPEND;
                else if (mode == "r+") flags = O_RDWR;
                else if (mode == "w+") flags = O_RDWR | O_CREAT | O_TRUNC;
                else if (mode == "a+") flags = O_RDWR | O_CREAT | O_APPEND;
                else throw std::runtime_error("Unsupported file mode: " + mode);

                int fd = ::open(path.c_str(), flags, 0644);
                return Value(static_cast<int64_t>(fd));
#endif
            }, 2, "sys_open"
        )));

        globalEnv->define("sys_read", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (args.size() != 2) throw std::runtime_error("sys_read expects 2 arguments: handle (int), length (int)");
                if (!args[0].isInt() || !args[1].isInt())
                    throw std::runtime_error("sys_read arguments must be integers");

#ifdef _WIN32
                HANDLE hFile = reinterpret_cast<HANDLE>(static_cast<intptr_t>(args[0].asInt()));
                DWORD length = static_cast<DWORD>(args[1].asInt());
                std::vector<char> buffer(length);
                DWORD bytesRead = 0;
                if (!ReadFile(hFile, buffer.data(), length, &bytesRead, NULL)) {
                    return Value(std::string());
                }
                return Value(std::string(buffer.data(), bytesRead));
#else
                int fd = static_cast<int>(args[0].asInt());
                size_t length = static_cast<size_t>(args[1].asInt());
                std::vector<char> buffer(length);
                ssize_t n = ::read(fd, buffer.data(), length);
                if (n < 0) return Value(std::string());
                return Value(std::string(buffer.data(), n));
#endif
            }, 2, "sys_read"
        )));

        globalEnv->define("sys_write", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (args.size() != 2) throw std::runtime_error("sys_write expects 2 arguments: handle (int), data (string)");
                if (!args[0].isInt() || !args[1].isString())
                    throw std::runtime_error("sys_write arguments: handle must be int, data must be string");

#ifdef _WIN32
                HANDLE hFile = reinterpret_cast<HANDLE>(static_cast<intptr_t>(args[0].asInt()));
                const std::string& data = args[1].asString();
                DWORD bytesWritten = 0;
                if (!WriteFile(hFile, data.c_str(), static_cast<DWORD>(data.size()), &bytesWritten, NULL)) {
                    return Value(static_cast<int64_t>(-1));
                }
                return Value(static_cast<int64_t>(bytesWritten));
#else
                int fd = static_cast<int>(args[0].asInt());
                const std::string& data = args[1].asString();
                ssize_t n = ::write(fd, data.c_str(), data.size());
                if (n < 0) return Value(static_cast<int64_t>(-1));
                return Value(static_cast<int64_t>(n));
#endif
            }, 2, "sys_write"
        )));

        globalEnv->define("sys_close", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (args.size() != 1) throw std::runtime_error("sys_close expects 1 argument: handle (int)");
                if (!args[0].isInt()) throw std::runtime_error("sys_close argument must be int");

#ifdef _WIN32
                HANDLE hFile = reinterpret_cast<HANDLE>(static_cast<intptr_t>(args[0].asInt()));
                BOOL result = CloseHandle(hFile);
                return Value(result != FALSE);
#else
                int fd = static_cast<int>(args[0].asInt());
                int result = ::close(fd);
                return Value(result == 0);
#endif
            }, 1, "sys_close"
        )));

        globalEnv->define("sys_socket", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (args.size() != 3) throw std::runtime_error("sys_socket expects 3 arguments: domain (int), type (int), protocol (int)");
                if (!args[0].isInt() || !args[1].isInt() || !args[2].isInt())
                    throw std::runtime_error("sys_socket arguments must be integers");

                int domain = static_cast<int>(args[0].asInt());
                int type = static_cast<int>(args[1].asInt());
                int protocol = static_cast<int>(args[2].asInt());

#ifdef _WIN32
                SOCKET s = socket(domain, type, protocol);
                if (s == INVALID_SOCKET) return Value(static_cast<int64_t>(-1));
                return Value(static_cast<int64_t>(static_cast<intptr_t>(s)));
#else
                int fd = socket(domain, type, protocol);
                if (fd < 0) return Value(static_cast<int64_t>(-1));
                return Value(static_cast<int64_t>(fd));
#endif
            }, 3, "sys_socket"
        )));

        globalEnv->define("sys_connect", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (args.size() != 3) throw std::runtime_error("sys_connect expects 3 arguments: sock (int), addr (string), port (int)");
                if (!args[0].isInt() || !args[1].isString() || !args[2].isInt())
                    throw std::runtime_error("sys_connect arguments invalid");

#ifdef _WIN32
                SOCKET s = static_cast<SOCKET>(static_cast<intptr_t>(args[0].asInt()));
#else
                int s = static_cast<int>(args[0].asInt());
#endif
                std::string addr = args[1].asString();
                int port = static_cast<int>(args[2].asInt());

                struct sockaddr_in server_addr;
                server_addr.sin_family = AF_INET;
                server_addr.sin_port = htons(port);
                server_addr.sin_addr.s_addr = inet_addr(addr.c_str());
                if (server_addr.sin_addr.s_addr == INADDR_NONE) {
                    return Value(false);
                }

                int result = connect(s, (struct sockaddr*)&server_addr, sizeof(server_addr));
                return Value(result == 0);
            }, 3, "sys_connect"
        )));

        globalEnv->define("sys_send", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (args.size() != 2) throw std::runtime_error("sys_send expects 2 arguments: sock (int), data (string)");
                if (!args[0].isInt() || !args[1].isString())
                    throw std::runtime_error("sys_send arguments invalid");

#ifdef _WIN32
                SOCKET s = static_cast<SOCKET>(static_cast<intptr_t>(args[0].asInt()));
#else
                int s = static_cast<int>(args[0].asInt());
#endif
                const std::string& data = args[1].asString();

                int sent = send(s, data.c_str(), static_cast<int>(data.size()), 0);
                if (sent < 0) return Value(static_cast<int64_t>(-1));
                return Value(static_cast<int64_t>(sent));
            }, 2, "sys_send"
        )));

        globalEnv->define("sys_recv", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (args.size() != 2) throw std::runtime_error("sys_recv expects 2 arguments: sock (int), length (int)");
                if (!args[0].isInt() || !args[1].isInt())
                    throw std::runtime_error("sys_recv arguments invalid");

#ifdef _WIN32
                SOCKET s = static_cast<SOCKET>(static_cast<intptr_t>(args[0].asInt()));
#else
                int s = static_cast<int>(args[0].asInt());
#endif
                int length = static_cast<int>(args[1].asInt());

                std::vector<char> buffer(length);
                int received = recv(s, buffer.data(), length, 0);
                if (received < 0) return Value(std::string());
                return Value(std::string(buffer.data(), received));
            }, 2, "sys_recv"
        )));

        globalEnv->define("sys_closesocket", Value(std::make_shared<NativeFunction>(
            [](const std::vector<Value>& args, std::shared_ptr<Environment>) -> Value {
                if (args.size() != 1) throw std::runtime_error("sys_closesocket expects 1 argument: sock (int)");
                if (!args[0].isInt()) throw std::runtime_error("sys_closesocket argument must be int");

#ifdef _WIN32
                SOCKET s = static_cast<SOCKET>(static_cast<intptr_t>(args[0].asInt()));
                int result = closesocket(s);
                return Value(result == 0);
#else
                int s = static_cast<int>(args[0].asInt());
                int result = close(s);
                return Value(result == 0);
#endif
            }, 1, "sys_closesocket"
        )));
    }

public:
    Interpreter(const std::string& libPath) : globalEnv(std::make_shared<Environment>()) {
        initSearchPaths(libPath);
        srand(static_cast<unsigned>(time(nullptr)));

#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
            exit(1);
        }
#endif

        registerNatives();
    }

    ~Interpreter() {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void execute(const std::vector<std::unique_ptr<Stmt>>& statements) {
        for (auto& stmt : statements) {
            try {
                if (auto importStmt = dynamic_cast<ImportStmt*>(stmt.get())) {
                    importModule(importStmt->getPath(), importStmt->isSystemPath(), importStmt->getLine());
                } else {
                    stmt->execute(globalEnv);
                }
            } catch (const ReturnException& e) {
                throw std::runtime_error("Unexpected return outside function at line " + std::to_string(e.line));
            } catch (const BreakException& be) {
                throw std::runtime_error("Unexpected break" + (be.label.empty() ? "" : " with label '" + be.label + "'") + " outside loop");
            } catch (const ContinueException& ce) {
                throw std::runtime_error("Unexpected continue" + (ce.label.empty() ? "" : " with label '" + ce.label + "'") + " outside loop");
            } catch (const ThrowException& te) {
                throw std::runtime_error("Uncaught exception: " + valueToString(te.value) + " at line " + std::to_string(te.line));
            } catch (const std::runtime_error& e) {
                int line = stmt->getLine();
                if (line > 0)
                    throw std::runtime_error(std::string(e.what()) + " (in statement at line " + std::to_string(line) + ")");
                else
                    throw;
            } catch (const std::exception& e) {
                throw std::runtime_error(std::string("Native exception: ") + e.what());
            }
        }
    }
};
int main(int argc, char* argv[]) {
    // 解析参数
    bool sdebug = false;
    std::string filename;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--sdebug") {
            sdebug = true;
            g_debugMode = true;  
        } else if (filename.empty()) {
            filename = arg;
        }
    }
    
    if (filename.empty()) {
        std::cerr << "Usage: " << argv[0] << " [--sdebug] <filename>" << std::endl;
        return 1;
    }

    std::string exePath = argv[0];
    size_t pos = exePath.find_last_of("/\\");
    std::string exeDir = (pos != std::string::npos) ? exePath.substr(0, pos) : ".";

    // 以二进制模式打开文件
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Could not open file: " << filename << std::endl;
        return 1;
    }

    // 读取全部内容到字符串
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    file.close();

    // 如果启用 sdebug，打印十六进制内容
    if (sdebug) {
        std::cerr << "=== Source file hex dump (first 256 bytes) ===" << std::endl;
        for (size_t i = 0; i < source.size() && i < 256; ++i) {
            if (i % 16 == 0) {
                if (i != 0) std::cerr << std::endl;
                std::cerr << std::hex << std::setw(4) << std::setfill('0') << i << ": ";
            }
            std::cerr << std::hex << std::setw(2) << std::setfill('0')
                      << (static_cast<unsigned int>(static_cast<unsigned char>(source[i]))) << " ";
        }
        std::cerr << std::dec << std::endl;
        std::cerr << "Total size: " << source.size() << " bytes" << std::endl;
        if (source.size() >= 3 &&
            static_cast<unsigned char>(source[0]) == 0xEF &&
            static_cast<unsigned char>(source[1]) == 0xBB &&
            static_cast<unsigned char>(source[2]) == 0xBF) {
            std::cerr << "Info: UTF-8 BOM detected at beginning." << std::endl;
        } else {
            std::cerr << "Info: No UTF-8 BOM found." << std::endl;
        }
    }

    if (source.size() >= 3 &&
        static_cast<unsigned char>(source[0]) == 0xEF &&
        static_cast<unsigned char>(source[1]) == 0xBB &&
        static_cast<unsigned char>(source[2]) == 0xBF) {
        source = source.substr(3);
        if (sdebug) std::cerr << "Info: UTF-8 BOM removed before parsing." << std::endl;
    }

    try {
        Lexer lexer(source);
        auto tokens = lexer.scanTokens();

        Parser parser(tokens, exeDir);
        auto statements = parser.parse();

        Interpreter interpreter(exeDir);
        interpreter.execute(statements);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
