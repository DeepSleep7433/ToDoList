#include "TodoCore/TodoItem.h"

#include <chrono>
#include <cstdio>
#include <random>

namespace todolist {

std::string TodoItem::newId() {
    // 32 个 hex 字符（伪 UUID v4 布局：xxxxxxxx-xxxx-4xxx-8xxx-xxxxxxxxxxxx）
    static std::random_device rd;
    static std::mt19937_64 gen((uint64_t(rd()) << 32) ^ (uint64_t(rd()) << 16) ^ rd());
    std::uniform_int_distribution<uint64_t> dist(0, 0xFFFFFFFFFFFFFFFFull);

    const char* hex = "0123456789abcdef";
    char buf[37];
    uint64_t a = dist(gen), b = dist(gen);
    int p = 0;
    for (int i = 0; i < 8; ++i) buf[p++] = hex[(a >> ((7 - i) * 4)) & 0xF];
    buf[p++] = '-';
    for (int i = 0; i < 4; ++i) buf[p++] = hex[(a >> ((3 - i) * 4)) & 0xF];
    buf[p++] = '-';
    buf[p++] = '4'; // version 4
    for (int i = 1; i < 4; ++i) buf[p++] = hex[(b >> ((3 - i) * 4)) & 0xF];
    buf[p++] = '-';
    buf[p++] = hex[((b >> 12) & 0x3) | 0x8]; // variant 10xx
    for (int i = 0; i < 3; ++i) buf[p++] = hex[(b >> ((11 - i) * 4)) & 0xF];
    buf[p++] = '-';
    for (int i = 0; i < 12; ++i) buf[p++] = hex[(b >> ((11 - i) * 4)) & 0xF];
    buf[p] = '\0';
    return std::string(buf);
}

int64_t TodoItem::nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

TodoItem TodoItem::create(const std::string& content) {
    TodoItem it;
    it.id = newId();
    it.content = content;
    const int64_t now = nowMs();
    it.createdAtMs = now;
    it.updatedAtMs = now;
    it.dirty = true; // 本地新建 → 待同步
    return it;
}

} // namespace todolist