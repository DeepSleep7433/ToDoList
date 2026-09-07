#pragma once

// 极简测试断言（无第三方框架，符合"手写"纪律）

#include <cstdio>

namespace th {
inline int& failures() { static int f = 0; return f; }
inline int& checks() { static int c = 0; return c; }
} // namespace th

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++th::checks();                                                      \
        if (!(cond)) {                                                       \
            ++th::failures();                                                \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
        }                                                                    \
    } while (0)

#define CHECK_EQ(a, b)                                                       \
    do {                                                                     \
        ++th::checks();                                                      \
        auto va = (a);                                                       \
        auto vb = (b);                                                       \
        if (!(va == vb)) {                                                   \
            ++th::failures();                                                \
            std::printf("  FAIL %s:%d  %s == %s  (lhs=%lld rhs=%lld)\n",     \
                        __FILE__, __LINE__, #a, #b,                          \
                        static_cast<long long>(va),                          \
                        static_cast<long long>(vb));                         \
        }                                                                    \
    } while (0)

#define RUN_TEST(fn)                                                         \
    do {                                                                     \
        const int before = th::failures();                                   \
        std::printf("[test] %s\n", #fn);                                     \
        fn();                                                                \
        if (th::failures() == before)                                        \
            std::printf("  ok\n");                                           \
    } while (0)