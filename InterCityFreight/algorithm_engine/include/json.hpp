// ============================================================================
//  json.hpp — Minimal, dependency-free JSON parser / serializer (C++17)
//  Written for the Inter-City Freight Logistics & Route Generator.
//  Supports: objects, arrays, strings, numbers, booleans, null.
//  This is intentionally small and self-contained so the C++ engine has no
//  external dependencies. It is NOT a general-purpose JSON library.
// ============================================================================
#pragma once
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <cmath>

namespace minijson {

class Json;
using JsonArray  = std::vector<Json>;
using JsonObject = std::map<std::string, Json>;

class JsonParseError : public std::runtime_error {
public:
    explicit JsonParseError(const std::string& m) : std::runtime_error(m) {}
};

class Json {
public:
    enum class Type { Null, Boolean, Number, String, Array, Object };

    Json() : type_(Type::Null) {}
    Json(std::nullptr_t) : type_(Type::Null) {}
    Json(bool b) : type_(Type::Boolean), bool_(b) {}
    Json(double d) : type_(Type::Number), num_(d) {}
    Json(int i) : type_(Type::Number), num_(static_cast<double>(i)) {}
    Json(long long i) : type_(Type::Number), num_(static_cast<double>(i)) {}
    Json(const char* s) : type_(Type::String), str_(s ? s : "") {}
    Json(const std::string& s) : type_(Type::String), str_(s) {}

    static Json array()  { Json j; j.type_ = Type::Array;  return j; }
    static Json object() { Json j; j.type_ = Type::Object; return j; }

    Type type() const { return type_; }
    bool is_null()   const { return type_ == Type::Null; }
    bool is_bool()   const { return type_ == Type::Boolean; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array()  const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

    bool        as_bool(bool def = false)                 const { return is_bool() ? bool_ : def; }
    double      as_number(double def = 0.0)               const { return is_number() ? num_ : def; }
    long long   as_int(long long def = 0)                 const { return is_number() ? (long long)num_ : def; }
    std::string as_string(const std::string& def = "")    const { return is_string() ? str_ : def; }

    size_t size() const {
        if (is_array())  return arr_.size();
        if (is_object()) return obj_.size();
        return 0;
    }
    bool empty() const { return size() == 0; }

    // Mutable element access (used while building output objects).
    Json& operator[](const std::string& k) { type_ = Type::Object; return obj_[k]; }
    void  push_back(const Json& v)         { type_ = Type::Array; arr_.push_back(v); }

    // Read access.
    bool has(const std::string& k) const { return is_object() && obj_.count(k) > 0; }
    const Json& at(size_t i) const { return arr_.at(i); }
    const Json& at(const std::string& k) const {
        static const Json null_value;
        if (!is_object()) return null_value;
        auto it = obj_.find(k);
        return it == obj_.end() ? null_value : it->second;
    }

    const JsonArray&  array_items()  const { return arr_; }
    const JsonObject& object_items() const { return obj_; }

    std::vector<std::string> keys() const {
        std::vector<std::string> ks;
        if (is_object()) for (const auto& kv : obj_) ks.push_back(kv.first);
        return ks;
    }

    // ---- Serialization ----
    std::string dump(int indent = 0) const {
        std::ostringstream o;
        dump_to(o, indent, 0);
        return o.str();
    }

    // ---- Parsing ----
    static Json parse(const std::string& text);   // defined below after Parser

private:
    static void escape_string(const std::string& s, std::ostringstream& o) {
        static const char* hex = "0123456789abcdef";
        o << '"';
        for (unsigned char c : s) {
            switch (c) {
                case '"':  o << "\\\""; break;
                case '\\': o << "\\\\"; break;
                case '\b': o << "\\b";  break;
                case '\f': o << "\\f";  break;
                case '\n': o << "\\n";  break;
                case '\r': o << "\\r";  break;
                case '\t': o << "\\t";  break;
                default:
                    if (c < 0x20) o << "\\u00" << hex[(c >> 4) & 0xF] << hex[c & 0xF];
                    else o << static_cast<char>(c);
            }
        }
        o << '"';
    }

    void dump_to(std::ostringstream& o, int indent, int depth) const {
        const bool pretty = indent > 0;
        const std::string pad    = pretty ? std::string(indent * (depth + 1), ' ') : "";
        const std::string padEnd = pretty ? std::string(indent * depth, ' ') : "";
        const std::string nl     = pretty ? "\n" : "";
        const std::string sep    = pretty ? " " : "";

        switch (type_) {
            case Type::Null:    o << "null"; break;
            case Type::Boolean: o << (bool_ ? "true" : "false"); break;
            case Type::Number: {
                if (std::isfinite(num_)) {
                    if (std::floor(num_) == num_ && std::fabs(num_) < 1e15) o << (long long)num_;
                    else { std::ostringstream t; t.precision(10); t << num_; o << t.str(); }
                } else o << "null";
                break;
            }
            case Type::String: escape_string(str_, o); break;
            case Type::Array: {
                if (arr_.empty()) { o << "[]"; break; }
                o << "[" << nl;
                for (size_t i = 0; i < arr_.size(); ++i) {
                    o << pad; arr_[i].dump_to(o, indent, depth + 1);
                    if (i + 1 < arr_.size()) o << ",";
                    o << nl;
                }
                o << padEnd << "]";
                break;
            }
            case Type::Object: {
                if (obj_.empty()) { o << "{}"; break; }
                o << "{" << nl;
                size_t i = 0;
                for (const auto& kv : obj_) {
                    o << pad; escape_string(kv.first, o); o << ":" << sep;
                    kv.second.dump_to(o, indent, depth + 1);
                    if (++i < obj_.size()) o << ",";
                    o << nl;
                }
                o << padEnd << "}";
                break;
            }
        }
    }

    Type        type_ = Type::Null;
    bool        bool_ = false;
    double      num_  = 0.0;
    std::string str_;
    JsonArray   arr_;
    JsonObject  obj_;
};

// ---------------------------------------------------------------------------
//  Recursive-descent parser
// ---------------------------------------------------------------------------
class Parser {
public:
    explicit Parser(const std::string& text) : text_(text), pos_(0) {}

