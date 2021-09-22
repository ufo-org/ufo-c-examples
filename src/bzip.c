#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "ufo_c/target/ufo_c.h"

#include <bzlib.h>

// The bzip code was adapted from bzip2recovery.

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
    // strncpy ( progName, argv[0], BZ_MAX_FILENAME-1);
    // progName[BZ_MAX_FILENAME-1]='\0';
    // inFileName[0] = outFileName[0] = 0;

    // Open bit stream for reading
    BitStream *input_stream = (BitStream *) malloc(sizeof(BitStream));
    if (input_stream == NULL) {
        fprintf(stderr, "ERROR: cannot allocate BitStream object\n");  
        return NULL;
    }

    input_stream->path = input_file_path; // FIXME 0-terminate
    input_stream->buffer = 0;
    input_stream->buffLive = 0;
    input_stream->mode = 'r';
    input_stream->read_bits = 0;

    // Open the input bzip2 file
    FILE* input_file = fopen(input_stream->path, "rb");
    if (input_file == NULL) {
        fprintf(stderr, "ERROR: cannot read file at %s\n", input_file_path);  
        return NULL;
    }

    input_stream->handle = input_file;    

    printf("xxx %p --- %p\n", input_stream, input_file);

    return input_stream;
}

// Returns 0 or 1, or <0 to indicate EOF or error
int BitStream_read_bit(BitStream *input_stream) {
    // always score a hit
    input_stream->read_bits++;

    // If there is a bit in the buffer, return that.
    if (input_stream->buffLive > 0) {
        input_stream->buffLive--;
        return (((input_stream->buffer) >> (input_stream->buffLive)) & 0x1);
    }

    // Read a byte from the input file
    int byte = getc(input_stream->handle);

    // Detect end of file or error
    if (byte == EOF) {
        // Detect error
        if (ferror(input_stream->handle)) {
            perror("ERROR");
            fprintf(stderr, "ERROR: cannot read bit from file at %s\n", 
                    input_stream->path);         
            return -2;
        }

        // Detect end of file
        return -1;
    }

    // Take the first bit form the read byte and return that. Stick the rest
    // into the buffer.
    input_stream->buffLive = 7;
    input_stream->buffer = byte;
    return (((input_stream->buffer) >> 7) & 0x1);
}

void BitStream_free(BitStream *input_stream) {   
    if (fclose(input_stream->handle) == EOF) {
        fprintf(stderr, "WARNING: failed to close file at %s\n", 
                input_stream->path); 
    }
    free(input_stream);
}

typedef struct {
    size_t blocks;
    uint64_t block_start_index[MAX_BLOCKS]; // TODO make into a vector that can expand.
    uint64_t block_end_index[MAX_BLOCKS];

    size_t read_blocks;
    uint64_t read_block_start_index[MAX_BLOCKS];
    uint64_t read_block_end_index[MAX_BLOCKS];
    
    size_t bad_blocks;
} Blocks;

Blocks *Blocks_parse(const char *input_file_path) {

    // Open file for reading.
    BitStream *input_stream = BitStream_new(input_file_path);
    if (input_stream == NULL) {
        fprintf(stderr, "ERROR: cannot open file %s\n", input_file_path);  
        return NULL;
    }

    // Initialize the counters.
    Blocks *boundaries = (Blocks *) malloc(sizeof(Blocks));
    if (NULL == boundaries) {
        fprintf(stderr, "ERROR: cannot allocate a struct for recording block boundaries\n");  
        return NULL;
    }

    boundaries->blocks = 0;
    boundaries->read_blocks = 0;
    boundaries->bad_blocks = 0;

    // Shifting buffer consisting of two parts: senior (most significant) and
    // junior (least significant).
    uint32_t senior_buffer = 0;
    uint32_t junior_buffer = 0;
    
    while (true) {
        int bit = BitStream_read_bit(input_stream);

        // In case of errors or encountering end of file. We recover, although
        // we probably shouldn't. I don't want to mess with the function too
        // much.
        if (bit < 0) {
            // If we're inside a block (not at the beginning or within an end
            // marker) we report an error and stop processing the block.
            if (input_stream->read_bits >= boundaries->block_start_index[boundaries->blocks] && 
                (input_stream->read_bits - boundaries->block_start_index[boundaries->blocks]) >= 40) {
                boundaries->block_end_index[boundaries->blocks] = input_stream->read_bits - 1;
                if (boundaries->blocks > 0) {
                    fprintf(stderr, "DEBUG: block %li runs from %li to %li (incomplete)\n",
                            boundaries->blocks, 
                            boundaries->block_start_index[boundaries->blocks], 
                            boundaries->block_end_index[boundaries->blocks]);

                    boundaries->bad_blocks++;
                }               
            } else {
                // Otherwise, I guess this is just EOF?
                boundaries->blocks--;
            }

            // Proceed to next block.
            break;
        }

        // Drop bit into shift register-like buffer
        senior_buffer = (senior_buffer << 1) | (junior_buffer >> 31);
        junior_buffer = (junior_buffer << 1) | (bit & 1);     

        //printf("%lx %lx == %lx %lx\n", senior_buffer, junior_buffer, SENIOR_BLOCK_HEADER, JUNIOR_BLOCK_HEADER);   
        //printf("%lx %lx == %lx %lx\n\n", senior_buffer, junior_buffer, SENIOR_BLOCK_ENDMARK, JUNIOR_BLOCK_ENDMARK);

        // Detect block header or block end mark
        if (((senior_buffer & 0x0000ffff) == SENIOR_BLOCK_HEADER && junior_buffer == JUNIOR_BLOCK_HEADER) ||
            ((senior_buffer & 0x0000ffff) == SENIOR_BLOCK_ENDMARK && junior_buffer == JUNIOR_BLOCK_ENDMARK)) {

            // If we found a bounary, the end of the preceding block is 49 bytes
            // ago (or zero if it's the first block)
            boundaries->block_end_index[boundaries->blocks] = 
                (input_stream->read_bits > 49) ? input_stream->read_bits - 49 : 0;

            // We finished reading a whole block
            if (boundaries->blocks > 0 
                && (boundaries->block_end_index[boundaries->blocks] 
                - boundaries->block_start_index[boundaries->blocks]) >= 130) {

                fprintf(stderr, "DEBUG: block %li runs from %li to %li\n",
                        boundaries->read_blocks + 1, 
                        boundaries->block_start_index[boundaries->blocks], 
                        boundaries->block_end_index[boundaries->blocks]);

                // Store the offsets
                boundaries->read_block_start_index[boundaries->read_blocks] = 
                    boundaries->block_start_index[boundaries->blocks];
                boundaries->read_block_end_index[boundaries->read_blocks] = 
                    boundaries->block_end_index[boundaries->blocks];

                // Increment number of complete blocks so far
                boundaries->read_blocks++;

                //boundaries->block_start_index[boundaries->blocks] = input_stream->read_bits;
            }

            // Too many blocks
            if (boundaries->blocks >= MAX_BLOCKS) {
                fprintf(stderr, "ERROR: analyzer can handle up to %i blocks, "
                                "but more blocks were found in file %s\n",
                                MAX_BLOCKS, input_stream->path);
                free(boundaries);
                return NULL;
            }

            // Increment number of blocks encountered so far and record the
            // start offset of next encountered block
            boundaries->blocks++;
            boundaries->block_start_index[boundaries->blocks] = input_stream->read_bits;
        }
    }

    // Clean up and exit
    BitStream_free(input_stream);
    return boundaries;
}

