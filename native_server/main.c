#include <stdio.h>

#include "../src/server/server.h"
#include "../src/utils/logger.h"

int main(int argc, char* argv[]) {
    Server* server = server_new();

    ALOGD("Starting main loop");

    GMainLoop* main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(main_loop);

    ALOGD("Exited main loop, cleaning up");
    g_main_loop_unref(main_loop);
}
