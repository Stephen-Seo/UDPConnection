#include <gtest/gtest.h>

#include <UDPConnection.h>
#include <UDPC_Defines.hpp>

#include <cstring>
#include <future>

TEST(UDPC, atostr) {
    UDPC::Context context(false);

    UDPC_ConnectionId conId;
    const char* resultBuf;

    for(unsigned int i = 0; i < 16; ++i) {
        conId.addr.s6_addr[i] = (i % 3 == 0 ? 0xFF : (i % 3 == 1 ? 0x12 : 0x56));
    }
    resultBuf = UDPC_atostr((UDPC_HContext)&context, conId);
    EXPECT_STREQ(resultBuf, "ff12:56ff:1256:ff12:56ff:1256:ff12:56ff");

    for(unsigned int i = 0; i < 8; ++i) {
        conId.addr.s6_addr[i] = 0;
    }
    resultBuf = UDPC_atostr((UDPC_HContext)&context, conId);
    EXPECT_STREQ(resultBuf, "::56ff:1256:ff12:56ff");

    conId.addr.s6_addr[0] = 1;
    conId.addr.s6_addr[1] = 2;
    resultBuf = UDPC_atostr((UDPC_HContext)&context, conId);
    EXPECT_STREQ(resultBuf, "12::56ff:1256:ff12:56ff");

    conId.addr.s6_addr[14] = 0;
    conId.addr.s6_addr[15] = 0;
    resultBuf = UDPC_atostr((UDPC_HContext)&context, conId);
    EXPECT_STREQ(resultBuf, "12::56ff:1256:ff12:0");

    for(unsigned int i = 0; i < 15; ++i) {
        conId.addr.s6_addr[i] = 0;
    }
    conId.addr.s6_addr[15] = 1;

    resultBuf = UDPC_atostr((UDPC_HContext)&context, conId);
    EXPECT_STREQ(resultBuf, "::1");

    conId.addr.s6_addr[15] = 0;

    resultBuf = UDPC_atostr((UDPC_HContext)&context, conId);
    EXPECT_STREQ(resultBuf, "::");
}

TEST(UDPC, atostr_concurrent) {
    UDPC::Context context(false);

    const char* results[64] = {
        "::1111:1",
        "::1111:2",
        "::1111:3",
        "::1111:4",
        "::1111:5",
        "::1111:6",
        "::1111:7",
        "::1111:8",
        "::1111:9",
        "::1111:a",
        "::1111:b",
        "::1111:c",
        "::1111:d",
        "::1111:e",
        "::1111:f",
        "::1111:10",
        "::1111:11",
        "::1111:12",
        "::1111:13",
        "::1111:14",
        "::1111:15",
        "::1111:16",
        "::1111:17",
        "::1111:18",
        "::1111:19",
        "::1111:1a",
        "::1111:1b",
        "::1111:1c",
        "::1111:1d",
        "::1111:1e",
        "::1111:1f",
        "::1111:20",
        "::1111:21",
        "::1111:22",
        "::1111:23",
        "::1111:24",
        "::1111:25",
        "::1111:26",
        "::1111:27",
        "::1111:28",
        "::1111:29",
        "::1111:2a",
        "::1111:2b",
        "::1111:2c",
        "::1111:2d",
        "::1111:2e",
        "::1111:2f",
        "::1111:30",
        "::1111:31",
        "::1111:32",
        "::1111:33",
        "::1111:34",
        "::1111:35",
        "::1111:36",
        "::1111:37",
        "::1111:38",
        "::1111:39",
        "::1111:3a",
        "::1111:3b",
        "::1111:3c",
        "::1111:3d",
        "::1111:3e",
        "::1111:3f",
        "::1111:40"
    };

    std::future<void> futures[32];
    const char* ptrs[32];
    for(unsigned int i = 0; i < 2; ++i) {
        for(unsigned int j = 0; j < 32; ++j) {
            futures[j] = std::async(std::launch::async, [] (unsigned int id, const char** ptr, UDPC::Context* c) {
                UDPC_ConnectionId conId = {
                    {0, 0, 0, 0,
                    0, 0, 0, 0,
                    0, 0, 0, 0,
                    0x11, 0x11, 0x0, (unsigned char)(id + 1)},
                    0
                };
                ptr[id] = UDPC_atostr((UDPC_HContext)c, conId);
            }, j, ptrs, &context);
        }
        for(unsigned int j = 0; j < 32; ++j) {
            ASSERT_TRUE(futures[j].valid());
            futures[j].wait();
        }
        for(unsigned int j = 0; j < 32; ++j) {
            EXPECT_STREQ(ptrs[j], results[j]);
        }
    }
}

TEST(UDPC, strtoa) {
    struct in6_addr addr;

    for(unsigned int i = 0; i < 16; ++i) {
        addr.s6_addr[i] = 0;
    }
    addr.s6_addr[15] = 1;

    EXPECT_EQ(UDPC_strtoa("::1"), addr);

    // check invalid
    EXPECT_EQ(UDPC_strtoa("1:1::1:1::1"), addr);
    EXPECT_EQ(UDPC_strtoa("derpadoodle"), addr);

    addr = {
        0xF0, 0xF, 0x0, 0x1,
        0x56, 0x78, 0x9A, 0xBC,
        0xDE, 0xFF, 0x1, 0x2,
        0x3, 0x4, 0x5, 0x6
    };
    EXPECT_EQ(UDPC_strtoa("F00F:1:5678:9abc:deff:102:304:506"), addr);

    addr = {
        0x0, 0xFF, 0x1, 0x0,
        0x0, 0x1, 0x10, 0x0,
        0x0, 0x0, 0x0, 0x0,
        0x12, 0x34, 0xab, 0xcd
    };
    EXPECT_EQ(UDPC_strtoa("ff:100:1:1000::1234:abcd"), addr);
}
