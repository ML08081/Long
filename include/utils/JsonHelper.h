#ifndef PATROL_UTILS_JSONHELPER_H
#define PATROL_UTILS_JSONHELPER_H

#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include <cctype>
#include <cstdlib>

// ==========================================================================
//  JsonHelper -- 轻量级 JSON 解析器（头文件，无第三方依赖，C++11）
//  支持 string / number / bool / null / object / array
// ==========================================================================

namespace patrol {
namespace json {

struct JsonNode {
    enum Type { Null, Bool, Int, Double, String, Object, Array } type = Null;

    bool        bool_val = false;
    long long   int_val  = 0;
    double      dbl_val  = 0.0;
    std::string str_val;
    std::map<std::string, JsonNode> obj_val;
    std::vector<JsonNode>           arr_val;

    bool is_null()   const { return type == Null; }
    bool is_object() const { return type == Object; }
    bool is_array()  const { return type == Array; }
    bool is_string() const { return type == String; }
    bool is_number() const { return type == Int || type == Double; }

    const std::string& string_or(const std::string& def) const {
        return (type == String) ? str_val : def;
    }
    long long int_or(long long def) const {
        if (type == Int)    return int_val;
        if (type == Double) return static_cast<long long>(dbl_val);
        return def;
    }
    const JsonNode& operator[](const std::string& key) const {
        static const JsonNode null_node;
        auto it = obj_val.find(key);
        return (it != obj_val.end()) ? it->second : null_node;
    }
    bool has(const std::string& key) const {
        return obj_val.find(key) != obj_val.end();
    }
};

namespace detail {
class Parser {
    const std::string& s_;
    size_t pos_;
public:
    explicit Parser(const std::string& s) : s_(s), pos_(0) {}
    JsonNode parse() { skipWs(); return parseValue(); }
private:
    void skipWs() {
        while (pos_ < s_.size() && std::isspace((unsigned char)s_[pos_])) ++pos_;
    }
    char peek() const { return pos_ < s_.size() ? s_[pos_] : '\0'; }
    char consume() { return pos_ < s_.size() ? s_[pos_++] : '\0'; }
    void expect(char c) {
        skipWs();
        if (peek() != c) throw std::runtime_error(std::string("expected ") + c);
        ++pos_;
    }
    JsonNode parseValue() {
        skipWs();
        char c = peek();
        if (c == '"') return parseString();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') { parseNull(); return JsonNode(); }
        if (c == '-' || std::isdigit((unsigned char)c)) return parseNumber();
        throw std::runtime_error(std::string("unexpected char"));
    }
    JsonNode parseString() {
        expect('"');
        std::string r;
        while (pos_ < s_.size() && peek() != '"') {
            char c = consume();
            if (c == '\\') {
                char e = consume();
                switch (e) {
                    case '"': r += '"';  break;
                    case '\\':r += '\\'; break;
                    case '/': r += '/';  break;
                    case 'n': r += '\n'; break;
                    case 'r': r += '\r'; break;
                    case 't': r += '\t'; break;
                    case 'u': for (int i=0;i<4&&pos_<s_.size();++i) consume(); r+='?'; break;
                    default:  r += e;   break;
                }
            } else { r += c; }
        }
        expect('"');
        JsonNode nd; nd.type = JsonNode::String; nd.str_val = r; return nd;
    }
    JsonNode parseNumber() {
        size_t start = pos_; bool flt = false;
        if (peek() == '-') ++pos_;
        while (pos_ < s_.size() && std::isdigit((unsigned char)s_[pos_])) ++pos_;
        if (peek() == '.') { flt=true; ++pos_; while (pos_<s_.size()&&std::isdigit((unsigned char)s_[pos_])) ++pos_; }
        if (peek()=='e'||peek()=='E') { flt=true; ++pos_; if (peek()=='+'||peek()=='-') ++pos_; while (pos_<s_.size()&&std::isdigit((unsigned char)s_[pos_])) ++pos_; }
        std::string num = s_.substr(start, pos_-start);
        JsonNode nd;
        if (flt) { nd.type=JsonNode::Double; nd.dbl_val=std::atof(num.c_str()); }
        else     { nd.type=JsonNode::Int;    nd.int_val=std::atoll(num.c_str()); }
        return nd;
    }
    JsonNode parseBool() {
        JsonNode nd; nd.type = JsonNode::Bool;
        if (s_.size()-pos_>=4 && s_.substr(pos_,4)=="true")  { pos_+=4; nd.bool_val=true; }
        else if (s_.size()-pos_>=5 && s_.substr(pos_,5)=="false") { pos_+=5; nd.bool_val=false; }
        else throw std::runtime_error("expected bool");
        return nd;
    }
    void parseNull() {
        if (s_.size()-pos_>=4 && s_.substr(pos_,4)=="null") pos_+=4;
        else throw std::runtime_error("expected null");
    }
    JsonNode parseObject() {
        expect('{'); JsonNode nd; nd.type=JsonNode::Object;
        skipWs(); if (peek()=='}') { ++pos_; return nd; }
        while (pos_<s_.size()) {
            skipWs(); if (peek()!='"') throw std::runtime_error("expected key");
            JsonNode k=parseString(); expect(':');
            nd.obj_val[k.str_val]=parseValue();
            skipWs();
            if (peek()==',') { ++pos_; continue; }
            if (peek()=='}') { ++pos_; break; }
            throw std::runtime_error("expected , or }");
        }
        return nd;
    }
    JsonNode parseArray() {
        expect('['); JsonNode nd; nd.type=JsonNode::Array;
        skipWs(); if (peek()==']') { ++pos_; return nd; }
        while (pos_<s_.size()) {
            nd.arr_val.push_back(parseValue()); skipWs();
            if (peek()==',') { ++pos_; continue; }
            if (peek()==']') { ++pos_; break; }
            throw std::runtime_error("expected , or ]");
        }
        return nd;
    }
};
} // namespace detail

inline JsonNode parse(const std::string& input, std::string* err = nullptr) {
    try { detail::Parser p(input); return p.parse(); }
    catch (const std::exception& e) { if (err) *err=e.what(); return JsonNode(); }
}

} // namespace json
} // namespace patrol

#endif // PATROL_UTILS_JSONHELPER_H