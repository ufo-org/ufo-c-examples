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

// Static elements of headers
const int stream_magic_size = 4;
const int block_magic_size = 6;
const int footer_magic_size = 6;
const unsigned char stream_magic[] = { 0x42, 0x5a, 0x68, 0x30 };
const unsigned char block_magic[]  = { 0x31, 0x41, 0x59, 0x26, 0x53, 0x59 };
const unsigned char footer_magic[] = { 0x17, 0x72, 0x45, 0x38, 0x50, 0x90 };

typedef struct {
    uint32_t senior;
    uint32_t junior;
} ShiftRegister;

const ShiftRegister block_header_template  = { 0x00003141UL, 0x59265359UL };
const ShiftRegister block_endmark_template = { 0x00001772UL, 0x45385090UL };

void ShiftRegister_append(ShiftRegister *shift_register, int bit) {
    shift_register->senior = (shift_register->senior << 1) | (shift_register->junior >> 31);
    shift_register->junior = (shift_register->junior << 1) | (bit & 1);  
}

bool ShiftRegister_equal_with_senior_mask(const ShiftRegister *a, const ShiftRegister *b, uint32_t senior_mask) {
    return (a->junior == b->junior) && ((a->senior & senior_mask) == (b->senior & senior_mask));
}

typedef struct {
    FILE*         handle;
    uint32_t      buffer;
    uint32_t      buffer_live;
    const char   *path;
    uint64_t      read_bits;    
    ShiftRegister shift_register;
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
    input_stream->buffer_live = 0;
    input_stream->read_bits = 0;

    input_stream->shift_register.senior = 0;
    input_stream->shift_register.junior = 0;

    // Open the input bzip2 file
    FILE* input_file = fopen(input_stream->path, "rb");
    if (input_file == NULL) {
        fprintf(stderr, "ERROR: cannot read file at %s\n", input_file_path);  
        return NULL;
    }

    input_stream->handle = input_file;   
    return input_stream;
}

