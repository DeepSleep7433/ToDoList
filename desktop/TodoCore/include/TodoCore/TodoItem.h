#pragma once

#include <cstdint>
#include <string>

namespace todolist {

// TodoItem — v1 冻结字段（客户端与服务端共用同一套语义）
//   deleted / serverVersion / dirty 为同步协议预留：
//   deleted        = tombstone（逻辑删除，随同步传播）
//   serverVersion  = 0 表示从未同步；>0 为服务端单调版本号
//   dirty          = 本地待推送标记（M2 起使用）
struct TodoItem {
    std::string id;                 // UUID（客户端生成，离线可新建）
    std::string content;            // UTF-8 文本
    bool isDone = false;
    int64_t createdAtMs = 0;        // epoch ms (UTC)
    int64_t updatedAtMs = 0;
    bool deleted = false;
    int64_t serverVersion = 0;
    bool dirty = false;

    // 生成一个随机 id（32 位十六进制，仿 UUID v4 布局，不做加密用途）
    static std::string newId();
    // 当前 UTC 毫秒时间戳
    static int64_t nowMs();
    // 便捷工厂：自动填充 id / 时间戳 / dirty
    static TodoItem create(const std::string& content);
};

} // namespace todolist