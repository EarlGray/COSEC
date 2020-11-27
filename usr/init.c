#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define __DEBUG
#include <cosec/log.h>

int main(int argc, char *argv[]) {
    logmsgdf("hello world\n");
    const char *msg = "[init] hello, userspace\n\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    return EXIT_SUCCESS;
}
