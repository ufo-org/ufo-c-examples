#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include "ufo_c/target/ufo_c.h"

#define HIGH_WATER_MARK (2L * 1024 * 1024 * 1024)
#define LOW_WATER_MARK  (1L * 1024 * 1024 * 1024)
#define MIN_LOAD_COUNT  (4L * 1024)

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

// Static elements of headers: sizes
const int stream_magic_size = 4;
const int block_magic_size = 6;
const int footer_magic_size = 6;

// Static elements of headers: values
const unsigned char stream_magic[] = { 0x42, 0x5a, 0x68, 0x30 };
const unsigned char block_magic[]  = { 0x31, 0x41, 0x59, 0x26, 0x53, 0x59 };
const unsigned char footer_magic[] = { 0x17, 0x72, 0x45, 0x38, 0x50, 0x90 };

// Shift register: tracks 64 most recently read bits.
typedef struct {
    uint32_t senior;
    uint32_t junior;
} ShiftRegister;

// Static elements of headers as shift register values.
const ShiftRegister block_header_template  = { 0x00003141UL, 0x59265359UL };
const ShiftRegister block_endmark_template = { 0x00001772UL, 0x45385090UL };

void ShiftRegister_append(ShiftRegister *shift_register, int bit) {
    shift_register->senior = (shift_register->senior << 1) | (shift_register->junior >> 31);
    shift_register->junior = (shift_register->junior << 1) | (bit & 1);  
}

bool ShiftRegister_equal_with_senior_mask(const ShiftRegister *a, const ShiftRegister *b, uint32_t senior_mask) {
    return (a->junior == b->junior) && ((a->senior & senior_mask) == (b->senior & senior_mask));
}

int ShiftRegister_junior_byte(const ShiftRegister *shift_register) {
    return shift_register->junior & 0x000000ff;
}

typedef struct {
    unsigned char *data;
    uint32_t       current_bit;
    size_t         current_byte;
    size_t         max_bytes;
    // ShiftRegister shift_register;
} BitBuffer;

BitBuffer BitBuffer_new(size_t max_bytes) {
    BitBuffer buffer;

    buffer.data = (unsigned char *) calloc(max_bytes, sizeof(unsigned char));
    buffer.current_bit = 0;
    buffer.current_byte = 0;
    buffer.max_bytes = max_bytes;

    if (buffer.data != NULL) {
        memset(buffer.data, 0, max_bytes);
    }

    return buffer;
}

int BitBuffer_append_bit(BitBuffer* buffer, unsigned char bit) {
    assert(buffer->current_bit < 8);
    if (buffer->data == NULL) {
        fprintf(stderr, "ERROR: BitBuffer has uninitialized data\n");
        return -2;
    }

    // Bit twiddle the new bit into position
    buffer->data[buffer->current_byte] |= bit << (7 - buffer->current_bit);

    // Print
    // fprintf(stderr, "DEBUG: Append %i to BitBuffer %liB + %ib\n", 
            // bit, buffer->current_byte, buffer->current_bit);

    // for (size_t i = 0; i < buffer->max_bytes; i++) {
    //     printf("%02x ", buffer->data[i]);
    //     if ((i + 1) % 80 == 0) {
    //         printf("\n");
    //     }
    // }
    // printf("\n");

    // If overflow bit, bump bytes. This goes first.
    if (buffer->current_bit == 7) {
        buffer->current_byte += 1;
        if (buffer->current_bit >= buffer->max_bytes) {
            fprintf(stderr, "ERROR: BitBuffer overflow\n");
            return -1;
        }
    }

    // Bump bits. 
    buffer->current_bit = (buffer->current_bit + 1) % 8;

    // We're done?    
    return 0;
}

int BitBuffer_append_byte(BitBuffer* buffer, unsigned char byte) {
    for (unsigned char i = 0; i < 8; i++) {
        unsigned char mask = 1 << (8 - 1 - i);
        unsigned char bit = (byte & mask) > 0 ? 1 : 0;
        //printf("byte=%02x i=%i mask=%02x ?=%02x bit=%i ", byte, i, mask, (byte & mask), bit);
        //printf("current byte=%li current bit=%i ", buffer->current_byte, buffer->current_bit);
        int result = BitBuffer_append_bit(buffer, bit);
        if (result < 0) {
            return result;
        }    
        // for (int ix = 0; ix < buffer->max_bytes; ix++) {
        //     printf("%02x ", buffer->data[ix]);
        // }
        // printf("\n");
    }
    return 0;
}

