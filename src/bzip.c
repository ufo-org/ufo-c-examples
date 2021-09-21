#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "ufo_c/target/ufo_c.h"

#include <bzlib.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    FILE*   f;
    BZFILE* b;
    int     nBuf;
    char    buf[ BUFFER_SIZE ];
    int     bzerror;
    // int     nWritten;

    f = fopen ( "test/test.txt.bz2", "r" );
    if (!f) {
        fprintf(stderr, "ERROR 1");
        exit(1);
    }
    b = BZ2_bzReadOpen ( &bzerror, f, 0, 0, NULL, 0 );
    if (bzerror != BZ_OK) {
    BZ2_bzReadClose ( &bzerror, b );
        fprintf(stderr, "ERROR 2");
        exit(1);
    }

    bzerror = BZ_OK;
    int i = 0;
    while (bzerror == BZ_OK) {
        nBuf = BZ2_bzRead ( &bzerror, b, buf, BUFFER_SIZE );
        if (bzerror == BZ_OK) {
            printf("%d: %d %s\n", i, nBuf, buf);
            /* do something with buf[0 .. nBuf-1] */
        }
        i++;
    }
    if (bzerror != BZ_STREAM_END) {
        BZ2_bzReadClose ( &bzerror, b );
        fprintf(stderr, "ERROR 3");
        exit(1);
    } else {
        BZ2_bzReadClose ( &bzerror, b );
    }
}