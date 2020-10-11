#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    const char *msg = "Hello world!\n";
    write(STDIN_FILENO, msg, strlen(msg));
}
