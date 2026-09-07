#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace todolist {
namespace json {

// 极简 JSON（手写，仅供 TodoCore 与同步协议使用；非通用库）
struct Value {
    enum Type { Null, Bool, Number, String, Array, Object };

    Type type = Null;
    bool b = false;
    double num = 0;
    std::string str;
    std::vector<Value> arr;
    std::vector<std::pair<std::string, Value>> obj;

    static Value makeNull();
    static Value makeBool(bool v);
    static Value makeNumber(double v);
    static Value makeString(std::string v);
    static Value makeArray();
    static Value makeObject();

    bool isNull() const { return type == Null; }
    bool isBool() const { return type == Bool; }
    bool isNumber() const { return type == Number; }
    bool isString() const { return type == String; }
    bool isArray() const { return type == Array; }
    bool isObject() const { return type == Object; }

    bool asBool() const { return b; }
    double asNumber() const { return num; }
    int64_t asInt64() const { return static_cast<int64_t>(num); }
    const std::string& asString() const { return str; }
    const std::vector<Value>& asArray() const { return arr; }

    // 对象字段访问；不存在或非对象时返回 nullptr
    const Value* find(const std::string& key) const;
    Value* find(const std::string& key);
    // 向对象添加字段（覆盖同键）
    void set(const std::string& key, Value v);
};

// 序列化（紧凑格式）
std::string dump(const Value& v);
// 解析；成功返回 true，失败写 err 并置 out 为 Null
bool parse(const std::string& text, Value& out, std::string* err = nullptr);

// 便于构造：{"k": v}
inline Value objectOf(const std::string& k, Value v) {
    Value o = Value::makeObject();
    o.set(k, std::move(v));
    return o;
}

} // namespace json
} // namespace todolist