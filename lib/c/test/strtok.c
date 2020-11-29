#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int test1() {
    char input[30];
    sprintf(input, "Hello, world!");
    const char *token;

    token = strtok(input, ", ");
    if (strcmp(token, "Hello")) {
        fprintf(stderr, "%s: want '%s', got '%s'\n", __func__, "Hello", token);
        return false;
    }

    token = strtok(NULL, ", ");
    if (strcmp(token, "world!")) {
        fprintf(stderr, "%s: want '%s', got '%s'\n", __func__, "world!", token);
        return false;
    }

    token = strtok(NULL, ", ");
    if (token != NULL) {
        fprintf(stderr, "%s: want NULL, got '%s'\n", __func__, token);
        return false;
    }

    return true;
}

int main() {
    int failed = 0;
    if (!test1()) ++failed;

    return failed;
}
