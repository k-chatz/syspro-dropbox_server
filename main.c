#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void wrongOptionValue(char *opt, char *val) {
    fprintf(stderr, "\nWrong value [%s] for option '%s'\n", val, opt);
    exit(EXIT_FAILURE);
}

/**
 * Read options from command line*/
void readOptions(int argc, char **argv, int *portNum) {
    int i;
    char *opt, *optVal;
    for (i = 1; i < argc; ++i) {
        opt = argv[i];
        optVal = argv[i + 1];
        if (strcmp(opt, "-p") == 0) {
            if (optVal != NULL && optVal[0] != '-') {
                *portNum = (int) strtol(optVal, NULL, 10);
            } else {
                wrongOptionValue(opt, optVal);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int portNum = 0;

    /* Read argument options from command line*/
    readOptions(argc, argv, &portNum);

    printf("portNum: %d\n", portNum);

    return 0;
}