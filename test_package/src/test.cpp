#include "UDPC.h"

int main() {
    auto ctx = UDPC_init(UDPC_create_id_easy("127.0.0.1", 0), 1, 1);
    if (UDPC_is_valid_context(ctx) == 0) {
        return 1;
    }
    UDPC_destroy(ctx);
    return 0;
}
