
#include "env.h"

#include <stdlib.h>

char **MergeEnvs (char **a, char **b) {
    size_t total_size = 0;
    char **p = a;
    while (*p) {
        total_size++;
        p++;
    }

    p = b;
    while (*p) {
        total_size++;
        p++;
    }

    char **res = (char **) calloc(total_size + 1, sizeof(char *));
    size_t i = 0;

    p = a;
    while (*p) {
        res[i++] = *p;
        p++;
    }

    p = b;
    while (*p) {
        res[i++] = *p;
        p++;
    }

    res[i] = NULL;

    return res;
}