void Blocks_read_block(Blocks *boundaries, size_t i) {
    bz_stream *stream = (bz_stream *) malloc(sizeof(bz_stream));
    stream->bzalloc = NULL;
    stream->bzfree = NULL;
    stream->opaque = NULL;

    int result = BZ2_bzDecompressInit(stream, /*verbosity*/ 0, /*small*/ 0);

    printf("result = %i\n", result);
    printf("stream->next_in  = avail: %i, total: %i %i\n", stream->avail_in, stream->total_in_lo32, stream->total_in_hi32);
    printf("stream->next_out = avail: %i, total: %i %i\n", stream->avail_out, stream->total_out_lo32, stream->total_out_hi32);

    BZ2_bzDecompress(stream);

    printf("result = %i\n", result);
    printf("stream->next_in  = avail: %i, total: %i %i\n", stream->avail_in, stream->total_in_lo32, stream->total_in_hi32);
    printf("stream->next_out = avail: %i, total: %i %i\n", stream->avail_out, stream->total_out_lo32, stream->total_out_hi32);

}

int main(int argc, char *argv[]) {
    Blocks *blocks = Blocks_parse("test/test.txt.bz2");
    Blocks_read_block(blocks, 0);



    // https://github.com/waigx/elinks/blob/2fc9b0bf5a2e5a1f7a5c7f4ee210e3feedd6db58/src/encoding/bzip2.c
//     typedef 
//    struct {
//       char *next_in;
//       unsigned int avail_in;
//       unsigned int total_in;

//       char *next_out;
//       unsigned int avail_out;
//       unsigned int total_out;

//       void *state;

//       void *(*bzalloc)(void *,int,int);
//       void (*bzfree)(void *,void *);
//       void *opaque;
//    } 
//    bz_stream;

    // FILE*   f;
    // BZFILE* b;
    // int     nBuf;
    // char    buf[ BUFFER_SIZE ];
    // int     bzerror;
    // int     nWritten;

    // f = fopen ( "test/test.txt.bz2", "r" );
    // if (!f) {
    //     fprintf(stderr, "ERROR 1");
    //     exit(1);
    // }
    // b = BZ2_bzReadOpen ( &bzerror, f, 0, 0, NULL, 0 );
    // if (bzerror != BZ_OK) {
    // BZ2_bzReadClose ( &bzerror, b );
    //     fprintf(stderr, "ERROR 2");
    //     exit(1);
    // }

    // bzerror = BZ_OK;
    // int i = 0;
    // while (bzerror == BZ_OK) {
    //     nBuf = BZ2_bzRead ( &bzerror, b, buf, BUFFER_SIZE );
    //     if (bzerror == BZ_OK) {
    //         printf("%d: %d %s\n", i, nBuf, buf);
    //         /* do something with buf[0 .. nBuf-1] */
    //     }
    //     i++;
    // }
    // if (bzerror != BZ_STREAM_END) {
    //     BZ2_bzReadClose ( &bzerror, b );
    //     fprintf(stderr, "ERROR 3");
    //     exit(1);
    // } else {
    //     BZ2_bzReadClose ( &bzerror, b );
    // }
}