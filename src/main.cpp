#include <cstdio>

#include "app.h"

int main(int, char**) {
    App app;
    if (!app.Init()) {
        std::fprintf(stderr, "CoreView Task Manager: failed to initialize window/GL context.\n");
        return 1;
    }
    app.Run();
    app.Shutdown();
    return 0;
}
