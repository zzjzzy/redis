//
// Created by root on 12/19/25.
//
#include "dict.h"
#include <stdio.h>
#include "siphash.c"

uint64_t siphash(const uint8_t *in, const size_t inlen, const uint8_t *k);
int main() {
    dict *d = malloc(sizeof(dict));
    printf("d is %d\n", d->rehashidx);

    uint8_t i = 2;
    uint64_t hash = siphash((uint8_t *)"abc", 3, &i);
    printf("hash is %lu\n", hash);
    return 0;
}
