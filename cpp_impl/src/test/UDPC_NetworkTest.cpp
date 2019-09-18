#include <cstring>
#include <string>
#include <cstdio>
#include <thread>
#include <chrono>

#include <UDPConnection.h>

void usage() {
    puts("[-c | -s] - client or server (default server)");
    puts("-ll <addr> - listen addr");
    puts("-lp <port> - listen port");
    puts("-cl <addr> - connection addr (client only)");
    puts("-cp <port> - connection port (client only)");
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
    } else {
        listenId = UDPC_create_id(UDPC_strtoa(listenAddr), std::atoi(listenPort));
    }
    if(isClient) {
        connectionId = UDPC_create_id(UDPC_strtoa(connectionAddr), std::atoi(connectionPort));
    }
    auto context = UDPC_init_threaded_update(listenId, isClient ? 1 : 0);
    if(!context) {
        puts("ERROR: context is NULL");
        return 1;
    }
    UDPC_set_logging_type(context, UDPC_LoggingType::INFO);
    unsigned int tick = 0;
    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if(isClient && UDPC_has_connection(context, connectionId) == 0) {
            UDPC_client_initiate_connection(context, connectionId);
        }
        if(tick++ > tickLimit) {
            break;
        }
    }
    UDPC_destroy(context);

    return 0;
}