int BitBuffer_append_uint32(BitBuffer* buffer, uint32_t value) {
    for (unsigned char i = 0; i < 32; i++) {
        uint32_t mask = 1 << (32 - 1 - i);
        uint32_t bit = (value & mask) > 0 ? 1 : 0;
        // printf("value=%08x i=%i mask=%08x ?=%08x bit=%i\n", value, i, mask, (value & mask), bit);
        int result = BitBuffer_append_bit(buffer, bit);
        if (result < 0) {
            return result;
        }    
    }
    return 0;
}

typedef struct {
    FILE*         handle;
    uint32_t      buffer;
    uint32_t      buffer_live;
    const char   *path;
    uint64_t      read_bits;    
    ShiftRegister shift_register;
} FileBitStream;

FileBitStream *FileBitStream_new(const char *input_file_path) {
    // Allocate bit stream for reading
    FileBitStream *input_stream = (FileBitStream *) malloc(sizeof(FileBitStream));
    if (input_stream == NULL) {
        fprintf(stderr, "ERROR: cannot allocate FileBitStream object\n");  
        return NULL;
    }

    // Initially the buffer is empty
    input_stream->buffer = 0;
    input_stream->buffer_live = 0;
    input_stream->read_bits = 0;

    // Initally the shift register is empty
    input_stream->shift_register.senior = 0;
    input_stream->shift_register.junior = 0;

    // Open the input bzip2 file
    FILE* input_file = fopen(input_file_path, "rb");
    if (input_file == NULL) {
        fprintf(stderr, "ERROR: cannot read file at %s\n", input_file_path);
        free(input_stream);
        return NULL;
    }

    // Update file info
    input_stream->path = input_file_path; // FIXME 0-terminate
    input_stream->handle = input_file;   
    return input_stream;
}

