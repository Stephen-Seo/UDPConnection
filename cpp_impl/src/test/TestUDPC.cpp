#include <gtest/gtest.h>

#include <UDPConnection.h>
#include <UDPC_Defines.hpp>

#include <cstring>
#include <future>

TEST(UDPC, atostr) {
    UDPC::Context context(false);

    const char* resultBuf;

    resultBuf = UDPC_atostr((UDPC_HContext)&context, 0x0100007F);
    EXPECT_EQ(std::strcmp(resultBuf, "127.0.0.1"), 0);

    resultBuf = UDPC_atostr((UDPC_HContext)&context, 0xFF08000A);
    EXPECT_EQ(std::strcmp(resultBuf, "10.0.8.255"), 0);

    resultBuf = UDPC_atostr((UDPC_HContext)&context, 0x0201A8C0);
    EXPECT_EQ(std::strcmp(resultBuf, "192.168.1.2"), 0);
}

TEST(UDPC, atostr_concurrent) {
    UDPC::Context context(false);

    const char* results[64] = {
        "0.0.0.0",
        "1.1.1.1",
        "2.2.2.2",
        "3.3.3.3",
        "4.4.4.4",
        "5.5.5.5",
        "6.6.6.6",
        "7.7.7.7",
        "8.8.8.8",
        "9.9.9.9",
        "10.10.10.10",
        "11.11.11.11",
        "12.12.12.12",
        "13.13.13.13",
        "14.14.14.14",
        "15.15.15.15",
        "16.16.16.16",
        "17.17.17.17",
        "18.18.18.18",
        "19.19.19.19",
        "20.20.20.20",
        "21.21.21.21",
        "22.22.22.22",
        "23.23.23.23",
        "24.24.24.24",
        "25.25.25.25",
        "26.26.26.26",
        "27.27.27.27",
        "28.28.28.28",
        "29.29.29.29",
        "30.30.30.30",
        "31.31.31.31",
        "32.32.32.32",
        "33.33.33.33",
        "34.34.34.34",
        "35.35.35.35",
        "36.36.36.36",
        "37.37.37.37",
        "38.38.38.38",
        "39.39.39.39",
        "40.40.40.40",
        "41.41.41.41",
        "42.42.42.42",
        "43.43.43.43",
        "44.44.44.44",
        "45.45.45.45",
        "46.46.46.46",
        "47.47.47.47",
        "48.48.48.48",
        "49.49.49.49",
        "50.50.50.50",
        "51.51.51.51",
        "52.52.52.52",
        "53.53.53.53",
        "54.54.54.54",
        "55.55.55.55",
        "56.56.56.56",
        "57.57.57.57",
        "58.58.58.58",
        "59.59.59.59",
        "60.60.60.60",
        "61.61.61.61",
        "62.62.62.62",
        "63.63.63.63"
    };

    std::future<void> futures[64];
    const char* ptrs[64];
    for(unsigned int i = 0; i < 2; ++i) {
        for(unsigned int j = 0; j < 64; ++j) {
            futures[j] = std::async(std::launch::async, [] (unsigned int id, const char** ptr, UDPC::Context* c) {
                ptr[id] = UDPC_atostr((UDPC_HContext)c, id | (id << 8) | (id << 16) | (id << 24));
            }, j, ptrs, &context);
        }
        for(unsigned int j = 0; j < 64; ++j) {
            ASSERT_TRUE(futures[j].valid());
            futures[j].wait();
        }
        for(unsigned int j = 0; j < 64; ++j) {
            EXPECT_EQ(std::strcmp(ptrs[j], results[j]), 0);
        }
    }
}

TEST(UDPC, strtoa) {
    EXPECT_EQ(UDPC_strtoa("127.0.0.1"), 0x0100007F);
    EXPECT_EQ(UDPC_strtoa("10.0.8.255"), 0xFF08000A);
    EXPECT_EQ(UDPC_strtoa("192.168.1.2"), 0x0201A8C0);
}
