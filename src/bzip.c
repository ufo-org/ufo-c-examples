#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "ufo_c/target/ufo_c.h"

#include <bzlib.h>

#define BUFFER_SIZE 1024

// This is the maximum number of blocks that we expect in a file. We cannot
// process more than this number. 50K blocks represents 40GB+ of uncompressed
// data.
#define MAX_BLOCKS 50000

// These are the header and endmark values expected to demarkate blocks.
#define SENIOR_BLOCK_HEADER  0x00003141UL
#define JUNIOR_BLOCK_HEADER  0x59265359UL
#define SENIOR_BLOCK_ENDMARK 0x00001772UL
#define JUNIOR_BLOCK_ENDMARK 0x45385090UL

typedef struct {
    FILE*       handle;
    uint32_t    buffer;
    uint32_t    buffLive; // TODO rename
    char        mode;     // TODO remove
    const char *path;
    uint64_t    read_bits;
} BitStream;

BitStream *BitStream_new(const char *input_file_path) {
    // Open the input bzip2 file.
    FILE* input_file = fopen(input_file_path, "rb");
    if (input_file == NULL) {
        fprintf(stderr, "ERROR: cannot read file at `%s'\n", input_file_path);  
        return NULL;
    }

    // Open bit stream for reading.
    BitStream *input_stream = (BitStream *) malloc(sizeof(BitStream));
    if (input_stream == NULL) {
        fprintf(stderr, "ERROR: cannot allocate BitStream object\n");  
        return NULL;
    }

    input_stream->handle = input_file;
    input_stream->buffer = 0;
    input_stream->buffLive = 0;
    input_stream->mode = 'r';
    input_stream->path = input_file_path; // FIXME 0-terminate
    input_stream->read_bits = 0;

    return input_stream;
}

// Returns 0 or 1, or <0 to indicate EOF or error
int BitStream_read_bit(BitStream *input_stream) {
    // If there is a bit in the buffer, return that.
    if (input_stream->buffLive > 0) {
        input_stream->buffLive--;
        return ((input_stream->buffer) >> (input_stream->buffLive)) & 0x1;
    }

    // Read a byte from the input file.
    int byte = getc(input_stream->handle);

    // Detect error
    if (byte == EOF && errno != 0) {
        fprintf(stderr, "ERROR: cannot read file at `%s'\n", 
                input_stream->path);         
        return -2;
    }

    // Detect end of file
    if (byte == EOF) {
        return -1;
    }

    // Take the first bit form the read byte and return that. Stick the rest
    // into the buffer.
    input_stream->buffLive = 7;
    input_stream->buffer = byte;
    input_stream->read_bits++;
    return ((input_stream->buffer) >> 7) & 0x1;
}

void BitStream_free(BitStream *input_stream) {   
    if (fclose(input_stream->handle) == EOF) {
        fprintf(stderr, "WARNING: failed to close file at %s\n", 
                input_stream->path); 
    }
    free(input_stream);
}

int discover_block_boundaries(const char *input_file_path) {

    // Open file for reading.
    BitStream *input_stream = BitStream_new(input_file_path);
    if (input_stream == NULL) {
        fprintf(stderr, "ERROR: cannot open file %s\n", input_file_path);  
        return 1;
    }


//    FILE*       inFile;
//    FILE*       outFile;
//    BitStream*  bsIn, *bsWr;
//    Int32       b, wrBlock, currBlock, rbCtr;
//    MaybeUInt64 bitsRead;

//    UInt32      buffHi, buffLo, blockCRC;
//    Char*       p;

//    strncpy ( progName, argv[0], BZ_MAX_FILENAME-1);
//    progName[BZ_MAX_FILENAME-1]='\0';
//    inFileName[0] = outFileName[0] = 0;

    size_t blocks = 0;
    size_t read_blocks = 0;

    uint64_t block_start_index[MAX_BLOCKS]; // TODO make into a vector that can expand.
    uint64_t block_end_index[MAX_BLOCKS];
    uint64_t read_block_start_index[MAX_BLOCKS];
    uint64_t read_block_end_index[MAX_BLOCKS];

    // Shifting buffer consisting of two parts: senior (most significant) and
    // junior (least significant).
    uint32_t senior_buffer = 0;
    uint32_t junior_buffer = 0;
    
    while (true) {
        int bit = BitStream_read_bit(input_stream);

        // In case of errors or encountering end of file. We could try to
        // recover, but we don't do that for this example.
        if (bit < 0) {
            // FIXME
            //     if (bitsRead >= bStart[currBlock] &&
            //     (bitsRead - bStart[currBlock]) >= 40) {
            //     bEnd[currBlock] = bitsRead-1;
            //     if (currBlock > 0)
            //        fprintf ( stderr, "   block %d runs from " MaybeUInt64_FMT 
            //                          " to " MaybeUInt64_FMT " (incomplete)\n",
            //                  currBlock,  bStart[currBlock], bEnd[currBlock] );
            //  } else
            //     currBlock--;
            //  break;
        }

        // Drop bit into shift register-like buffer
        senior_buffer = (senior_buffer << 1) | (junior_buffer >> 31);
        junior_buffer = (junior_buffer << 1) | (bit & 1);

        // Detect block header or block end mark
        if (((senior_buffer & 0x0000ffff) == SENIOR_BLOCK_HEADER && junior_buffer == JUNIOR_BLOCK_HEADER)
            || ((senior_buffer & 0x0000ffff) == SENIOR_BLOCK_ENDMARK && junior_buffer == JUNIOR_BLOCK_ENDMARK)) {

            // If we found a bounary, the end of the preceding block is 49 bytes
            // ago (or zero if it's the first block)
            block_end_index[blocks] = (input_stream->read_bits > 49) ? input_stream->read_bits - 49 : 0;

            // We finished reading a whole block
            if (blocks > 0 && (block_end_index[blocks] - block_start_index[blocks]) >= 130) {
                fprintf(stderr, "DEBUG: block %li runs from %li to %li\n",
                        read_blocks + 1, block_start_index[blocks], block_end_index[blocks]);

                // Store the offsets
                read_block_start_index[read_blocks] = block_start_index[blocks];
                read_block_end_index[read_blocks] = block_end_index[blocks];

                // Increment number of complete blocks so far
                read_blocks++;
            }

            // Too many blocks
            if (blocks >= MAX_BLOCKS) {
                fprintf(stderr, "ERROR: analyzer can handle up to %i blocks, "
                                "but more blocks were found in file %s\n",
                                MAX_BLOCKS, input_stream->path);
                return 1;
            }

            // Increment number of blocks encountered so far and record the
            // start offset of next encountered block
            blocks++;
            block_start_index[blocks] = input_stream->read_bits;
        }

    }

    // Clean up and exit
    BitStream_free(input_stream);
    return 0;
}

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