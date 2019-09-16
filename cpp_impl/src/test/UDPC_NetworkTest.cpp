#include <cstring>
#include <string>
#include <cstdio>

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
        } else {
            printf("ERROR: invalid argument \"%s\"\n", argv[0]);
            usage();
            return 1;
        }

        --argc; ++argv;
    }

    return 0;
}
