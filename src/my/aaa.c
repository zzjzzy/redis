//
// Created by root on 12/19/25.
//
#include <stdio.h>
#include <sys/time.h>

int main() {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    printf("%ld\n", tv.tv_sec);
    printf("%ld\n", tv.tv_usec);
    return 0;
}
