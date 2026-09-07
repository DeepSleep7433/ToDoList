#include "TodoCore/HttpClient.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

namespace todolist {
namespace http {
namespace {

#ifdef _WIN32

struct WsaInit {
    WsaInit() {
        WSADATA d;
        WSAStartup(MAKEWORD(2, 2), &d);
    }
    ~WsaInit() { WSACleanup(); }
};

// 解析 http://host[:port]/path → host/port/path
bool splitUrl(const std::string& url, std::string& host, std::string& port,
              std::string& path) {
    const std::string prefix = "http://";
    if (url.compare(0, prefix.size(), prefix) != 0) return false;
    size_t p = prefix.size();
    size_t slash = url.find('/', p);
    std::string authority = (slash == std::string::npos)
        ? url.substr(p)
        : url.substr(p, slash - p);
    path = (slash == std::string::npos) ? "/" : url.substr(slash);
    size_t colon = authority.find(':');
    if (colon == std::string::npos) {
        host = authority;
        port = "80";
    } else {
        host = authority.substr(0, colon);
        port = authority.substr(colon + 1);
    }
    return !host.empty();
}

class WinClient : public Client {
public:
    Response post(const std::string& url, const std::string& token,
                  const std::string& body) override {
        return roundtrip("POST", url, token, body);
    }
    Response get(const std::string& url, const std::string& token) override {
        return roundtrip("GET", url, token, "");
    }

private:
    static Response roundtrip(const char* method, const std::string& url,
                              const std::string& token, const std::string& body) {
        Response out;
        static WsaInit wsa;
        std::string host, port, path;
        if (!splitUrl(url, host, port, path)) {
            out.error = "bad url: " + url;
            return out;
        }

        addrinfo hints;
        std::memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        addrinfo* res = nullptr;
        if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0 || !res) {
            out.error = "dns failed: " + host;
            return out;
        }
        SOCKET sock = INVALID_SOCKET;
        for (addrinfo* it = res; it; it = it->ai_next) {
            sock = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
            if (sock == INVALID_SOCKET) continue;
            if (connect(sock, it->ai_addr, static_cast<int>(it->ai_addrlen)) == 0) break;
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
        freeaddrinfo(res);
        if (sock == INVALID_SOCKET) {
            out.error = "connect failed: " + host + ":" + port;
            return out;
        }
        const int tmoMs = 10000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&tmoMs), sizeof tmoMs);
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&tmoMs), sizeof tmoMs);

        std::string req = std::string(method) + " " + path + " HTTP/1.1\r\n"
            "Host: " + host + ":" + port + "\r\n"
            "User-Agent: todolist-sync/0.1\r\n"
            "Accept: application/json\r\n";
        if (!token.empty())
            req += "Authorization: Bearer " + token + "\r\n";
        if (!body.empty()) {
            req += "Content-Type: application/json\r\n";
            req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        }
        req += "Connection: close\r\n\r\n";
        if (!body.empty()) req += body;

        size_t sent = 0;
        while (sent < req.size()) {
            int n = ::send(sock, req.data() + sent,
                           static_cast<int>(req.size() - sent), 0);
            if (n <= 0) {
                out.error = "send failed";
                closesocket(sock);
                return out;
            }
            sent += static_cast<size_t>(n);
        }

        std::string raw;
        char buf[8192];
        for (;;) {
            int n = ::recv(sock, buf, sizeof buf, 0);
            if (n > 0) {
                raw.append(buf, static_cast<size_t>(n));
            } else {
                break;
            }
        }
        closesocket(sock);
        return parseResponse(std::move(raw));
    }

    static Response parseResponse(std::string raw) {
        Response out;
        size_t headEnd = raw.find("\r\n\r\n");
        if (headEnd == std::string::npos) {
            out.error = "malformed response";
            return out;
        }
        const std::string head = raw.substr(0, headEnd);
        std::string rest = raw.substr(headEnd + 4);

        // 状态行
        size_t sp1 = head.find(' ');
        if (sp1 == std::string::npos) {
            out.error = "malformed status line";
            return out;
        }
        out.status = std::atoi(head.c_str() + sp1 + 1);

        // 头
        bool chunked = false;
        size_t cl = std::string::npos;
        size_t p = 0;
        while (p < head.size()) {
            size_t eol = head.find("\r\n", p);
            if (eol == std::string::npos) break;
            const std::string line = head.substr(p, eol - p);
            p = eol + 2;
            size_t colon = line.find(':');
            if (colon == std::string::npos) continue;
            std::string name = line.substr(0, colon);
            std::string val = line.substr(colon + 1);
            while (!val.empty() && std::isspace(static_cast<unsigned char>(val.front())))
                val.erase(val.begin());
            for (auto& c : name)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (name == "content-length") cl = std::strtoull(val.c_str(), nullptr, 10);
            else if (name == "transfer-encoding" && val.find("chunked") != std::string::npos)
                chunked = true;
        }

        if (chunked) {
            out.body = decodeChunked(rest);
        } else if (cl != std::string::npos && rest.size() > cl) {
            rest.resize(cl);
            out.body = std::move(rest);
        } else {
            out.body = std::move(rest);
        }
        return out;
    }

    // 解码 chunked 响应体（http/1.1 Transfer-Encoding: chunked）
    static std::string decodeChunked(const std::string& body) {
        std::string out;
        size_t i = 0;
        while (i < body.size()) {
            size_t eol = body.find("\r\n", i);
            if (eol == std::string::npos) break;
            std::string sizeLine = body.substr(i, eol - i);
            const size_t semi = sizeLine.find(';'); // chunk-extensions 忽略
            if (semi != std::string::npos) sizeLine.resize(semi);
            char* end = nullptr;
            const unsigned long chunk = std::strtoul(sizeLine.c_str(), &end, 16);
            i = eol + 2;
            if (chunk == 0) break; // 终止块
            if (i + chunk > body.size()) break; // 不完整
            out.append(body, i, chunk);
            i += chunk;
            if (i + 1 < body.size() && body[i] == '\r' && body[i + 1] == '\n') i += 2;
            else break;
        }
        return out;
    }
};

#else
class StubClient : public Client {
public:
    Response post(const std::string&, const std::string&, const std::string&) override {
        Response r;
        r.error = "http client not implemented on this platform";
        return r;
    }
    Response get(const std::string&, const std::string&) override {
        Response r;
        r.error = "http client not implemented on this platform";
        return r;
    }
};
#endif

} // namespace

std::unique_ptr<Client> create() {
#ifdef _WIN32
    return std::make_unique<WinClient>();
#else
    return std::make_unique<StubClient>();
#endif
}

} // namespace http
} // namespace todolist