// Returns 0 or 1, or <0 to indicate EOF or error
int BitStream_read_bit(BitStream *input_stream) {
    // always score a hit
    input_stream->read_bits++;

    // If there is a bit in the buffer, return that. Append the hot bit into the
    // shift register buffer.
    if (input_stream->buffer_live > 0) {
        input_stream->buffer_live--;
        int bit = (((input_stream->buffer) >> (input_stream->buffer_live)) & 0x1);
        ShiftRegister_append(&input_stream->shift_register, bit);
        return bit;
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
    // into the buffer. Append the hot bit into the shift register buffer.
    input_stream->buffer_live = 7;
    input_stream->buffer = byte;
    int bit = (((input_stream->buffer) >> 7) & 0x1);
    ShiftRegister_append(&input_stream->shift_register, bit);
    return bit;
}

void BitStream_free(BitStream *input_stream) {   
    if (fclose(input_stream->handle) == EOF) {
        fprintf(stderr, "WARNING: failed to close file at %s\n", 
                input_stream->path); 
    }
    free(input_stream);
}

typedef struct {
    const char *path;

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

    boundaries->path = input_file_path;
    boundaries->blocks = 0;
    boundaries->read_blocks = 0;
    boundaries->bad_blocks = 0;

    // Shifting buffer consisting of two parts: senior (most significant) and
    // junior (least significant).
    //ShiftRegister shift_register = {0, 0};
    
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
        // senior_buffer = (senior_buffer << 1) | (junior_buffer >> 31);
        // junior_buffer = (junior_buffer << 1) | (bit & 1); 
        //ShiftRegister_append(&shift_register, bit);

        //printf("%lx %lx == %lx %lx\n", senior_buffer, junior_buffer, SENIOR_BLOCK_HEADER, JUNIOR_BLOCK_HEADER);   
        //printf("%lx %lx == %lx %lx\n\n", senior_buffer, junior_buffer, SENIOR_BLOCK_ENDMARK, JUNIOR_BLOCK_ENDMARK);

        // Detect block header or block end mark
        if (ShiftRegister_equal_with_senior_mask(&input_stream->shift_register, &block_header_template, 0x0000ffff)
            || ShiftRegister_equal_with_senior_mask(&input_stream->shift_register, &block_endmark_template, 0x0000ffff)) {
        // if (((shift_register.senior & 0x0000ffff) == SENIOR_BLOCK_HEADER && shift_register.junior == JUNIOR_BLOCK_HEADER) ||
        //     ((shift_register.senior & 0x0000ffff) == SENIOR_BLOCK_ENDMARK && shift_register.junior == JUNIOR_BLOCK_ENDMARK)) {

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

bz_stream *bz_stream_new() {
    bz_stream *stream = (bz_stream *) malloc(sizeof(bz_stream));
    if (stream == NULL) {
        return NULL;
    }

    stream->next_in = NULL;
    stream->avail_in = 0;
    stream->total_in_lo32 = 0;
    stream->total_in_hi32 = 0;

    stream->next_out = NULL;
    stream->avail_out = 0;
    stream->total_out_lo32 = 0;
    stream->total_out_hi32 = 0;

    stream->state = NULL; 

    stream->bzalloc = NULL;
    stream->bzfree = NULL;
    stream->opaque = NULL;

    return stream;
}

bz_stream *bz_stream_init() {
    bz_stream *stream = bz_stream_new();
    if (stream == NULL) {
        fprintf(stderr, "ERROR: cannot allocate a bzip2 stream\n");  
        return NULL;
    }

    int result = BZ2_bzDecompressInit(stream, /*verbosity*/ 0, /*small*/ 0);
    if (result != BZ_OK) {        
        fprintf(stderr, "ERROR: cannot initialize a bzip2 stream\n");  
        free(stream);
        return NULL;
    }

    // struct bz2_enc_data *data = mem_alloc(sizeof(*data));
    // if (data == NULL) {
    //     fprintf(stderr, "ERROR: cannot initialize a bzip2 data object\n"); 
        
    //     return -3;
    // }
    return stream;
}

bool FILE_has_more(FILE* file) {
    int character = fgetc(file);

    if (character == EOF) {
        return false;
    }

    ungetc(character, file);
    return true;   
}

int FILE_seek_bits(FILE *file, uint64_t bits, int whence) {
    // Maff
    uint64_t bytes = bits / 8;
    int remainder = bits % 8;

    fprintf(stderr, "DEBUG: Seeking %lib or %liB + %ib\n", bits, bytes, remainder);

    // Move to byte offset
    int seek_result = fseek(file, bytes, whence);
    if (seek_result < 1) {
        return seek_result;
    }

    // Move to bit offset
    for (int extra_bit = 0; extra_bit < remainder; extra_bit++) {
        int character = fgetc(file);
        if (character == EOF) {
            return ferror(file) ? -1 : 0;
        }        
    }    
    return 0;
}

int bz_stream_read_from_file(bz_stream *stream, FILE* input_file,
                             size_t input_buffer_size, char *input_buffer) {
    
    int read_characters = fread(input_buffer, sizeof(char), input_buffer_size, input_file);
    if (ferror(input_file)) { 
        fprintf(stderr, "ERROR: cannot read from file\n");
        return -1; 
    };        

    // Point the bzip decompressor at the newly read data.
    stream->avail_in = read_characters;
    stream->next_in = input_buffer;

    fprintf(stderr, "DEBUG: read %i bytes from file: %s\n", read_characters, stream->next_in);
    return read_characters;  
}

typedef struct {

} Block;

Block *Block_from(Blocks *boundaries, size_t index) {

    if (boundaries->read_blocks <= index) {
        fprintf(stderr, "ERROR: no block at index %li\n", index);
        return NULL;
    }

    // Retrieve the offsets, for convenience
    uint64_t block_start_offset_in_bits = boundaries->read_block_start_index[index];
    uint64_t block_end_offset_in_bits = boundaries->read_block_end_index[index];
    uint64_t payload_size_in_bits = block_end_offset_in_bits - block_start_offset_in_bits + 1;

    fprintf(stderr, "DEBUG: "
        "Reading block %li from file %s between offsets %li and %li (%lib)\n",
        index, boundaries->path, block_end_offset_in_bits, 
        block_start_offset_in_bits, payload_size_in_bits);

    // StreamHeader    =32b
    //  - HeaderMagic   16b   0x425a            B Z
    //  - Version        8b   0x68              h 
    //  - Level          8b   0x31-0x39         1-9
    // BlockHeader    =105b
    //  - BlockMagic    48b   0x314159265359    π 
    //  - BlockCRC      32b                     CRC-32 checksum of uncompressed data
    //  - Randomized     1b   0                 zero (depracated)         
    //  - OrigPtr       24b                     original pointer (BWT stage, internal)
    // StreamFooter    =80b + 0-7b
    //  - FooterMagic   48b   0x177245385090    sqrt π 
    //  - StreamCRC     32b   
    //  - Padding       0-7b    

    // Calculate size of buffer through the power of arithmetic
    uint64_t buffer_size_in_bits = 
        /* stream header size */ 32 +
        /* block header size  */ 105 +
        /* contents size      */ payload_size_in_bits +
        /* stream footer size */ 80;

    // Account for alignment at the end of stream footer, add 0-7
    uint8_t padding = (8 - (buffer_size_in_bits % 8)) % 8;
    buffer_size_in_bits += padding;

    // Calculate bytes, for convenience
    uint64_t buffer_size_in_bytes = buffer_size_in_bits / 8;


    fprintf(stderr, "DEBUG: "
            "Preparing buffer of size %lib = %ib + %ib + %lib + %ib + %ib\n",
            buffer_size_in_bits, 32, 105, payload_size_in_bits, 80, padding);

    // Alloc the buffre for the chunk
    unsigned char *buffer = (unsigned char *) calloc(buffer_size_in_bytes, sizeof(unsigned char));
    memset(buffer, 0, buffer_size_in_bytes);
    // size_t position_in_buffer = 0;

    // // Write out file magic
    // memcpy(buffer + position_in_buffer, stream_magic, stream_magic_size); 
    // // TODO rewrite last element of stream magic to whatever is read from the file to indicate correct size
    // // TODO maybe i should just read the magic from the file?
    // // TODO for now assuming 9
    // buffer[3] += 9;
    // position_in_buffer += stream_magic_size;

    // // Write out the unchanging bytes of the header of the block
    // memcpy(buffer + position_in_buffer, block_magic, block_magic_size);
    // position_in_buffer += block_magic_size;    

    // for (size_t i = 0; i < buffer_size_in_bytes; i++) {
    //     printf("%2x ", buffer[i]);
    //     if ((i + 1) % 80 == 0) {
    //         printf("\n");
    //     }
    // }
    // printf("\n");

    // Open file for reading the remainder
    fprintf(stderr, "DEBUG: Reading file %s\n", boundaries->path);
    FILE *input_file = fopen(boundaries->path, "rb");
    if (input_file == NULL) {
        fprintf(stderr, "ERROR: cannot open file at %s\n", boundaries->path);
        return NULL;
    }
    size_t position_in_file = 0;

    // Assume the first boundary is at a byte boundary, otherwise something is very wrong
    // assert(boundaries->read_block_start_index[0] % 8 == 0); // FIXME

    // Read in everything before the first block boundary
    // FIXME This has to be done on the bit stream
    for (; position_in_file < boundaries->read_block_start_index[0] / 8; position_in_file++) {
        buffer[position_in_file] = fgetc(input_file);
    }

    printf("DEBUG: Header (%lib/%liB) ", boundaries->read_block_start_index[0], position_in_file);
    for (size_t i = 0; i < position_in_file; i++) {
        printf("%2x ", buffer[i]);
        if ((i + 1) % 80 == 0) {
            printf("\n");
        }
    }
    printf("\n");

    // FIXME seek is broken: fgetc gets a whole byte, not a single bit.
    int seek_result = FILE_seek_bits(input_file, block_start_offset_in_bits, SEEK_SET);
    if (seek_result < 0) {
        fprintf(stderr, "ERROR: cannot seek to %lib offset\n", block_start_offset_in_bits);
    }    

    //size_t bits_read = position_in_buffer;


    return NULL;
}
 
 
 int Blocks_read_block(Blocks *boundaries, size_t i) {

    if (boundaries->read_blocks == 0) {
        fprintf(stderr, "ERROR: no blocks found to extract data from\n");
        return -7;
    }

    bz_stream *stream = bz_stream_init();
    if (stream == NULL) {
        fprintf(stderr, "ERROR: cannot initialize stream\n");  
        return -8;
    }  

    size_t output_buffer_size = 9000;
    size_t input_buffer_size = 2898839 - 1627422;
    size_t header_buffer_size = 10; // bytes

    char output_buffer[output_buffer_size];    
    char input_buffer[input_buffer_size];
    char header_buffer[output_buffer_size];        

    // Set array contents to zero, just in case.
    memset(output_buffer, 0, output_buffer_size);
    memset(input_buffer, 0, input_buffer_size);
    memset(header_buffer, 0, header_buffer_size);

    // Tell BZip2 where to write decompressed data.
	stream->avail_out = output_buffer_size;
	stream->next_out = output_buffer;

    fprintf(stderr, "DEBUG: Reading file %s\n", boundaries->path);
    // Open file for reading
    FILE *input_file = fopen(boundaries->path, "rb");
    if (input_file == NULL) {
        fprintf(stderr, "ERROR: cannot open file at %s\n", boundaries->path);
        return -1;
    }

    // // Read the header
    // {
    //     if (ferror(input_file)) {
    //         fprintf(stderr, "ERROR: file read error\n");
    //         return -6;
    //     }
    //     int result = bz_stream_read_from_file(stream, input_file, header_buffer_size, header_buffer);
    //     if (result < 0) {
    //         fprintf(stderr, "ERROR: cannot load header from file %s into header buffer\n", boundaries->path);
    //         return result;
    //     }
    //     result = BZ2_bzDecompress(stream);
    //     if (result != BZ_OK) {
    //         fprintf(stderr, "ERROR: cannot process the header\n");
    //     }
    // }

    // TODO assert: output buffer should still be clean

    // TODO loop starts here
    FILE_seek_bits(input_file, 1627373/*1627422*/, SEEK_SET);

    //int i=1;
    // for (int i = 0; i < 1; i++) {

        if (ferror(input_file)) {
            fprintf(stderr, "ERROR: file read error\n");
            return -6;
        }

        // If nothing in buffer, read some compressed data into it from the file.
        if (stream->avail_in == 0 && FILE_has_more(input_file))  {
            int result = bz_stream_read_from_file(stream, input_file, input_buffer_size, input_buffer);
            if (result < 0) {
                fprintf(stderr, "ERROR: cannot load data from file %s into input buffer\n", boundaries->path);
                return result;
            }
        }

        int result = BZ2_bzDecompress(stream);

        if (result != BZ_OK && result != BZ_STREAM_END) { 
            fprintf(stderr, "ERROR: cannot process stream, error no.: %i\n", result);
            return -3;
        };

        if (result == BZ_OK 
            && !FILE_has_more(input_file) 
            && stream->avail_in == 0 
            && stream->avail_out > 0) {

            fprintf(stderr, "ERROR: cannot process stream, unexpected end of file\n");
            return -4; 
        };

        if (result == BZ_STREAM_END) {        
            fprintf(stderr, "WARNING: stream ended before end of file\n");
            printf("%li read: %i -> %s\n", i, result, output_buffer);
            return output_buffer_size - stream->avail_out; 
        };

        if (stream->avail_out == 0) {   
            fprintf(stderr, "DEBUG: stream ended: no data read\n");  
            printf("%li read: %i -> %s\n", i, result, output_buffer);
            return output_buffer_size; 
        };
        
        printf("%li read: %i -> %s\n", i, result, output_buffer);

    // }

    return 0; // unreachable
}

int main(int argc, char *argv[]) {
    Blocks *blocks = Blocks_parse("test/test2.txt.bz2");
    // Blocks_read_block(blocks, 0);


    for (size_t i = 0; i < blocks->read_blocks; i++) {
        Block_from(blocks, i);
    }

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
    // char    buf[ BUFFER_SIZE ];true
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