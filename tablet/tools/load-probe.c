#include <dlfcn.h>
#include <stdio.h>
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    void *handle = dlopen(argv[1], RTLD_NOW | RTLD_GLOBAL);
    if (!handle) { fprintf(stderr, "Library load failed: %s\n", dlerror()); return 1; }
    char (*check)(void) = dlsym(handle, "_xovi_shouldLoad");
    printf("Library loaded; compatibility check: %s\n", !check ? "not provided" : check() ? "passed" : "refused");
    return 0;
}
