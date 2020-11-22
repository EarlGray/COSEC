#include <stdlib.h>
#include <stdio.h>

int main() {
    printf("USER=%s\n", getenv("USER"));
    printf("HOME=%s\n", getenv("HOME"));
    printf("PATH=%s\n", getenv("PATH"));
}
