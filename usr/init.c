#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    const char *msg = "[init] hello, userspace\n\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    return EXIT_SUCCESS;
}
