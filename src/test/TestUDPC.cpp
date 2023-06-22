#include <gtest/gtest.h>

#include <UDPC.h>
#include <UDPC_Defines.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>

TEST(UDPC, atostr) {
    UDPC::Context context(false);

    UDPC_ConnectionId conId;
    const char* resultBuf;

    for(unsigned int i = 0; i < 16; ++i) {
        conId.addr.s6_addr[i] = (i % 3 == 0 ? 0xFF : (i % 3 == 1 ? 0x12 : 0x56));
    }
    resultBuf = UDPC_atostr((UDPC_HContext)&context, conId.addr);
    EXPECT_STREQ(resultBuf, "ff12:56ff:1256:ff12:56ff:1256:ff12:56ff");

    for(unsigned int i = 0; i < 8; ++i) {
        conId.addr.s6_addr[i] = 0;
    }
    resultBuf = UDPC_atostr((UDPC_HContext)&context, conId.addr);
    EXPECT_STREQ(resultBuf, "::56ff:1256:ff12:56ff");

    conId.addr.s6_addr[0] = 1;
    conId.addr.s6_addr[1] = 2;
    resultBuf = UDPC_atostr((UDPC_HContext)&context, conId.addr);
    EXPECT_STREQ(resultBuf, "102::56ff:1256:ff12:56ff");

    conId.addr.s6_addr[14] = 0;
    conId.addr.s6_addr[15] = 0;
    resultBuf = UDPC_atostr((UDPC_HContext)&context, conId.addr);
    EXPECT_STREQ(resultBuf, "102::56ff:1256:ff12:0");

    for(unsigned int i = 0; i < 15; ++i) {
        conId.addr.s6_addr[i] = 0;
    }
    conId.addr.s6_addr[15] = 1;

    resultBuf = UDPC_atostr((UDPC_HContext)&context, conId.addr);
    EXPECT_STREQ(resultBuf, "::1");

    conId.addr.s6_addr[15] = 0;

    resultBuf = UDPC_atostr((UDPC_HContext)&context, conId.addr);
    EXPECT_STREQ(resultBuf, "::");

    conId.addr = {
        0xAE, 0x0, 0x12, 1,
        0x10, 0x45, 0x2, 0x13,
        0, 0, 0, 0,
        0, 0, 0, 0
    };
    resultBuf = UDPC_atostr((UDPC_HContext)&context, conId.addr);
    EXPECT_STREQ(resultBuf, "ae00:1201:1045:213::");
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
                    0,
                    0
                };
                ptr[id] = UDPC_atostr((UDPC_HContext)c, conId.addr);
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

TEST(UDPC, atostr_unsafe) {
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
            futures[j] = std::async(std::launch::async, [] (unsigned int id, const char** ptr) {
                UDPC_ConnectionId conId = {
                    {0, 0, 0, 0,
                    0, 0, 0, 0,
                    0, 0, 0, 0,
                    0x11, 0x11, 0x0, (unsigned char)(id + 1)},
                    0,
                    0
                };
                ptr[id] = UDPC_atostr_unsafe(conId.addr);
            }, j, ptrs);
        }
        for(unsigned int j = 0; j < 32; ++j) {
            ASSERT_TRUE(futures[j].valid());
            futures[j].wait();
        }
        for(unsigned int j = 0; j < 32; ++j) {
            EXPECT_STREQ(ptrs[j], results[j]);
            UDPC_atostr_unsafe_free_ptr(ptrs + j);
            UDPC_atostr_unsafe_free_ptr(ptrs + j);
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

    addr = {
        0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0xFF, 0xFF,
        0x7F, 0x0, 0x0, 0x1
    };
    EXPECT_EQ(UDPC_strtoa("127.0.0.1"), addr);

    addr = {
        0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0xFF, 0xFF,
        0xA, 0x1, 0x2, 0x3
    };
    EXPECT_EQ(UDPC_strtoa("10.1.2.3"), addr);
}

TEST(UDPC, create_id_easy) {
    UDPC_ConnectionId conId;

    // not link local
    conId = UDPC_create_id_easy("::FFFF:7F00:1", 301);
    for(unsigned int i = 0; i < 10; ++i) {
        EXPECT_EQ(UDPC_IPV6_ADDR_SUB(conId.addr)[i], 0);
    }
    EXPECT_EQ(UDPC_IPV6_ADDR_SUB(conId.addr)[10], 0xFF);
    EXPECT_EQ(UDPC_IPV6_ADDR_SUB(conId.addr)[11], 0xFF);
    EXPECT_EQ(UDPC_IPV6_ADDR_SUB(conId.addr)[12], 0x7F);
    EXPECT_EQ(UDPC_IPV6_ADDR_SUB(conId.addr)[13], 0);
    EXPECT_EQ(UDPC_IPV6_ADDR_SUB(conId.addr)[14], 0);
    EXPECT_EQ(UDPC_IPV6_ADDR_SUB(conId.addr)[15], 0x1);

    EXPECT_EQ(conId.scope_id, 0);
    EXPECT_EQ(conId.port, 301);

    // link local
    conId = UDPC_create_id_easy("fe80::1234:5678:9%3", 123);
    EXPECT_EQ(UDPC_IPV6_ADDR_SUB(conId.addr)[0], 0xFE);
    EXPECT_EQ(UDPC_IPV6_ADDR_SUB(conId.addr)[1], 0x80);
    for(unsigned int i = 2; i < 10; ++i) {
        EXPECT_EQ(UDPC_IPV6_ADDR_SUB(conId.addr)[i], 0);
    }
    EXPECT_EQ(UDPC_IPV6_ADDR_SUB(conId.addr)[10], 0x12);
    EXPECT_EQ(UDPC_IPV6_ADDR_SUB(conId.addr)[11], 0x34);
    EXPECT_EQ(UDPC_IPV6_ADDR_SUB(conId.addr)[12], 0x56);
    EXPECT_EQ(UDPC_IPV6_ADDR_SUB(conId.addr)[13], 0x78);
    EXPECT_EQ(UDPC_IPV6_ADDR_SUB(conId.addr)[14], 0);
    EXPECT_EQ(UDPC_IPV6_ADDR_SUB(conId.addr)[15], 0x9);

    EXPECT_EQ(conId.scope_id, 3);
    EXPECT_EQ(conId.port, 123);
}

