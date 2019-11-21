#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <threads.h>

#ifdef UDPC_LIBSODIUM_ENABLED
#include <sodium.h>
#endif

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
    puts("-ck <pubkey_file> - connect to server expecting this public key");
    puts("-sk <pubkey> <seckey> - start with pub/sec key pair");
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
    const char *pubkey_file = NULL;
    const char *seckey_file = NULL;
    unsigned char pubkey[crypto_sign_PUBLICKEYBYTES];
    unsigned char seckey[crypto_sign_SECRETKEYBYTES];
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
        } else if(strcmp(argv[0], "-ck") == 0 && argc > 1) {
            --argc; ++argv;
            pubkey_file = argv[0];
        } else if(strcmp(argv[0], "-sk") == 0 && argc > 2) {
            --argc; ++argv;
            pubkey_file = argv[0];
            --argc; ++argv;
            seckey_file = argv[0];
        } else {
            printf("ERROR: invalid argument \"%s\"\n", argv[0]);
            usage();
            return 1;
        }

        --argc; ++argv;
    }

    if(isLibSodiumEnabled == 0) {
        puts("Disabled libsodium");
    } else {
        if(pubkey_file) {
            FILE *pubkey_f = fopen(pubkey_file, "r");
            if(!pubkey_f) {
                printf("ERROR: Failed to open pubkey_file \"%s\"\n", pubkey_file);
                return 1;
            }
            size_t count = fread(pubkey, 1, crypto_sign_PUBLICKEYBYTES, pubkey_f);
            if(count != crypto_sign_PUBLICKEYBYTES) {
                fclose(pubkey_f);
                printf("ERROR: Failed to read pubkey_file \"%s\"\n", pubkey_file);
                return 1;
            }
            fclose(pubkey_f);

            if(seckey_file) {
                FILE *seckey_f = fopen(seckey_file, "r");
                if(!seckey_f) {
                    printf("ERROR: Failed to open seckey_file \"%s\"\n", seckey_file);
                    return 1;
                }
                count = fread(seckey, 1, crypto_sign_SECRETKEYBYTES, seckey_f);
                if(count != crypto_sign_SECRETKEYBYTES) {
                    fclose(seckey_f);
                    printf("ERROR: Failed to read seckey_file \"%s\"\n", seckey_file);
                    return 1;
                }
                fclose(seckey_f);
            }
        } else if(seckey_file) {
            printf("ERROR: Invalid state (seckey_file defined but not pubkey_file)\n");
            return 1;
        }
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
    UDPC_HContext context = UDPC_init(listenId, isClient, isLibSodiumEnabled);
    if(!context) {
        puts("ERROR: context is NULL");
        return 1;
    }

    UDPC_set_logging_type(context, logLevel);
    UDPC_set_receiving_events(context, isReceivingEvents);
    if(!isClient && pubkey_file && seckey_file) {
        UDPC_set_libsodium_keys(context, seckey, pubkey);
        puts("Set pubkey/seckey for server");
    }

    UDPC_enable_threaded_update(context);

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
            if(isLibSodiumEnabled && pubkey_file) {
                UDPC_client_initiate_connection_pk(context, connectionId, pubkey);
            } else {
                UDPC_client_initiate_connection(context, connectionId, isLibSodiumEnabled);
            }
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
