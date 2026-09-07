#include "TodoCore/Json.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace todolist {
namespace json {

Value Value::makeNull() { return Value{}; }
Value Value::makeBool(bool v) { Value x; x.type = Bool; x.b = v; return x; }
Value Value::makeNumber(double v) { Value x; x.type = Number; x.num = v; return x; }
Value Value::makeString(std::string v) { Value x; x.type = String; x.str = std::move(v); return x; }
Value Value::makeArray() { Value x; x.type = Array; return x; }
Value Value::makeObject() { Value x; x.type = Object; return x; }

const Value* Value::find(const std::string& key) const {
    if (type != Object) return nullptr;
    for (const auto& kv : obj)
        if (kv.first == key) return &kv.second;
    return nullptr;
}

Value* Value::find(const std::string& key) {
    if (type != Object) return nullptr;
    for (auto& kv : obj)
        if (kv.first == key) return &kv.second;
    return nullptr;
}

void Value::set(const std::string& key, Value v) {
    if (type != Object) {
        type = Object;
        obj.clear();
    }
    for (auto& kv : obj) {
        if (kv.first == key) {
            kv.second = std::move(v);
            return;
        }
    }
    obj.emplace_back(key, std::move(v));
}

// ---------- 序列化 ----------

namespace {
void escapeString(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c); // UTF-8 原样
                }
        }
    }
    out += '"';
}

void dumpTo(const Value& v, std::string& out) {
    switch (v.type) {
        case Value::Null: out += "null"; break;
        case Value::Bool: out += v.b ? "true" : "false"; break;
        case Value::Number: {
            if (std::isfinite(v.num)) {
                char buf[40];
                std::snprintf(buf, sizeof buf, "%.17g", v.num);
                out += buf;
            } else {
                out += "null";
            }
            break;
        }
        case Value::String: escapeString(out, v.str); break;
        case Value::Array: {
            out += '[';
            bool first = true;
            for (const auto& e : v.arr) {
                if (!first) out += ',';
                first = false;
                dumpTo(e, out);
            }
            out += ']';
            break;
        }
        case Value::Object: {
            out += '{';
            bool first = true;
            for (const auto& kv : v.obj) {
                if (!first) out += ',';
                first = false;
                escapeString(out, kv.first);
                out += ':';
                dumpTo(kv.second, out);
            }
            out += '}';
            break;
        }
    }
}
} // namespace

std::string dump(const Value& v) {
    std::string out;
    dumpTo(v, out);
    return out;
}

// ---------- 解析 ----------

namespace {

struct Parser {
    const std::string& s;
    size_t i = 0;
    std::string err;

    explicit Parser(const std::string& text) : s(text) {}

    void skipWs() {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    }

    bool fail(const std::string& m) {
        if (err.empty()) err = m + " (offset " + std::to_string(i) + ")";
        return false;
    }

    bool parseValue(Value& out) {
        skipWs();
        if (i >= s.size()) return fail("unexpected end");
        char c = s[i];
        if (c == '{') return parseObject(out);
        if (c == '[') return parseArray(out);
        if (c == '"') return parseString(out);
        if (c == 't' || c == 'f') return parseBool(out);
        if (c == 'n') return parseNull(out);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parseNumber(out);
        return fail(std::string("unexpected char '") + c + "'");
    }

    bool parseObject(Value& out) {
        out = Value::makeObject();
        ++i; // {
        skipWs();
        if (i < s.size() && s[i] == '}') { ++i; return true; }
        while (true) {
            skipWs();
            if (i >= s.size() || s[i] != '"') return fail("expect object key");
            Value keyV;
            if (!parseString(keyV)) return false;
            skipWs();
            if (i >= s.size() || s[i] != ':') return fail("expect ':'");
            ++i;
            Value val;
            if (!parseValue(val)) return false;
            out.set(keyV.str, std::move(val));
            skipWs();
            if (i >= s.size()) return fail("unterminated object");
            char c = s[i];
            ++i;
            if (c == '}') return true;
            if (c != ',') return fail("expect ',' or '}'");
        }
    }

    bool parseArray(Value& out) {
        out = Value::makeArray();
        ++i; // [
        skipWs();
        if (i < s.size() && s[i] == ']') { ++i; return true; }
        while (true) {
            Value val;
            if (!parseValue(val)) return false;
            out.arr.push_back(std::move(val));
            skipWs();
            if (i >= s.size()) return fail("unterminated array");
            char c = s[i];
            ++i;
            if (c == ']') return true;
            if (c != ',') return fail("expect ',' or ']'");
        }
    }

    void appendUtf8(std::string& out, uint32_t cp) {
        if (cp <= 0x7F) {
            out += static_cast<char>(cp);
        } else if (cp <= 0x7FF) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool parseHex4(uint32_t& out) {
        if (i + 4 > s.size()) return false;
        uint32_t v = 0;
        for (int k = 0; k < 4; ++k) {
            char c = s[i + k];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<uint32_t>(c - 'A' + 10);
            else return false;
        }
        i += 4;
        out = v;
        return true;
    }

    bool parseString(Value& out) {
        ++i; // "
        std::string str;
        while (true) {
            if (i >= s.size()) return fail("unterminated string");
            char c = s[i];
            if (c == '"') { ++i; break; }
            if (c == '\\') {
                ++i;
                if (i >= s.size()) return fail("bad escape");
                char e = s[i];
                ++i;
                switch (e) {
                    case '"': str += '"'; break;
                    case '\\': str += '\\'; break;
                    case '/': str += '/'; break;
                    case 'b': str += '\b'; break;
                    case 'f': str += '\f'; break;
                    case 'n': str += '\n'; break;
                    case 'r': str += '\r'; break;
                    case 't': str += '\t'; break;
                    case 'u': {
                        uint32_t cp = 0;
                        if (!parseHex4(cp)) return fail("bad \\u escape");
                        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                            i += 2;
                            uint32_t lo = 0;
                            if (!parseHex4(lo)) return fail("bad surrogate");
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        }
                        appendUtf8(str, cp);
                        break;
                    }
                    default: return fail("bad escape char");
                }
            } else {
                str += c;
                ++i;
            }
        }
        out = Value::makeString(std::move(str));
        return true;
    }

    bool parseBool(Value& out) {
        if (s.compare(i, 4, "true") == 0) { i += 4; out = Value::makeBool(true); return true; }
        if (s.compare(i, 5, "false") == 0) { i += 5; out = Value::makeBool(false); return true; }
        return fail("bad bool");
    }

    bool parseNull(Value& out) {
        if (s.compare(i, 4, "null") == 0) { i += 4; out = Value::makeNull(); return true; }
        return fail("bad null");
    }

    bool parseNumber(Value& out) {
        size_t start = i;
        if (i < s.size() && s[i] == '-') ++i;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        if (i < s.size() && s[i] == '.') {
            ++i;
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        }
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            ++i;
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        }
        const std::string tok = s.substr(start, i - start);
        char* end = nullptr;
        const double d = std::strtod(tok.c_str(), &end);
        if (end == tok.c_str()) return fail("bad number");
        out = Value::makeNumber(d);
        return true;
    }
};
} // namespace

bool parse(const std::string& text, Value& out, std::string* err) {
    Parser p(text);
    if (!p.parseValue(out)) {
        out = Value::makeNull();
        if (err) *err = p.err;
        return false;
    }
    p.skipWs();
    if (p.i != text.size()) {
        out = Value::makeNull();
        if (err) *err = "trailing content";
        return false;
    }
    return true;
}

} // namespace json
} // namespace todolist