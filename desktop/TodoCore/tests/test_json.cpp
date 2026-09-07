#include <cstdio>
#include <string>

#include "TodoCore/Json.h"
#include "test_harness.h"

using todolist::json::Value;
namespace json = todolist::json;

static void testRoundtripBasic() {
    std::string in = R"({"id":"abc","content":"买 牛奶 \"x\"","isDone":true,"deleted":false,"n":1.5,"arr":[1,"two",null],"empty":{}})";
    Value v;
    std::string err;
    CHECK(json::parse(in, v, &err));
    CHECK(err.empty());
    CHECK(v.isObject());
    const Value* id = v.find("id");
    CHECK(id && id->isString() && id->asString() == "abc");
    const Value* content = v.find("content");
    CHECK(content && content->asString() == "买 牛奶 \"x\"");
    const Value* done = v.find("isDone");
    CHECK(done && done->isBool() && done->asBool());
    const Value* del = v.find("deleted");
    CHECK(del && del->isBool() && !del->asBool());
    const Value* n = v.find("n");
    CHECK(n && n->isNumber());
    const Value* arr = v.find("arr");
    CHECK(arr && arr->isArray() && arr->asArray().size() == 3);
    // 序列化后可再解析且等价
    const std::string out = json::dump(v);
    Value v2;
    CHECK(json::parse(out, v2, &err));
    CHECK(v2.find("content")->asString() == "买 牛奶 \"x\"");
}

static void testUnicodeEscapes() {
    std::string in = R"({"s":"\u4e70\u725b\u5976","e":"a\nb\tc"})";
    Value v;
    std::string err;
    CHECK(json::parse(in, v, &err));
    CHECK(v.find("s")->asString() == "买牛奶");
    CHECK(v.find("e")->asString() == "a\nb\tc");
}

static void testParseErrors() {
    const char* bad[] = {"{", "[1,]", "{\"a\":}", "nul", "{\"a\":1}x"};
    for (const char* t : bad) {
        Value v;
        std::string err;
        CHECK(!json::parse(t, v, &err));
    }
}

static void testBuildAndDump() {
    Value o = Value::makeObject();
    o.set("serverTime", Value::makeString("2026-09-07T05:17:29.009Z"));
    Value arr = Value::makeArray();
    Value it = Value::makeObject();
    it.set("id", Value::makeString("x"));
    it.set("isDone", Value::makeBool(true));
    it.set("deleted", Value::makeBool(false));
    it.set("serverVersion", Value::makeNumber(3));
    arr.arr.push_back(it);
    o.set("items", std::move(arr));
    const std::string s = json::dump(o);
    CHECK(s.find("\"serverTime\":\"2026-09-07T05:17:29.009Z\"") != std::string::npos);
    CHECK(s.find("\"items\":[{\"id\":\"x\",\"isDone\":true") != std::string::npos);
}

int main() {
    RUN_TEST(testRoundtripBasic);
    RUN_TEST(testUnicodeEscapes);
    RUN_TEST(testParseErrors);
    RUN_TEST(testBuildAndDump);
    std::printf("\n%d checks, %d failures\n", th::checks(), th::failures());
    return th::failures() == 0 ? 0 : 1;
}