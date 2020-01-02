#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <threads.h>
#include <assert.h>

#ifdef UDPC_LIBSODIUM_ENABLED
#include <sodium.h>
#endif

#include <UDPConnection.h>

#define QUEUED_MAX_SIZE 32
#define SEND_IDS_SIZE 64
#define WHITELIST_FILES_SIZE 64

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
    puts("-ck <pubkey_file> - add pubkey to whitelist");
    puts("-sk <pubkey> <seckey> - start with pub/sec key pair");
    puts("-p <\"fallback\" or \"strict\"> - set auth policy");
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
    const char *whitelist_pk_files[WHITELIST_FILES_SIZE];
    unsigned int whitelist_pk_files_index = 0;
    unsigned char whitelist_pks[WHITELIST_FILES_SIZE][crypto_sign_PUBLICKEYBYTES];
    int authPolicy = UDPC_AUTH_POLICY_FALLBACK;

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
            if(whitelist_pk_files_index >= WHITELIST_FILES_SIZE) {
                puts("ERROR: limit reached for whitelisted pks");
                return 1;
            }
            whitelist_pk_files[whitelist_pk_files_index++] = argv[0];
        } else if(strcmp(argv[0], "-sk") == 0 && argc > 2) {
            --argc; ++argv;
            pubkey_file = argv[0];
            --argc; ++argv;
            seckey_file = argv[0];
        } else if(strcmp(argv[0], "-p") == 0 && argc > 1) {
            if(strcmp(argv[1], "fallback") == 0) {
                authPolicy = UDPC_AUTH_POLICY_FALLBACK;
                --argc; ++argv;
            } else if(strcmp(argv[1], "strict") == 0) {
                authPolicy = UDPC_AUTH_POLICY_STRICT;
                --argc; ++argv;
            } else {
                printf("ERROR: invalid argument \"%s %s\"\n", argv[0], argv[1]);
                usage();
                return 1;
            }
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
        if(pubkey_file && seckey_file) {
            FILE *pubkey_f = fopen(pubkey_file, "rb");
            if(!pubkey_f) {
                printf("ERROR: Failed to open pubkey_file \"%s\"\n", pubkey_file);
                return 1;
            }
            size_t count = fread(pubkey, crypto_sign_PUBLICKEYBYTES, 1, pubkey_f);
            if(count != 1) {
                fclose(pubkey_f);
                printf("ERROR: Failed to read pubkey_file \"%s\"\n", pubkey_file);
                return 1;
            }
            fclose(pubkey_f);

            FILE *seckey_f = fopen(seckey_file, "rb");
            if(!seckey_f) {
                printf("ERROR: Failed to open seckey_file \"%s\"\n", seckey_file);
                return 1;
            }
            count = fread(seckey, crypto_sign_SECRETKEYBYTES, 1, seckey_f);
            if(count != 1) {
                fclose(seckey_f);
                printf("ERROR: Failed to read seckey_file \"%s\"\n", seckey_file);
                return 1;
            }
            fclose(seckey_f);
        } else if(pubkey_file || seckey_file) {
            printf("ERROR: Invalid state (pubkey_file and seckey_file not "
                "defined)\n");
            return 1;
        }
        for(unsigned int i = 0; i < whitelist_pk_files_index; ++i) {
            FILE *pkf = fopen(whitelist_pk_files[i], "rb");
            if(!pkf) {
                printf("ERROR: Failed to open whitelist pubkey file \"%s\"\n",
                    whitelist_pk_files[i]);
                return 1;
            }
            size_t count = fread(whitelist_pks[i], crypto_sign_PUBLICKEYBYTES, 1, pkf);
            if(count != 1) {
                fclose(pkf);
                printf("ERROR: Failed to read whitelist pubkey file \"%s\"\n",
                    whitelist_pk_files[i]);
                return 1;
            }
            fclose(pkf);
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
    if(pubkey_file && seckey_file) {
        UDPC_set_libsodium_keys(context, seckey, pubkey);
        puts("Set pubkey/seckey");
    }

    UDPC_set_auth_policy(context, authPolicy);
    assert(UDPC_get_auth_policy(context) == authPolicy);
    if(authPolicy == UDPC_AUTH_POLICY_FALLBACK) {
        puts("Auth policy set to \"fallback\"");
    } else if(authPolicy == UDPC_AUTH_POLICY_STRICT) {
        puts("Auth policy set to \"strict\"");
    }

    if(isLibSodiumEnabled && whitelist_pk_files_index > 0) {
        puts("Enabling pubkey whitelist...");
        for(unsigned int i = 0; i < whitelist_pk_files_index; ++i) {
            if(UDPC_add_whitelist_pk(context, whitelist_pks[i]) != i + 1) {
                puts("Failed to add pubkey to whitelist");
                return 1;
            }
        }
    }

    UDPC_enable_threaded_update(context);

    unsigned int tick = 0;
    unsigned int temp = 0;
    unsigned int temp2, temp3;
    int temp4;
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

            for(unsigned int i = 0; i < temp; ++i) {
                size = UDPC_get_max_queued_size() - UDPC_get_queued_size(context, list[i], &temp4);
                if(temp4 == 0) {
                    continue;
                }
                for(unsigned int j = 0; j < size; ++j) {
                    temp2 = htonl(sendIds[i]++);
                    UDPC_queue_send(context, list[i], 0, &temp2, sizeof(unsigned int));
                }
            }
            if(list) {
                UDPC_free_list_connected(list);
            }
            do {
                received = UDPC_get_received(context, &size);
//                printf("Received data size = %u\n", received.dataSize);
                if(received.dataSize == sizeof(unsigned int)) {
                    if((received.flags & 0x8) != 0) {
                        temp2 = ntohl(*((unsigned int*)received.data));
                        printf("Got out of order, data = %u\n", temp2);
                    }
//                    printf("Got rtt %u\n", received.rtt);
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

    UDPC_set_accept_new_connections(context, 0);

    puts("Dropping all connections...");
    UDPC_ConnectionId *ids = UDPC_get_list_connected(context, &temp);
    UDPC_ConnectionId *current = ids;
    while(temp > 0) {
        UDPC_drop_connection(context, *current, 0);
        ++current;
        --temp;
    }
    UDPC_free_list_connected(ids);

    puts("Waiting 2 seconds for disconnect packets to be sent...");
    sleep_seconds(2);

    puts("Cleaning up UDPC context...");
    UDPC_destroy(context);

    return 0;
}
