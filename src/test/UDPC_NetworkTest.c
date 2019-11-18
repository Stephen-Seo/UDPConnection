#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <threads.h>

#include <UDPConnection.h>

#define QUEUED_MAX_SIZE 32
#define SEND_IDS_SIZE 64

void usage() {
    puts("[-c | -s] - client or server (default server)");
    puts("-ll <addr> - listen addr");
    puts("-lp <port> - listen port");
    puts("-cl <addr> - connection addr (client only)");
    puts("-cp <port> - connection port (client only)");
    puts("-t <tick_count>");
    puts("-n - do not add payload to packets");
    puts("-l (silent|error|warning|info|verbose|debug) - log level, default debug");
    puts("-e - enable receiving events");
    puts("-ls - enable libsodium");
}

void sleep_seconds(unsigned int seconds) {
    struct timespec duration;
    duration.tv_sec = seconds;
    duration.tv_nsec = 0;
    thrd_sleep(&duration, NULL);
}

int main(int argc, char **argv) {
    --argc; ++argv;
    if(argc == 0) {
        usage();
        return 0;
    }

    int isClient = 0;
    const char *listenAddr = NULL;
    const char *listenPort = NULL;
    const char *connectionAddr = NULL;
    const char *connectionPort = NULL;
    unsigned int tickLimit = 15;
    int noPayload = 0;
    UDPC_LoggingType logLevel = UDPC_DEBUG;
    int isReceivingEvents = 0;
    int isLibSodiumEnabled = 0;
    while(argc > 0) {
        if(strcmp(argv[0], "-c") == 0) {
            isClient = 1;
        } else if(strcmp(argv[0], "-s") == 0) {
            isClient = 0;
        } else if(strcmp(argv[0], "-ll") == 0 && argc > 1) {
            --argc; ++argv;
            listenAddr = argv[0];
        } else if(strcmp(argv[0], "-lp") == 0 && argc > 1) {
            --argc; ++argv;
            listenPort = argv[0];
        } else if(strcmp(argv[0], "-cl") == 0 && argc > 1) {
            --argc; ++argv;
            connectionAddr = argv[0];
        } else if(strcmp(argv[0], "-cp") == 0 && argc > 1) {
            --argc; ++argv;
            connectionPort = argv[0];
        } else if(strcmp(argv[0], "-t") == 0 && argc > 1) {
            --argc; ++argv;
            tickLimit = atoi(argv[0]);
            printf("Set tick limit to %u\n", tickLimit);
        } else if(strcmp(argv[0], "-n") == 0) {
            noPayload = 1;
            puts("Disabling sending payload");
        } else if(strcmp(argv[0], "-l") == 0) {
            --argc; ++argv;
            if(strcmp(argv[0], "silent") == 0) {
                logLevel = UDPC_SILENT;
            } else if(strcmp(argv[0], "error") == 0) {
                logLevel = UDPC_ERROR;
            } else if(strcmp(argv[0], "warning") == 0) {
                logLevel = UDPC_WARNING;
            } else if(strcmp(argv[0], "info") == 0) {
                logLevel = UDPC_INFO;
            } else if(strcmp(argv[0], "verbose") == 0) {
                logLevel = UDPC_VERBOSE;
            } else if(strcmp(argv[0], "debug") == 0) {
                logLevel = UDPC_DEBUG;
            } else {
                printf("ERROR: invalid argument \"%s\", expected "
                    "silent|error|warning|info|verbose|debug", argv[0]);
                usage();
                return 1;
            }
        } else if(strcmp(argv[0], "-e") == 0) {
            isReceivingEvents = 1;
            puts("Enabled isReceivingEvents");
        } else if(strcmp(argv[0], "-ls") == 0) {
            isLibSodiumEnabled = 1;
            puts("Enabled libsodium");
        } else {
            printf("ERROR: invalid argument \"%s\"\n", argv[0]);
            usage();
            return 1;
        }

        --argc; ++argv;
    }

    if(isLibSodiumEnabled == 0) {
        puts("Disabled libsodium");
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
    if(strcmp(listenAddr, "any") == 0) {
        listenId = UDPC_create_id_anyaddr(atoi(listenPort));
    } else {
        listenId = UDPC_create_id_easy(listenAddr, atoi(listenPort));
    }
    if(isClient) {
        connectionId = UDPC_create_id_easy(connectionAddr, atoi(connectionPort));
    }
    UDPC_HContext context = UDPC_init_threaded_update(listenId, isClient, isLibSodiumEnabled);
    if(!context) {
        puts("ERROR: context is NULL");
        return 1;
    }
    UDPC_set_logging_type(context, logLevel);
    UDPC_set_receiving_events(context, isReceivingEvents);
    unsigned int tick = 0;
    unsigned int temp = 0;
    unsigned int temp2, temp3;
    unsigned long size;
    UDPC_ConnectionId *list = NULL;
    unsigned int sendIds[SEND_IDS_SIZE];
    unsigned int sendIdsSize = 0;
    UDPC_PacketInfo received;
    UDPC_Event event;
    while(1) {
        sleep_seconds(1);
        if(isClient && UDPC_has_connection(context, connectionId) == 0) {
            UDPC_client_initiate_connection(context, connectionId, isLibSodiumEnabled);
        }
        if(!noPayload) {
            list = UDPC_get_list_connected(context, &temp);
            if(list) {
                if(sendIdsSize < temp) {
                    while(sendIdsSize < temp) {
                        if(sendIdsSize == SEND_IDS_SIZE) {
                            temp = SEND_IDS_SIZE;
                            break;
                        }
                        sendIds[sendIdsSize++] = 0;
                    }
                } else if(sendIdsSize > temp) {
                    sendIdsSize = temp;
                }
                size = UDPC_get_queue_send_current_size(context);
                temp2 = size < QUEUED_MAX_SIZE ? QUEUED_MAX_SIZE - size : 0;
                for(unsigned int i = 0; i < temp2; ++i) {
                    temp3 = htonl(sendIds[i % temp]++);
                    UDPC_queue_send(context, list[i % temp], 0, &temp3, sizeof(unsigned int));
                }
                UDPC_free_list_connected(list);
            }
            do {
                received = UDPC_get_received(context, &size);
                if(received.dataSize == sizeof(unsigned int)) {
                    if((received.flags & 0x8) != 0) {
                        temp2 = ntohl(*((unsigned int*)received.data));
                        printf("Got out of order, data = %u\n", temp2);
                    }
                }
            } while (size > 0);
        }
        do {
            event = UDPC_get_event(context, &size);
            if(event.type == UDPC_ET_NONE) {
                break;
            }
            const char *typeString;
            switch(event.type) {
            case UDPC_ET_CONNECTED:
                typeString = "CONNECTED";
                break;
            case UDPC_ET_DISCONNECTED:
                typeString = "DISCONNECTED";
                break;
            case UDPC_ET_GOOD_MODE:
                typeString = "GOOD_MODE";
                break;
            case UDPC_ET_BAD_MODE:
                typeString = "BAD_MODE";
                break;
            default:
                typeString = "INVALID_TYPE";
                break;
            }
            printf("Got event %s: %s %u\n",
                typeString,
                UDPC_atostr(context, event.conId.addr),
                event.conId.port);
        } while(size > 0);
        if(tick++ > tickLimit) {
            break;
        }
    }
    UDPC_destroy(context);

    return 0;
}
