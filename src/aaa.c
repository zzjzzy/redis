//
// Created by root on 12/19/25.
//
#include "dict.h"
#include <stdio.h>
#include "siphash.c"
#include "server.h"

int main() {
    dict * d = dictCreate(&sdsHashDictType);
    dictAdd(d, "aaa", "bbb");
    dictEntry *de = dictFind(d, "aaa");
    printf("de:%s", dictGetVal(de));
    return 0;
}
