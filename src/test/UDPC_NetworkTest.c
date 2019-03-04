#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <threads.h>

#include <UDPConnection.h>

void printUsage()
{
    printf("Usage: [-c] -a <addr> -p <target_port> -l <listen_port>\n");
}

int main(int argc, char** argv)
{
    int isClient = 0;
    uint32_t targetAddress = 0;
    uint16_t targetPort = 0;
    uint16_t listenPort = 0;

    --argc; ++argv;
    while(argc > 0)
    {
        if(strcmp("-c", argv[0]) == 0)
        {
            isClient = 1;
        }
        else if(strcmp("-a", argv[0]) == 0 && argc > 1)
        {
            targetAddress = UDPC_strtoa(argv[1]);
            --argc; ++argv;
        }
        else if(strcmp("-p", argv[0]) == 0 && argc > 1)
        {
            targetPort = strtoul(argv[1], NULL, 10);
            --argc; ++argv;
        }
        else if(strcmp("-l", argv[0]) == 0 && argc > 1)
        {
            listenPort = strtoul(argv[1], NULL, 10);
            --argc; ++argv;
        }
        else if(strcmp("-h", argv[0]) == 0 || strcmp("--help", argv[0]) == 0)
        {
            printUsage();
            return 0;
        }
        --argc; ++argv;
    }

    UDPC_Context *ctx = UDPC_init(listenPort, isClient);
    if(UDPC_get_error(ctx) == UDPC_SUCCESS)
    {
        UDPC_set_logging_type(ctx, 4);
        while(UDPC_get_error(ctx) == UDPC_SUCCESS)
        {
            if(isClient)
            {
                UDPC_client_initiate_connection(ctx, targetAddress, targetPort);
            }
            UDPC_update(ctx);
            UDPC_check_events(ctx);
            thrd_sleep(&(struct timespec){0, 16666666}, NULL);
        }
    }
    UDPC_destroy(ctx);

    return 0;
}