// Returns 0 or 1, or <0 to indicate EOF or error
int FileBitStream_read_bit(FileBitStream *input_stream) {

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

int FileBitStream_read_byte(FileBitStream *input_stream) {
    for (int bit = 0; bit < 8; bit++) {
        int result = FileBitStream_read_bit(input_stream);
        if (result < 0) {
            return result;
        }
    }
    return ShiftRegister_junior_byte(&input_stream->shift_register);
}

uint32_t FileBitStream_read_uint32(FileBitStream *input_stream) {
    for (int bit = 0; bit < 32; bit++) {
        int result = FileBitStream_read_bit(input_stream);
        if (result < 0) {
            return result;
        }
    }
    return input_stream->shift_register.junior;
}

int FileBitStream_seek_bit(FileBitStream *input_stream, uint64_t bit_offset) {

    // Don't feel like solving the case for < 64, we never use it, since the
    // first block will start around 80b.
    if (bit_offset < (sizeof(uint32_t) * 2)) {
        fprintf(stderr, "ERROR: Seeking to bit offset < 64b is not allowed.\n");
    }

    // Maff
    uint64_t byte_offset = bit_offset / 8;
    uint64_t byte_offset_without_buffer = byte_offset - (sizeof(uint32_t) * 2); // Computron will maff
    int remainder_bit_offset = bit_offset % 8;

    fprintf(stderr, "DEBUG: Seeking %lib = %liB + %liB + %ib.\n", 
            bit_offset, byte_offset_without_buffer, 
            (sizeof(uint32_t) * 2), remainder_bit_offset);

    // Reset the buffer
    input_stream->buffer = 0;
    input_stream->buffer_live = 0;

    // Move to byte offset
    int seek_result = fseek(input_stream->handle, byte_offset_without_buffer, SEEK_SET);
    if (seek_result < -1) {
        perror("ERROR");
        fprintf(stderr, "ERROR: Seek failed in %s\n", input_stream->path);
        return seek_result;
    }

    // Repopulate the shift register. This should read 64b, and we should be at
    // `byte offset` afterwards.
    size_t read_bytes = fread(&input_stream->shift_register.senior, sizeof(uint32_t), 1, input_stream->handle);
    if (read_bytes != 1) {
        fprintf(stderr, "ERROR: Error populating senior part of shift buffer " 
                "(read %li bytes instead of the expected %i).\n", 
                read_bytes, 1);
    }
    read_bytes = fread(&input_stream->shift_register.junior, sizeof(uint32_t), 1, input_stream->handle);
    if (read_bytes != 1) {
        fprintf(stderr, "ERROR: Error populating junior part of shift buffer " 
                "(read %li bytes instead of the expected %i).\n", 
                read_bytes, 1);
    }

    fprintf(stderr, "DEBUG: Populated buffer %x :: %x.\n", 
            input_stream->shift_register.senior, 
            input_stream->shift_register.junior);

    // Seeking counts as reading the bits, so we account for all the bits. We
    // oly ever seek form the beginning of the file, so this is easy.
    input_stream->read_bits = byte_offset * 8;

    // Move to bit offset
    for (int extra_bit = 0; extra_bit < remainder_bit_offset; extra_bit++) {
        int bit = FileBitStream_read_bit(input_stream);
        if (bit == -1) {
            fprintf(stderr, "ERROR: Seek error, unexpected EOF at %li", 
                    byte_offset * 8 + extra_bit);
            return -1;
        }
        if (bit == -2) {
            fprintf(stderr, "ERROR: Seek error.");
            return -2;
        }        
    }    

    // Victory is ours.
    return 0;
}

void FileBitStream_free(FileBitStream *input_stream) {   
    // Close file associated with stream
    if (fclose(input_stream->handle) == EOF) {
        fprintf(stderr, "WARNING: failed to close file at %s\n", 
                input_stream->path); 
    }

    // Free memory
    free(input_stream);
}

// A structure representing blocks in a BZIP2 file. Records offset of beginning
// and end of each block.
typedef struct {
    const char *path;
    size_t blocksx;
    uint64_t start_offset[MAX_BLOCKS];
    uint64_t end_offset[MAX_BLOCKS];
    uint64_t decompressed_start_offset[MAX_BLOCKS];    
    uint64_t decompressed_end_offset[MAX_BLOCKS];   
    size_t bad_blocks;
    size_t decompressed_size;
} Blocks;

Blocks *Blocks_parse(const char *input_file_path) {

    // Open file for reading.
    FileBitStream *input_stream = FileBitStream_new(input_file_path);
    if (input_stream == NULL) {
        fprintf(stderr, "ERROR: cannot open file %s\n", input_file_path);  
        return NULL;
    }

    size_t blocks = 0;
    uint64_t start_offset[MAX_BLOCKS];
    uint64_t end_offset[MAX_BLOCKS];

    // Initialize the counters.
    Blocks *boundaries = (Blocks *) malloc(sizeof(Blocks));    
    if (NULL == boundaries) {
        fprintf(stderr, "ERROR: cannot allocate a struct for recording block boundaries\n");  
        return NULL;
    }

    boundaries->path = input_file_path;
    boundaries->blocksx = 0;
    boundaries->bad_blocks = 0;

    // Shifting buffer consisting of two parts: senior (most significant) and
    // junior (least significant).
    //ShiftRegister shift_register = {0, 0};
    
    while (true) {
        int bit = FileBitStream_read_bit(input_stream);

        // In case of errors or encountering end of file. We recover, although
        // we probably shouldn't. I don't want to mess with the function too
        // much.
        if (bit < 0) {
            // If we're inside a block (not at the beginning or within an end
            // marker) we report an error and stop processing the block.
            if (input_stream->read_bits >= start_offset[blocks] && 
               (input_stream->read_bits - start_offset[blocks]) >= 40) {
                end_offset[blocks] = input_stream->read_bits - 1;
                if (blocks > 0) {
                    fprintf(stderr, "DEBUG: block %li runs from %li to %li (incomplete)\n",
                            blocks, 
                            start_offset[blocks], 
                            end_offset[blocks]);

                    boundaries->bad_blocks++;
                }               
            } else {
                // Otherwise, I guess this is just EOF?
                blocks--;
            }

            // Proceed to next block.
            break;
        }

        // Detect block header or block end mark
        if (ShiftRegister_equal_with_senior_mask(&input_stream->shift_register, &block_header_template, 0x0000ffff)
            || ShiftRegister_equal_with_senior_mask(&input_stream->shift_register, &block_endmark_template, 0x0000ffff)) {

            // If we found a bounary, the end of the preceding block is 49 bytes
            // ago (or zero if it's the first block)
            end_offset[blocks] = 
                (input_stream->read_bits > 49) ? input_stream->read_bits - 49 : 0;

            // We finished reading a whole block
            if (blocks > 0 
                && (end_offset[blocks] 
                - start_offset[blocks]) >= 130) {

                fprintf(stderr, "DEBUG: block %li runs from %li to %li\n",
                        boundaries->blocksx + 1, 
                        start_offset[blocks], 
                        end_offset[blocks]);

                // Store the offsets
                boundaries->start_offset[boundaries->blocksx] = 
                    start_offset[blocks];
                boundaries->end_offset[boundaries->blocksx] = 
                    end_offset[blocks];

                // Increment number of complete blocks so far
                boundaries->blocksx++;

                //start_offset[blocks] = input_stream->read_bits;
            }

            // Too many blocks
            if (blocks >= MAX_BLOCKS) {
                fprintf(stderr, "ERROR: analyzer can handle up to %i blocks, "
                                "but more blocks were found in file %s\n",
                                MAX_BLOCKS, input_stream->path);
                free(boundaries);
                return NULL;
            }

            // Increment number of blocks encountered so far and record the
            // start offset of next encountered block
            blocks++;
            start_offset[blocks] = input_stream->read_bits;
        }
    }

    // Clean up and exit
    FileBitStream_free(input_stream);        
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

    return stream;
}

// bool FILE_has_more(FILE* file) {
//     int character = fgetc(file);

//     if (character == EOF) {
//         return false;
//     }

//     ungetc(character, file);
//     return true;   
// }

// int FILE_seek_bits(FILE *file, uint64_t bits, int whence) {
//     // Maff
//     uint64_t bytes = bits / 8;
//     int remainder = bits % 8;

//     fprintf(stderr, "DEBUG: Seeking %lib or %liB + %ib\n", bits, bytes, remainder);

//     // Move to byte offset
//     int seek_result = fseek(file, bytes, whence);
//     if (seek_result < 1) {
//         return seek_result;
//     }

//     // Move to bit offset
//     for (int extra_bit = 0; extra_bit < remainder; extra_bit++) {
//         int character = fgetc(file);
//         if (character == EOF) {
//             return ferror(file) ? -1 : 0;
//         }        
//     }    
//     return 0;
// }

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
    unsigned char   *buffer;
    size_t  size;
    size_t  max_size;
} Block;