    Json parse() {
        skip_ws();
        Json v = parse_value();
        skip_ws();
        if (pos_ != text_.size()) throw JsonParseError("trailing characters after JSON value");
        return v;
    }

private:
    const std::string& text_;
    size_t pos_;

    void skip_ws() { while (pos_ < text_.size() && std::isspace((unsigned char)text_[pos_])) ++pos_; }
    char peek() {
        if (pos_ >= text_.size()) throw JsonParseError("unexpected end of input");
        return text_[pos_];
    }
    char get() { char c = peek(); ++pos_; return c; }
    void expect(char c) {
        if (get() != c) throw JsonParseError(std::string("expected '") + c + "'");
    }

    Json parse_value() {
        skip_ws();
        char c = peek();
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') return Json(parse_string());
        if (c == 't' || c == 'f') return parse_bool();
        if (c == 'n') return parse_null();
        return parse_number();
    }

    Json parse_object() {
        Json o = Json::object();
        expect('{'); skip_ws();
        if (peek() == '}') { get(); return o; }
        while (true) {
            skip_ws();
            std::string key = parse_string();
            skip_ws(); expect(':');
            o[key] = parse_value();
            skip_ws();
            char c = get();
            if (c == ',') continue;
            if (c == '}') break;
            throw JsonParseError("expected ',' or '}' in object");
        }
        return o;
    }

    Json parse_array() {
        Json a = Json::array();
        expect('['); skip_ws();
        if (peek() == ']') { get(); return a; }
        while (true) {
            a.push_back(parse_value());
            skip_ws();
            char c = get();
            if (c == ',') continue;
            if (c == ']') break;
            throw JsonParseError("expected ',' or ']' in array");
        }
        return a;
    }

    std::string parse_string() {
        expect('"');
        std::string out;
        while (true) {
            char c = get();
            if (c == '"') break;
            if (c == '\\') {
                char e = get();
                switch (e) {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/';  break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    case 'n':  out += '\n'; break;
                    case 'r':  out += '\r'; break;
                    case 't':  out += '\t'; break;
                    case 'u': {
                        unsigned cp = 0;
                        for (int i = 0; i < 4; ++i) cp = cp * 16 + hexval(get());
                        if (cp < 0x80) out += (char)cp;
                        else if (cp < 0x800) {
                            out += (char)(0xC0 | (cp >> 6));
                            out += (char)(0x80 | (cp & 0x3F));
                        } else {
                            out += (char)(0xE0 | (cp >> 12));
                            out += (char)(0x80 | ((cp >> 6) & 0x3F));
                            out += (char)(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default: throw JsonParseError("invalid escape sequence");
                }
            } else {
                out += c;
            }
        }
        return out;
    }

    Json parse_bool() {
        if (text_.compare(pos_, 4, "true") == 0)  { pos_ += 4; return Json(true); }
        if (text_.compare(pos_, 5, "false") == 0) { pos_ += 5; return Json(false); }
        throw JsonParseError("invalid literal");
    }

    Json parse_null() {
        if (text_.compare(pos_, 4, "null") == 0) { pos_ += 4; return Json(nullptr); }
        throw JsonParseError("invalid literal");
    }

    Json parse_number() {
        size_t start = pos_;
        if (pos_ < text_.size() && (text_[pos_] == '-' || text_[pos_] == '+')) ++pos_;
        while (pos_ < text_.size() &&
               (std::isdigit((unsigned char)text_[pos_]) || text_[pos_] == '.' ||
                text_[pos_] == 'e' || text_[pos_] == 'E' || text_[pos_] == '+' || text_[pos_] == '-'))
            ++pos_;
        std::string numstr = text_.substr(start, pos_ - start);
        if (numstr.empty()) throw JsonParseError("invalid number");
        try { return Json(std::stod(numstr)); }
        catch (...) { throw JsonParseError("invalid number: " + numstr); }
    }

    int hexval(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw JsonParseError("invalid hex digit");
    }
};

inline Json Json::parse(const std::string& text) {
    Parser p(text);
    return p.parse();
}

} // namespace minijson