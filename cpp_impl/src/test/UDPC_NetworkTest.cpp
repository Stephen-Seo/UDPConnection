#include <cstring>
#include <string>
#include <cstdio>
#include <thread>
#include <chrono>
#include <regex>
#include <vector>

#include <UDPConnection.h>

static const std::regex ipv6_regex_linkonly = std::regex(R"d(fe80:(:[0-9a-fA-F]{0,4}){0,4}%[0-9a-zA-Z]{1,})d");

void usage() {
    puts("[-c | -s] - client or server (default server)");
    puts("-ll <addr> - listen addr");
    puts("-lp <port> - listen port");
    puts("-cl <addr> - connection addr (client only)");
    puts("-cp <port> - connection port (client only)");
    puts("-t <tick_count>");
    puts("-n - do not add payload to packets");
}

int main(int argc, char **argv) {
    --argc; ++argv;
    if(argc == 0) {
        usage();
        return 0;
    }

    bool isClient = false;
    const char *listenAddr = nullptr;
    const char *listenPort = nullptr;
    const char *connectionAddr = nullptr;
    const char *connectionPort = nullptr;
    unsigned int tickLimit = 15;
    bool noPayload = false;
    while(argc > 0) {
        if(std::strcmp(argv[0], "-c") == 0) {
            isClient = true;
        } else if(std::strcmp(argv[0], "-s") == 0) {
            isClient = false;
        } else if(std::strcmp(argv[0], "-ll") == 0 && argc > 1) {
            --argc; ++argv;
            listenAddr = argv[0];
        } else if(std::strcmp(argv[0], "-lp") == 0 && argc > 1) {
            --argc; ++argv;
            listenPort = argv[0];
        } else if(std::strcmp(argv[0], "-cl") == 0 && argc > 1) {
            --argc; ++argv;
            connectionAddr = argv[0];
        } else if(std::strcmp(argv[0], "-cp") == 0 && argc > 1) {
            --argc; ++argv;
            connectionPort = argv[0];
        } else if(std::strcmp(argv[0], "-t") == 0 && argc > 1) {
            --argc; ++argv;
            tickLimit = std::atoi(argv[0]);
            printf("Set tick limit to %u\n", tickLimit);
        } else if(std::strcmp(argv[0], "-n") == 0) {
            noPayload = true;
            puts("Disabling sending payload");
        } else {
            printf("ERROR: invalid argument \"%s\"\n", argv[0]);
            usage();
            return 1;
        }

        --argc; ++argv;
    }

    if(!listenAddr) {
        puts("ERROR: listenAddr was not specified");
        return 1;
    } else if(!listenPort) {
        puts("ERROR: listenPort was not specified");
        return 1;
    } else if(isClient && !connectionAddr) {
        puts("ERROR: connectionAddr was not specified");
        return 1;
    } else if(isClient && !connectionPort) {
        puts("ERROR: connectionPort was not specified");
        return 1;
    }

    UDPC_ConnectionId listenId;
    UDPC_ConnectionId connectionId;
    if(std::strcmp(listenAddr, "any") == 0) {
        listenId = UDPC_create_id_anyaddr(std::atoi(listenPort));
    } else if(std::regex_match(listenAddr, ipv6_regex_linkonly)) {
        uint32_t scope_id;
        auto addr = UDPC_strtoa_link(listenAddr, &scope_id);
        listenId = UDPC_create_id_full(addr, scope_id, std::atoi(listenPort));
    } else {
        listenId = UDPC_create_id(UDPC_strtoa(listenAddr), std::atoi(listenPort));
    }
    if(isClient) {
        if(std::regex_match(connectionAddr, ipv6_regex_linkonly)) {
            uint32_t scope_id;
            auto addr = UDPC_strtoa_link(connectionAddr, &scope_id);
            connectionId = UDPC_create_id_full(addr, scope_id, std::atoi(connectionPort));
        } else {
            connectionId = UDPC_create_id(UDPC_strtoa(connectionAddr), std::atoi(connectionPort));
        }
    }
    auto context = UDPC_init_threaded_update(listenId, isClient ? 1 : 0);
    if(!context) {
        puts("ERROR: context is NULL");
        return 1;
    }
    UDPC_set_logging_type(context, UDPC_LoggingType::UDPC_INFO);
    unsigned int tick = 0;
    unsigned int temp = 0;
    unsigned int temp2, temp3;
    UDPC_ConnectionId *list = nullptr;
    std::vector<unsigned int> sendIds;
    UDPC_PacketInfo received;
    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if(isClient && UDPC_has_connection(context, connectionId) == 0) {
            UDPC_client_initiate_connection(context, connectionId);
        }
        if(!noPayload) {
            list = UDPC_get_list_connected(context, &temp);
            if(list) {
                if(sendIds.size() < temp) {
                    sendIds.resize(temp, 0);
                } else if(sendIds.size() > temp) {
                    sendIds.resize(temp);
                }
                temp2 = UDPC_get_queue_send_available(context);
                for(unsigned int i = 0; i < temp2; ++i) {
                    temp3 = htonl(sendIds[i % temp]++);
                    UDPC_queue_send(context, list[i % temp], 0, &temp3, sizeof(unsigned int));
                }
                UDPC_free_list_connected(list);
            }
            do {
                received = UDPC_get_received(context, &temp);
                if(received.dataSize == sizeof(unsigned int)) {
                    if((received.flags & 0x8) != 0) {
                        temp2 = ntohl(*((unsigned int*)received.data));
                        printf("Got out of order, data = %u\n", temp2);
                    }
                }
            } while (temp > 0);
        }
        if(tick++ > tickLimit) {
            break;
        }
    }
    UDPC_destroy(context);

    return 0;
}
