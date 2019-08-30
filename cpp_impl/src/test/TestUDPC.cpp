#include <gtest/gtest.h>

#include <UDPConnection.h>
#include <UDPC_Defines.hpp>

#include <cstring>

TEST(UDPC, atostr) {
    UDPC::Context context(false);

    UDPC_atostr((UDPC_HContext)&context, 0x0100007F);
    EXPECT_EQ(std::strcmp(context.atostrBuf, "127.0.0.1"), 0);

    UDPC_atostr((UDPC_HContext)&context, 0xFF08000A);
    EXPECT_EQ(std::strcmp(context.atostrBuf, "10.0.8.255"), 0);

    UDPC_atostr((UDPC_HContext)&context, 0x0201A8C0);
    EXPECT_EQ(std::strcmp(context.atostrBuf, "192.168.1.2"), 0);
}

TEST(UDPC, strtoa) {
    EXPECT_EQ(UDPC_strtoa("127.0.0.1"), 0x0100007F);
    EXPECT_EQ(UDPC_strtoa("10.0.8.255"), 0xFF08000A);
    EXPECT_EQ(UDPC_strtoa("192.168.1.2"), 0x0201A8C0);
}