int __block = 0;

Block *Block_from(Blocks *boundaries, size_t index) {

    if (boundaries->blocksx <= index) {
        fprintf(stderr, "ERROR: no block at index %li\n", index);
        return NULL;
    }

    // Retrieve the offsets, for convenience
    const uint64_t block_start_offset_in_bits = boundaries->start_offset[index];
    const uint64_t block_end_offset_in_bits = boundaries->end_offset[index]; // Inclusive
    const uint64_t payload_size_in_bits = block_end_offset_in_bits - block_start_offset_in_bits + 1;
    const uint64_t header_end_offset_in_bits = boundaries->start_offset[0];

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

    // Assume the first boundary is at a byte boundary, otherwise something is very wrong
    assert(boundaries->start_offset[0] % 8 == 0);

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

    // Open file for reading the remainder
    FileBitStream *input_stream = FileBitStream_new(boundaries->path);
    if (input_stream == NULL) {
        fprintf(stderr, "ERROR: cannot open bit stream for %s\n", boundaries->path);
        // TODO free what needs freeing
        return NULL;
    }

    // Read in everything before the first block boundary
    size_t read_bytes = 0;
    while (input_stream->read_bits < header_end_offset_in_bits) {
        int byte = FileBitStream_read_byte(input_stream);
        if (byte < 0) {
            fprintf(stderr, "ERROR: cannot read byte from bit stream\n");
            FileBitStream_free(input_stream);
            // TODO free what needs freeing
            return NULL;
        }
        buffer[read_bytes] = byte;
        read_bytes++;
    }

    fprintf(stderr, "DEBUG: Stream header (%lib/%liB) ", header_end_offset_in_bits, read_bytes);
    for (size_t i = 0; i < read_bytes; i++) {
        fprintf(stderr, "%2x ", buffer[i]);
        if ((i + 1) % 80 == 0) {
            fprintf(stderr, "\n");
        }
    }
    fprintf(stderr, "\n");

    // Seek to the block boundary
    int seek_result = FileBitStream_seek_bit(input_stream, block_start_offset_in_bits);
    if (seek_result < 0) {
        fprintf(stderr, "ERROR: cannot seek to %lib offset\n", block_start_offset_in_bits);
        return NULL;
    }

    // Read 32-bit CRC, which is just behind the boundary
    uint32_t block_crc = FileBitStream_read_uint32(input_stream);
    fprintf(stderr, "DEBUG: Block CRC %08x\n", block_crc);
    if (block_crc < 0) {
        fprintf(stderr, "ERROR: cannot read uint32 from bit stream\n");
        FileBitStream_free(input_stream);
        // TODO free what needs freeing
        return NULL;
    }

    for (int i = sizeof(uint32_t) - 1; i >= 0; i--) {
        buffer[read_bytes++] = 0xff & (block_crc >> (8 * i));
    }
    
    // This is how much we can copy byte-by-byte before getting into trouble at
    // the end of the file. The remainder we have to copy bit-by-bit.

    //payload_size_in_bits
    size_t block_end_byte_aligned_offset_in_bits = (payload_size_in_bits & (~0x07)) + block_start_offset_in_bits;
    //(((block_end_offset_in_bits) / 8) * 8);
    size_t remaining_byte_unaligned_bits = (payload_size_in_bits & (0x07));
    //(block_end_offset_in_bits + 1) % 8;

    // block 0 has to read until offset 1627373b = 1627368b + 6b                    80
    // block 1 has to read until offset 2898839b = 2898832b + 0b  BUT SHOULD BE 2b          6
    // block 2 has to read until offset 4162819b = 4162816b + 4b 
    // block 3 has to read until offset 4371764b = 4371760b + 5b  BUT SHOULD BE 1b          4

    // // FIXME 
    // if (__block == 1) {
    //     remaining_byte_unaligned_bits = 2;
    // }
    // if (__block == 3) {
    //     remaining_byte_unaligned_bits = 1;
    // }

    printf("ZZZ %li\n", input_stream->read_bits);

    fprintf(stderr, "DEBUG: The block has to read until offset %lib = %lib + %lib. [%lib]\n", 
            block_end_offset_in_bits,
            block_end_byte_aligned_offset_in_bits, 
            remaining_byte_unaligned_bits,
            block_start_offset_in_bits);

    // Read until the last byte-aligned datum of the block
    while (input_stream->read_bits < block_end_byte_aligned_offset_in_bits) {
        int byte = FileBitStream_read_byte(input_stream);
        //printf("%4li %4li %02x \n", input_stream->read_bits, block_end_offset_in_bits, byte);
        if (byte < 0) {
            // printf("%li %li %02x \n", input_stream->read_bits, block_end_offset_in_bits, byte);
            fprintf(stderr, "ERROR: cannot read byte from bit stream\n");
            FileBitStream_free(input_stream);
            // TODO free what needs freeing
            return NULL;
        }
        buffer[read_bytes] = byte;
        read_bytes++;
    }
    // printf("%li %li \n", input_stream->read_bits, block_end_offset_in_bits);
    printf("ZZZ %li\n", input_stream->read_bits);
    //printf("--- %02x\n", FileBitStream_read_byte(input_stream));

    // Ok, now we write the rest into a buffer to align it.
    BitBuffer bit_buffer = BitBuffer_new(1 + footer_magic_size + 4); // TODO size
    for (int annoying_unaligned_bit = 0; 
         annoying_unaligned_bit < remaining_byte_unaligned_bits ; // sus
         annoying_unaligned_bit++) {

        // Read a bit from file, yay
        int bit = FileBitStream_read_bit(input_stream);
        if (bit < 0) {
            fprintf(stderr, "ERROR: cannot read bit from bit stream\n");
            return NULL;
        }

        printf("unaligned bit %i/%li -> %i\n", annoying_unaligned_bit, remaining_byte_unaligned_bits, bit);

        // Write that bit to buffer, yay
        int result = BitBuffer_append_bit(&bit_buffer, (unsigned char) bit);
        if (result < 0) {
            fprintf(stderr, "ERROR: cannot write bit to bit buffer\n");
            return NULL;
        }
    }

    // XXX
    printf("XXX ");
    for (int i = 0; i < bit_buffer.max_bytes; i++) {
        printf("%02x ", bit_buffer.data[i]);
    }
    printf("\n");

    // Ok, now we write out the footer to the buffer, to align that.
    fprintf(stderr, "DEBUG: Footer magic begins\n");
    for (int footer_magic_byte = 0; 
         footer_magic_byte < footer_magic_size; 
         footer_magic_byte++) {

        fprintf(stderr, "footer magic byte %2x \n", footer_magic[footer_magic_byte]);
        int result = BitBuffer_append_byte(&bit_buffer, footer_magic[footer_magic_byte]);
        if (result < 0) {
            fprintf(stderr, "ERROR: cannot write magic footer byte to bit buffer\n");
            return NULL;
        }

        // printf("xxx ");
        // for (int i = 0; i < bit_buffer.max_bytes; i++) {
        //     printf("%02x ", bit_buffer.data[i]);
        // }
        // printf("\n");
    }

    // Write out the block CRC value as the stream CRC value, since it's just
    // one block.
    fprintf(stderr, "DEBUG: Write out stream CRC value to %04x\n", block_crc);
    int result = BitBuffer_append_uint32(&bit_buffer, block_crc);
    if (result < 0) {
        fprintf(stderr, "ERROR: cannot write CRC byte to bit buffer\n");
        return NULL;
    }

    //buffer[read_bytes] |= bit_buffer.data[0];

    

    // Write the bit buffer into the actual buffer.
    printf("YYY ");
    for (int i = 0; i < bit_buffer.max_bytes; i++) {
        printf("%02x ", bit_buffer.data[i]);
        buffer[read_bytes + i] = bit_buffer.data[i];
    }
    printf("\n");
    read_bytes += bit_buffer.max_bytes;

    // The whole enchilada
    // fprintf(stderr, "DEBUG: Constructed buffer for this block\n");
    // for (size_t i = 0; i < read_bytes; i++) {
    //     printf("%02x ", buffer[i]);
    //     if ((i + 1) % 80 == 0) {
    //         printf("\n");
    //     }
    // }
    // printf("\n");

    // TODO: free what needs freeing

    // Return block data
    Block *block = malloc(sizeof(Block));
    block->buffer = buffer;
    block->size = read_bytes;
    block->max_size = buffer_size_in_bytes;
    return block;
}

