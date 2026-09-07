#pragma once

#include <memory>
#include <string>

namespace todolist {
namespace http {

struct Response {
    int status = 0;        // 0 = 传输层失败
    std::string body;
    std::string error;     // 非空表示网络/协议错误
    bool ok() const { return error.empty() && status >= 200 && status < 300; }
};

// 极简 HTTP/1.1 客户端接口（实现：Winsock；测试可用桩替身）
class Client {
public:
    virtual ~Client() = default;

    virtual Response post(const std::string& url,
                          const std::string& bearerToken,
                          const std::string& body) = 0;
    virtual Response get(const std::string& url,
                         const std::string& bearerToken) = 0;
};

// 创建平台实现；非 Windows 平台抛异常（由错误响应承载）
std::unique_ptr<Client> create();

} // namespace http
} // namespace todolist