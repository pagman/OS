
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "defines.h"


int checkPositive(int value, char * msg) {
    if (value < 0) {
        printf("%s", msg);
        return 1;
    }

    return 0;
}

int checkNotNull(char * value, char * msg) {
    if (value == NULL) {
        printf("%s", msg);
        return 1;
    }

    return 0;
}

int checkFile(char * X) {
    int L = 0;

    FILE * fp = fopen(X, "rt");
    if (fp == NULL) {
        printf("file X not found: X= %s \n", X);
        perror("fopen");
        exit(1);
    }

    char ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == '\n') {
            L++;
        }
    }

    fclose(fp);

    if (L < MINIMUM_LINES) {
        printf("Number of lines is less than %d \n Please use a file with more lines \n", MINIMUM_LINES);
        exit(1);
    }

    return L;
}