int Block_decompress(Block *block, size_t output_buffer_size, char *output_buffer) {

    // Hand-crank the BZip2 decompressor
    bz_stream *stream = bz_stream_init();
    if (stream == NULL) {
        fprintf(stderr, "ERROR: cannot initialize stream\n");  
        return -1;
    }         

    // Set array contents to zero, just in case.
    memset(output_buffer, 0, output_buffer_size);

    // Tell BZip2 where to write decompressed data.
	stream->avail_out = output_buffer_size;
	stream->next_out = output_buffer;

    // Tell BZip2 where to getr input data from.
    stream->avail_in = block->size;
    stream->next_in  = (char *) block->buffer;

    // Debug print
    printf("DEBUG: Compressed block\n");
    for (int i = 0; i < 64; i++) {
        printf("%02x ", (unsigned char) stream->next_in[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }
    printf("\n\n");
    for (int i = stream->avail_in - (64 + 4); i < stream->avail_in; i++) {
        printf("%02x ", (unsigned char) stream->next_in[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }
    printf("\n");

    // Do the do.
    int result = BZ2_bzDecompress(stream);

    if (result != BZ_OK && result != BZ_STREAM_END) { 
        fprintf(stderr, "ERROR: cannot process stream, error no.: %i\n", result);
        return -3;
    };

    if (result == BZ_OK 
        && stream->avail_in == 0 
        && stream->avail_out > 0) {

        fprintf(stderr, "ERROR: cannot process stream, unexpected end of file\n");
        return -4; 
    };

    if (result == BZ_STREAM_END) {        
        return output_buffer_size - stream->avail_out; 
    };

    if (stream->avail_out == 0) {   
        fprintf(stderr, "DEBUG: stream ended: no data read\n");  
        return output_buffer_size; 
    };

    // Supposedly unreachable.
    return 0;
}

Blocks *Blocks_new(char *filename) {
    // Parse the file.
    Blocks *blocks = Blocks_parse(filename);

    // Create the structures for outputting the decompressed data into
    // (temporarily)
    size_t output_buffer_size = 1024 * 1024 * 1024; // 1MB
    char *output_buffer = (char *) calloc(output_buffer_size, sizeof(char));
    
    // Unfortunatelly, we need to scan the entire file to figure out where the blocks go when decompressed.
    size_t end_of_last_decomporessed_block = 0;
    for (size_t i = 0; i < blocks->blocksx; i++) {
        // Extract a single compressed block.
        __block = i;
        Block *block = Block_from(blocks, i);
        int output_buffer_occupancy = Block_decompress(block, output_buffer_size, output_buffer);
        printf("BUFFER_SIZE=%d\n", output_buffer_occupancy);   

        if (output_buffer_occupancy < 0) {
            fprintf(stderr, "ERROR: Failed to decompress block %ld\n", i);
            break;
        }

        // printf("%li %li %li\n", end_of_last_decomporessed_block, start_of_decompressed_block, end_of_decomporessed_block);

        // Calculate start and end of this block when it is decompressed.
        // TODO we could defer some of this.
        blocks->decompressed_start_offset[i] = end_of_last_decomporessed_block;        
        blocks->decompressed_end_offset[i] = blocks->decompressed_start_offset[i] + output_buffer_occupancy;
        end_of_last_decomporessed_block += blocks->decompressed_end_offset[i];
    }

    blocks->decompressed_size = end_of_last_decomporessed_block;

    return blocks;
}

typedef char *BZip2;

static int32_t BZip2_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    // Blocks *blocks = (Blocks *) user_data;

    // bool found_start_block = false;
    // bool found_end_block = false;
    
    // size_t start_block;
    // size_t end_block;

    // size_t position = start;

    // Bleh

    // for (size_t block_index=0; block_index < blocks->blocksx; block_index++) {
    //     if (position >= blocks->decompressed_start_offset[block_index] 
    //         && start <= blocks->decompressed_end_offset[block_index]) {
    //         assert(!found_start_block);
    //         start_block = block_index;
    //     }
    //     if (end >= blocks->decompressed_start_offset[block_index] 
    //         && end <= blocks->decompressed_end_offset[block_index]) {
    //         assert(!found_end_block);
    //         end_block = block_index;
    //     }
    // }

    // if (!start_block || !end_block) {
    //     return -1;
    // }

    // size_t position = start;
    // for (size_t block_index = start_block; block_index <= end_block; block_index++) {

    // }

    // size_t output_buffer_size = 1024 * 1024 * 1024; // 1MB
    // char *output_buffer = (char *) calloc(output_buffer_size, sizeof(char));

    return 1;
}

BZip2 BZip2_new(UfoCore *ufo_system, char *filename) {
    
    Blocks *blocks = Blocks_new(filename);

    // TODO check for bad blocks

    UfoParameters parameters;
    parameters.header_size = 0;
    parameters.element_size = strideOf(char);
    parameters.element_ct = blocks->decompressed_size;
    parameters.min_load_ct = MIN_LOAD_COUNT;
    parameters.read_only = true;
    parameters.populate_data = blocks;
    parameters.populate_fn = BZip2_populate;

    UfoObj ufo_object = ufo_new_object(ufo_system, &parameters);
    
    return (BZip2) ufo_header_ptr(&ufo_object);   
}

void BZip2_free(UfoCore *ufo_system, BZip2 ptr) {
    UfoObj ufo_object = ufo_get_by_address(ufo_system, ptr);
    if (ufo_is_error(&ufo_object)) {
        fprintf(stderr, "Cannot free %p: not a UFO object.\n", ptr);
        return;
    }
    // TODO free what needs freeing    
    ufo_free(ufo_object);
}

int main(int argc, char *argv[]) {
    UfoCore ufo_system = ufo_new_core("/tmp/ufos/", HIGH_WATER_MARK, LOW_WATER_MARK);

    BZip2 object = BZip2_new(&ufo_system, "test/test2.txt.bz2");

    BZip2_free(&ufo_system, object);
    ufo_core_shutdown(ufo_system);
}