TEST(UDPC, ConnectionIdBits) {
    UDPC_ConnectionId id = UDPC_create_id({0}, 0);
    for(unsigned int i = 0; i < sizeof(UDPC_ConnectionId); ++i) {
        EXPECT_EQ(((char*)&id)[i], 0);
    }

    id = UDPC_create_id_full({0}, 0, 0);
    for(unsigned int i = 0; i < sizeof(UDPC_ConnectionId); ++i) {
        EXPECT_EQ(((char*)&id)[i], 0);
    }

    id = UDPC_create_id_anyaddr(0);
    for(unsigned int i = 0; i < sizeof(UDPC_ConnectionId); ++i) {
        EXPECT_EQ(((char*)&id)[i], 0);
    }

    id = UDPC_create_id_easy("::", 0);
    for(unsigned int i = 0; i < sizeof(UDPC_ConnectionId); ++i) {
        EXPECT_EQ(((char*)&id)[i], 0);
    }
}

TEST(UDPC, NetworkOrderEndianness) {
    if(UDPC_is_big_endian() != 0) {
        puts("Is big-endian");
        uint16_t s = 0x0102;
        s = UDPC_no16i(s);
        EXPECT_EQ(s, 0x0102);

        uint32_t l = 0x01020304;
        l = UDPC_no32i(l);
        EXPECT_EQ(l, 0x01020304);

        uint64_t ll = 0x0102030405060708;
        ll = UDPC_no64i(ll);
        EXPECT_EQ(ll, 0x0102030405060708);

        l = 0x40208040;
        float *f = reinterpret_cast<float*>(&l);
        *f = UDPC_no32f(*f);
        EXPECT_EQ(l, 0x40208040);

        ll = 0x4000001010008040;
        double *d = reinterpret_cast<double*>(&ll);
        *d = UDPC_no64f(*d);
        EXPECT_EQ(ll, 0x4000001010008040);
    } else {
        puts("Is NOT big-endian");
        uint16_t s = 0x0102;
        s = UDPC_no16i(s);
        EXPECT_EQ(s, 0x0201);
        s = UDPC_no16i(s);
        EXPECT_EQ(s, 0x0102);

        uint32_t l = 0x01020304;
        l = UDPC_no32i(l);
        EXPECT_EQ(l, 0x04030201);
        l = UDPC_no32i(l);
        EXPECT_EQ(l, 0x01020304);

        uint64_t ll = 0x0102030405060708;
        ll = UDPC_no64i(ll);
        EXPECT_EQ(ll, 0x0807060504030201);
        ll = UDPC_no64i(ll);
        EXPECT_EQ(ll, 0x0102030405060708);

        l = 0x40208040;
        float *f = reinterpret_cast<float*>(&l);
        *f = UDPC_no32f(*f);
        EXPECT_EQ(l, 0x40802040);
        *f = UDPC_no32f(*f);
        EXPECT_EQ(l, 0x40208040);

        ll = 0x4000001010008040;
        double *d = reinterpret_cast<double*>(&ll);
        *d = UDPC_no64f(*d);
        EXPECT_EQ(ll, 0x4080001010000040);
        *d = UDPC_no64f(*d);
        EXPECT_EQ(ll, 0x4000001010008040);
    }
}

TEST(UDPC, a4toa6) {
    EXPECT_EQ(UDPC_a4toa6(0), in6addr_any);
    uint32_t a4 = htonl(0x7F000001);
    EXPECT_EQ(UDPC_a4toa6(a4), in6addr_loopback);

    UDPC_IPV6_ADDR_TYPE a6 = UDPC_strtoa("::FFFF:0102:0304");
    a4 = htonl(0x01020304);
    EXPECT_EQ(UDPC_a4toa6(a4), a6);
}

TEST(UDPC, free_packet_ptr) {
    UDPC_PacketInfo pinfo;
    pinfo.dataSize = 8;
    pinfo.data = (char*)std::malloc(pinfo.dataSize);

    UDPC_free_PacketInfo_ptr(&pinfo);
    UDPC_free_PacketInfo_ptr(&pinfo);
    UDPC_free_PacketInfo_ptr(nullptr);
}
