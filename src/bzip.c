#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>

#include "bzip.h"
#include "ufo_c/target/ufo_c.h"

#include <bzlib.h>

//#define DEBUG
#ifdef DEBUG 
#define LOG(...) fprintf(stderr, "DEBUG: " __VA_ARGS__)
#define LOG_SHORT(...) fprintf(stderr,  __VA_ARGS__)
#else
#define LOG(...) 
#define LOG_SHORT(...) 
#endif
#define WARN(...) fprintf(stderr, "WARNING: " __VA_ARGS__)
#define REPORT(...) fprintf(stderr, "ERROR: " __VA_ARGS__)

#define HIGH_WATER_MARK (2L * 1024 * 1024 * 1024)
#define LOW_WATER_MARK  (1L * 1024 * 1024 * 1024)
#define MIN_LOAD_COUNT  (900L * 1024) // around block size, so around 900kB

// The bzip code was adapted from bzip2recovery.

// Sizes of temporary buffers used to chunk and decompress BZip data.
#define BUFFER_SIZE 1024
#define DECOMPRESSED_BUFFER_SIZE 1024 * 1024 * 1024 // 900kB + some room just in case

// This is the maximum number of blocks that we expect in a file. We cannot
// process more than this number. 50K blocks represents 40GB+ of uncompressed
// data. 
#define MAX_BLOCKS 50000 // TODO: grow the vector instead?

// These are the sizes of magic byte sequences in BZip files. All in bytes.
const int stream_magic_size = 4;
const int block_magic_size = 6;
const int footer_magic_size = 6;

// These are the header and endmark values expected to demarkate blocks.
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

// This is a buffer to which bytes or individuaL bits can both be appended.
typedef struct {
    unsigned char *data;
    uint32_t       current_bit;
    size_t         current_byte;
    size_t         max_bytes;
} BitBuffer;

BitBuffer *BitBuffer_new(size_t max_bytes) {
    BitBuffer *buffer = malloc(sizeof(BitBuffer));

    if (buffer == NULL) {
        REPORT("Cannot allocate BitBuffer.");
        return NULL;
    }

    buffer->data = (unsigned char *) calloc(max_bytes, sizeof(unsigned char));
    buffer->current_bit = 0;
    buffer->current_byte = 0;
    buffer->max_bytes = max_bytes;

    if (buffer->data == NULL) {
        REPORT("Cannot allocate internal buffer in BitBuffer.");
        free(buffer);
        return NULL;
    }

    // Just in case.
    memset(buffer->data, 0, max_bytes);

    return buffer;
}

void BitBuffer_free(BitBuffer *buffer) {
    free(buffer->data);
    free(buffer);
}

int BitBuffer_append_bit(BitBuffer* buffer, unsigned char bit) {
    assert(buffer->current_bit < 8);
    if (buffer->data == NULL) {
        REPORT("BitBuffer has uninitialized data.\n");
        return -2;
    }

    // Bit twiddle the new bit into position
    buffer->data[buffer->current_byte] |= bit << (7 - buffer->current_bit);

    // If overflow bit, bump bytes. This goes first.
    if (buffer->current_bit == 7) {
        buffer->current_byte += 1;
        if (buffer->current_bit >= buffer->max_bytes) {
            REPORT("ERROR: BitBuffer overflow.\n");
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
        int result = BitBuffer_append_bit(buffer, bit);
        if (result < 0) {
            return result;
        }    
    }
    return 0;
}

int BitBuffer_append_uint32(BitBuffer* buffer, uint32_t value) {
    for (unsigned char i = 0; i < 32; i++) {
        uint32_t mask = 1 << (32 - 1 - i);
        uint32_t bit = (value & mask) > 0 ? 1 : 0;
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
        REPORT("Cannot allocate FileBitStream object.\n");  
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
        REPORT("Cannot read file at %s.\n", input_file_path);
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
            REPORT("Cannot read bit from file at %s.\n", 
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
        REPORT("Seeking to bit offset < 64b is not allowed.\n");
    }

    // Maff
    uint64_t byte_offset = bit_offset / 8;
    uint64_t byte_offset_without_buffer = byte_offset - (sizeof(uint32_t) * 2); // Computron will maff
    int remainder_bit_offset = bit_offset % 8;

    // Reset the buffer
    input_stream->buffer = 0;
    input_stream->buffer_live = 0;

    // Move to byte offset
    int seek_result = fseek(input_stream->handle, byte_offset_without_buffer, SEEK_SET);
    if (seek_result < -1) {
        perror("ERROR");
        REPORT("Seek failed in %s\n", input_stream->path);
        return seek_result;
    }

    // Repopulate the shift register. This should read 64b, and we should be at
    // `byte offset` afterwards.
    size_t read_bytes = fread(&input_stream->shift_register.senior, sizeof(uint32_t), 1, input_stream->handle);
    if (read_bytes != 1) {
        REPORT("Error populating senior part of shift buffer " 
                "(read %li bytes instead of the expected %i).\n", 
                read_bytes, 1);
    }
    read_bytes = fread(&input_stream->shift_register.junior, sizeof(uint32_t), 1, input_stream->handle);
    if (read_bytes != 1) {
        REPORT("Error populating junior part of shift buffer " 
                "(read %li bytes instead of the expected %i).\n", 
                read_bytes, 1);
    }

    // Seeking counts as reading the bits, so we account for all the bits. We
    // oly ever seek form the beginning of the file, so this is easy.
    input_stream->read_bits = byte_offset * 8;

    // Move to bit offset
    for (int extra_bit = 0; extra_bit < remainder_bit_offset; extra_bit++) {
        int bit = FileBitStream_read_bit(input_stream);
        if (bit == -1) {
            REPORT("Seek error, unexpected EOF at %li.", 
                    byte_offset * 8 + extra_bit);
            return -1;
        }
        if (bit == -2) {
            REPORT("Seek error.");
            return -2;
        }        
    }    

    // Victory is ours.
    return 0;
}

void FileBitStream_free(FileBitStream *input_stream) {   
    // Close file associated with stream
    if (fclose(input_stream->handle) == EOF) {
        WARN("Failed to close file at %s.\n", input_stream->path); 
    }

    // Free memory
    free(input_stream);
}

// A structure representing blocks in a BZIP2 file. Records offset of beginning
// and end of each block.
typedef struct {
    const char *path;
    size_t blocks;
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
        REPORT("Cannot open file %s.\n", input_file_path);  
        return NULL;
    }

    size_t blocks = 0;
    uint64_t start_offset[MAX_BLOCKS];
    uint64_t end_offset[MAX_BLOCKS];

    // Initialize the counters.
    Blocks *boundaries = (Blocks *) malloc(sizeof(Blocks));    
    if (NULL == boundaries) {
        REPORT("Cannot allocate a struct for recording block boundaries.\n");  
        return NULL;
    }

    boundaries->path = input_file_path;
    boundaries->blocks = 0;
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
                    LOG("Block %li runs from %li to %li (incomplete)\n",
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

                LOG("Block %li runs from %li to %li\n",
                    boundaries->blocks + 1, 
                    start_offset[blocks], 
                    end_offset[blocks]);

                // Store the offsets
                boundaries->start_offset[boundaries->blocks] = 
                    start_offset[blocks];
                boundaries->end_offset[boundaries->blocks] = 
                    end_offset[blocks];

                // Increment number of complete blocks so far
                boundaries->blocks++;

                //start_offset[blocks] = input_stream->read_bits;
            }

            // Too many blocks
            if (blocks >= MAX_BLOCKS) {
                REPORT("analyzer can handle up to %i blocks, "
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
        REPORT("cannot allocate a bzip2 stream\n");  
        return NULL;
    }

    int result = BZ2_bzDecompressInit(stream, /*verbosity*/ 0, /*small*/ 0);
    if (result != BZ_OK) {        
        REPORT("cannot initialize a bzip2 stream\n");  
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

//     LOG("(Seeking %lib or %liB + %ib\n", bits, bytes, remainder);

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
        REPORT("cannot read from file.\n");
        return -1; 
    };        

    // Point the bzip decompressor at the newly read data.
    stream->avail_in = read_characters;
    stream->next_in = input_buffer;

    LOG("Read %i bytes from file: %s.\n", read_characters, stream->next_in);
    return read_characters;  
}

typedef struct {
    unsigned char   *buffer;
    size_t  size;
    size_t  max_size;
} Block;

int __block = 0;

void Block_free(Block *block) {
    LOG("Free %p\n", block);
    LOG("Free %p\n", block->buffer);
    free(block->buffer);
    free(block);
}

Block *Block_from(Blocks *boundaries, size_t index) {

    if (boundaries->blocks <= index) {
        REPORT("No block at index %li.\n", index);
        return NULL;
    }

    // Retrieve the offsets, for convenience
    const uint64_t block_start_offset_in_bits = boundaries->start_offset[index];
    const uint64_t block_end_offset_in_bits = boundaries->end_offset[index]; // Inclusive
    const uint64_t payload_size_in_bits = block_end_offset_in_bits - block_start_offset_in_bits + 1;
    const uint64_t header_end_offset_in_bits = boundaries->start_offset[0];

    LOG("Reading block %li from file %s between offsets %li and %li (%lib)\n",
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


    LOG("Preparing buffer of size %lib = %ib + %ib + %lib + %ib + %ib\n",
        buffer_size_in_bits, 32, 105, payload_size_in_bits, 80, padding);

    // Alloc the buffre for the chunk
    unsigned char *buffer = (unsigned char *) calloc(buffer_size_in_bytes, sizeof(unsigned char));
    memset(buffer, 0, buffer_size_in_bytes);

    // Open file for reading the remainder
    FileBitStream *input_stream = FileBitStream_new(boundaries->path);
    if (input_stream == NULL) {
        REPORT("cannot open bit stream for %s\n", boundaries->path);
        // TODO free what needs freeing
        return NULL;
    }

    // Read in everything before the first block boundary
    size_t read_bytes = 0;
    while (input_stream->read_bits < header_end_offset_in_bits) {
        int byte = FileBitStream_read_byte(input_stream);
        if (byte < 0) {
            REPORT("cannot read byte from bit stream\n");
            FileBitStream_free(input_stream);
            // TODO free what needs freeing
            return NULL;
        }
        buffer[read_bytes] = byte;
        read_bytes++;
    }

    // Seek to the block boundary
    int seek_result = FileBitStream_seek_bit(input_stream, block_start_offset_in_bits);
    if (seek_result < 0) {
        REPORT("cannot seek to %lib offset\n", block_start_offset_in_bits);
        return NULL;
    }

    // Read 32-bit CRC, which is just behind the boundary
    uint32_t block_crc = FileBitStream_read_uint32(input_stream);
    LOG("Block CRC %08x.\n", block_crc);
    if (block_crc < 0) {
        REPORT("Cannot read uint32 from bit stream.\n");
        FileBitStream_free(input_stream);
        // TODO free what needs freeing
        return NULL;
    }

    for (int i = sizeof(uint32_t) - 1; i >= 0; i--) {
        buffer[read_bytes++] = 0xff & (block_crc >> (8 * i));
    }
    
    // This is how much we can copy byte-by-byte before getting into trouble at
    // the end of the file. The remainder we have to copy bit-by-bit.
    size_t block_end_byte_aligned_offset_in_bits = (payload_size_in_bits & (~0x07)) + block_start_offset_in_bits;
    size_t remaining_byte_unaligned_bits = (payload_size_in_bits & (0x07));

    LOG("The block has to read until offset %lib = %lib + %lib. [%lib]\n", 
            block_end_offset_in_bits,
            block_end_byte_aligned_offset_in_bits, 
            remaining_byte_unaligned_bits,
            block_start_offset_in_bits);

    // Read until the last byte-aligned datum of the block
    while (input_stream->read_bits < block_end_byte_aligned_offset_in_bits) {
        int byte = FileBitStream_read_byte(input_stream);
        if (byte < 0) {
            REPORT("Cannot read byte from bit stream.\n");
            FileBitStream_free(input_stream);
            // TODO free what needs freeing
            return NULL;
        }
        buffer[read_bytes] = byte;
        read_bytes++;
    }

    // Ok, now we write the rest into a buffer to align it.
    BitBuffer *bit_buffer = BitBuffer_new(1 + footer_magic_size + 4); // TODO size
    for (int annoying_unaligned_bit = 0; 
         annoying_unaligned_bit < remaining_byte_unaligned_bits ; // sus
         annoying_unaligned_bit++) {

        // Read a bit from file, yay
        int bit = FileBitStream_read_bit(input_stream);
        if (bit < 0) {
            REPORT("cannot read bit from bit stream.\n");
            return NULL;
        }

        // Write that bit to buffer, yay
        int result = BitBuffer_append_bit(bit_buffer, (unsigned char) bit);
        if (result < 0) {
            REPORT("Cannot write bit to bit buffer.\n");
            return NULL;
        }
    }

    // Ok, now we write out the footer to the buffer, to align that.
    for (int footer_magic_byte = 0; 
         footer_magic_byte < footer_magic_size; 
         footer_magic_byte++) {

        int result = BitBuffer_append_byte(bit_buffer, footer_magic[footer_magic_byte]);
        if (result < 0) {
            REPORT("cannot write magic footer byte to bit buffer\n");
            return NULL;
        }
    }

    // Write out the block CRC value as the stream CRC value, since it's just
    // one block.
    int result = BitBuffer_append_uint32(bit_buffer, block_crc);
    if (result < 0) {
        REPORT("cannot write CRC byte to bit buffer\n");
        return NULL;
    }  

    // Write the bit buffer into the actual buffer.
    for (int i = 0; i < bit_buffer->max_bytes; i++) {
        buffer[read_bytes + i] = bit_buffer->data[i];
    }
    read_bytes += bit_buffer->max_bytes;

    // Return block data
    Block *block = malloc(sizeof(Block));
    block->buffer = buffer;
    block->size = read_bytes;
    block->max_size = buffer_size_in_bytes;

    // Free what needs freeing
    BitBuffer_free(bit_buffer);

    // Do not release buffer, because that's what we return. Make sure to free
    // it afterwards though.

    return block;
}

int Block_decompress(Block *block, size_t output_buffer_size, char *output_buffer) {

    // Hand-crank the BZip2 decompressor
    bz_stream *stream = bz_stream_init();
    if (stream == NULL) {
        REPORT("cannot initialize stream\n");  
        return -1;
    }         

    // Set array contents to zero, just in case.
    memset(output_buffer, 0x7c, output_buffer_size);

    // Tell BZip2 where to write decompressed data.
	stream->avail_out = output_buffer_size;
	stream->next_out = output_buffer;

    // Tell BZip2 where to getr input data from.
    stream->avail_in = block->size;
    stream->next_in  = (char *) block->buffer;

    // Do the do.
    int result = BZ2_bzDecompress(stream);

    LOG("Decompressed bytes (only 40 first): ");
    for (int i = 0; i < 40; i++) {
        LOG_SHORT("%02x ", output_buffer[i]);
    }
    LOG_SHORT("\n");

    if (result != BZ_OK && result != BZ_STREAM_END) { 
        REPORT("Cannot process stream, error no.: %i.\n", result);
        BZ2_bzDecompressEnd(stream);
        return -3;
    };

    if (result == BZ_OK 
        && stream->avail_in == 0 
        && stream->avail_out > 0) {
        BZ2_bzDecompressEnd(stream);
        REPORT("Cannot process stream, unexpected end of file.\n");        
        return -4; 
    };

    if (result == BZ_STREAM_END) {        
        LOG("Finshed processing stream.\n");  
        BZ2_bzDecompressEnd(stream); 
        return output_buffer_size - stream->avail_out;
    };

    if (stream->avail_out == 0) {   
        LOG("Stream ended: no data read.\n");  
        BZ2_bzDecompressEnd(stream);
        return output_buffer_size; 
    };

    // Supposedly unreachable.
    REPORT("Unreachable isn't.\n");  
    BZ2_bzDecompressEnd(stream);
    return 0;
}

void Blocks_free(Blocks *blocks) {
    // Nothing to free, 
    free(blocks);
}

Blocks *Blocks_new(char *filename) {
    // Parse the file.
    Blocks *blocks = Blocks_parse(filename);

    // Create the structures for outputting the decompressed data into
    // (temporarily).
    size_t output_buffer_size = 1024 * 1024 * 1024; // 1MB
    char *output_buffer = (char *) calloc(output_buffer_size, sizeof(char));
    
    // Unfortunatelly, we need to scan the entire file to figure out where the blocks go when decompressed.
    size_t end_of_last_decompressed_block = 0;
    for (size_t i = 0; i < blocks->blocks; i++) {

        // Extract a single compressed block.
        __block = i;
        Block *block = Block_from(blocks, i);        
        int output_buffer_occupancy = Block_decompress(block, output_buffer_size, output_buffer);
        LOG("Finished decompressing block %li, found %i elements\n", i, output_buffer_occupancy);   

        // Cleanup: we don't actually need the block for anything, except size.
        Block_free(block);

        if (output_buffer_occupancy < 0) {
            REPORT("Failed to decompress block %li, stopping\n", i);
            break;
        }

        // Calculate start and end of this block when it is decompressed.
        // TODO we could defer some of this.
        blocks->decompressed_start_offset[i] = end_of_last_decompressed_block + (0 == i ? 0 : 1);        
        blocks->decompressed_end_offset[i] = blocks->decompressed_start_offset[i] + output_buffer_occupancy;
        end_of_last_decompressed_block = blocks->decompressed_end_offset[i];        
    }

    // Cleanup
    free(output_buffer);

    // Add one, because end is inclusive.
    blocks->decompressed_size = end_of_last_decompressed_block + 1;
    return blocks;
}

static int32_t BZip2_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {

    Blocks *blocks = (Blocks *) user_data;

    for (size_t block = 0; block < blocks->blocks; block ++) {
        LOG("UFO checking block lengths: block %li : decompressed %li-%li [%li]\n", block,
            blocks->decompressed_start_offset[block], blocks->decompressed_end_offset[block], 
            blocks->decompressed_end_offset[block] - blocks->decompressed_start_offset[block] + 1);
    }

    size_t block_index = 0;
    bool found_start = false;

    // Ignore blocks that do not are not a part of the current segment.
    for (; block_index < blocks->blocks; block_index++) {
        if (start >= blocks->decompressed_start_offset[block_index] && start <= blocks->decompressed_end_offset[block_index]) { 
            found_start = true;
            LOG("UFO will start at block %li: start=%li <= decompressed_offset=%li\n", 
                block_index, start, blocks->decompressed_start_offset[block_index]);
            break;
        }
        LOG("UFO skips block %li: start=%li <= decompressed_offset=%li \n", 
            block_index, start, blocks->decompressed_start_offset[block_index]);
    }

    // The start index is not within any of the blocks?
    if (!found_start) {
        REPORT("Start index %li is not within any of the BZip2 blocks.\n", start); 
        return 1;
    }

    // At this point block index is the index of the block containing the start.
    // We calculate the bytes_to_skip: the offset of the start of the UFO segment with
    // respect to the first value in the Bzip2 block.
    size_t bytes_to_skip = start - (blocks->decompressed_start_offset[block_index]);
    LOG("UFO will skip %lu bytes of block %lu\n", bytes_to_skip, block_index);

    // The index in the target buffer.
    // size_t target_index = 0;

    // Temp buffers, because I don't know how to do it without it.
    size_t decompressed_buffer_size = DECOMPRESSED_BUFFER_SIZE; // 1MB
    char *decompressed_buffer = (char *) calloc(decompressed_buffer_size, sizeof(char));
    // memset(decompressed_buffer, 0x5c, decompressed_buffer_size); // FIXME remove

    // Check if we filled all the requested bytes from all the necessary BZip blocks.
    bool found_end = false;

    // The offset in target at which to paste the next decompressed BZip block.
    size_t offset_in_target = 0;

    // The number of bytes we still have to generate.
    size_t left_to_fill_in_target = end - start;

    // Decompress blocks, until required number of decompressed blocks produce
    // the prerequisite number of bytes of data.
    for (; block_index < blocks->blocks; block_index++) {

        // Retrive and decompress the block.
        LOG("UFO loads and decompresses block %li.\n", block_index);
        Block *block = Block_from(blocks, block_index);
        int decompressed_buffer_occupancy = Block_decompress(block, decompressed_buffer_size, decompressed_buffer);
        if (decompressed_buffer_occupancy <= 0) {
            LOG("UFO failed to decompress BZip.\n");
            return -1;
        } else {
            LOG("UFO retrieved %i elements by decompressing block %li.\n", 
                decompressed_buffer_occupancy, block_index);
        }
        size_t elements_to_copy = decompressed_buffer_occupancy - bytes_to_skip;
        LOG("UFO can retrieve %lu = %i - %li elements from BZip block "
            "and needs to grab %lu elements to fill the target location.\n", 
            elements_to_copy, decompressed_buffer_occupancy, bytes_to_skip, left_to_fill_in_target);
        if (elements_to_copy > left_to_fill_in_target) {            
            elements_to_copy = left_to_fill_in_target;
        }        
        LOG("UFO will grab %lu elements from BZip decompressed block to fill the target location.\n", 
            elements_to_copy);
        assert(decompressed_buffer_occupancy >= bytes_to_skip);

        // Copy the contents of the block to the target area. I can't do this
        // without this intermediate buffer, because the first block might need
        // to discard some number of bytes from the front. 
        // 
        // Also the last one has to disregards some number of bytes from the
        // back, but the block won't produce anything unless it's given room to
        // write out the whole block. <-- TODO: this needs verification
        LOG("UFO copies %lu elements from decompressed block %lu to target area %p = %p + %lu\n", 
            elements_to_copy, block_index, decompressed_buffer + bytes_to_skip, 
            decompressed_buffer, bytes_to_skip);
        memcpy(/* destination */ target + offset_in_target, 
               /* source      */ decompressed_buffer + bytes_to_skip, 
               /* elements    */ elements_to_copy); // TODO remove elements to make sure we don't load more than `end`

        // Start filling the target in the next iteration from the palce we
        // finished here.
        offset_in_target += elements_to_copy;

        // We copied these many elements already.
        left_to_fill_in_target -= elements_to_copy;     

        // Only the first block has bytes_to_skip != 0.
        bytes_to_skip = 0;   

        // Cleanup.
        Block_free(block);        

        if ((end - 1) >= blocks->decompressed_start_offset[block_index] 
            && (end - 1) <= blocks->decompressed_end_offset[block_index]) { 
            found_end = true;
            break;
        }
    }

    // Cleanup
    free(decompressed_buffer);

    // TODO check if found end
    if (!found_end) {
        REPORT("did not find a block containing the end of "
                "the segment range [%li-%li].\n", start, end);
        return 1;
    }

    return 0;
}

BZip2 *BZip2_ufo_new(UfoCore *ufo_system, char *filename, bool read_only) {
    
    Blocks *blocks = Blocks_new(filename);
    
    // Check for bad blocks
    if (blocks->bad_blocks > 0) {
        REPORT("UFO some blocks could not be read. Quitting.\n");
        Blocks_free(blocks);
        return NULL;
    }

    UfoParameters parameters;
    parameters.header_size = 0;
    parameters.element_size = strideOf(char);
    parameters.element_ct = blocks->decompressed_size;
    parameters.min_load_ct = MIN_LOAD_COUNT;
    parameters.read_only = read_only;
    parameters.populate_data = blocks;
    parameters.populate_fn = BZip2_populate;

    UfoObj ufo_object = ufo_new_object(ufo_system, &parameters);
    
    if (ufo_is_error(&ufo_object)) {
        REPORT("UFO object could not be created.\n");
    }

    BZip2 *object = (BZip2 *) malloc(sizeof(BZip2));
    object->data = ufo_header_ptr(&ufo_object);   
    object->size = blocks->decompressed_size;

    LOG("UFO created with length %li and payload at %p.\n", 
        object->size, object->data);
    
    return object;
}

void BZip2_ufo_free(UfoCore *ufo_system, BZip2 *object) {
    // Grab UFO object for this address.
    UfoObj ufo_object = ufo_get_by_address(ufo_system, object->data);
    if (ufo_is_error(&ufo_object)) {
        REPORT("Cannot free %p (%li): not a UFO object.\n", 
               object->data, object->size);
        return;
    }
    
    // Retrieve the parameters to finalize all the user objects within.
    UfoParameters parameters;
    int result = ufo_get_params(ufo_system, &ufo_object, &parameters);
    if (result < 0) {
        REPORT("Cannot free %p: unable to access UFO parameters.\n", object->data);
        ufo_free(ufo_object);
        return;
    }

    // Free the blocks struct
    Blocks *data = (Blocks *) parameters.populate_data;
    Blocks_free(data);

    // Free the actual object
    ufo_free(ufo_object);

    // Free the object wrapper struct;
    free(object);
}

BZip2 *BZip2_normil_new(char *filename) {
    FILE *stream = fopen(filename, "r");
    if (!stream) {
        REPORT("Cannot read file \"%s\"\n", filename);
        return NULL;
    }

    int error;
    BZFILE *bzip2_stream = BZ2_bzReadOpen(&error, stream, 0, 1, NULL, 0);
    if (error != BZ_OK) {
        BZ2_bzReadClose (&error, bzip2_stream);
        REPORT("Cannot process file \"%s\"\n", filename);
        return NULL;
    }

    size_t buffer_size = 2UL * 1000 * 1000 * 1000;
    char *buffer = (char *) malloc(sizeof(char) * buffer_size);
    size_t buffer_occupancy = 0;

    error = BZ_OK;
    while (error == BZ_OK) {
        printf("< %i", (int) (buffer_size - buffer_occupancy));
        int read_bytes = BZ2_bzRead(&error, bzip2_stream, (void *)(buffer + buffer_occupancy), (int) (buffer_size - buffer_occupancy));
        printf("> %i %i %i\n", read_bytes, error, error == BZ_OK);
        buffer_occupancy += read_bytes;
    }

    if (error != BZ_STREAM_END) {
        // BZ2_bzerror(bzip2_stream, &error);
        REPORT("Could not decompress file \"%s\"\n", filename);
        free(buffer);
        BZ2_bzReadClose(&error, bzip2_stream);
        return NULL;
    } 

    BZ2_bzReadClose(&error, bzip2_stream);

    char *data = (char *) realloc(buffer, buffer_occupancy);
    if (data == NULL) {
        REPORT("Could not compactify buffer from size %lu to size %lu\n", buffer_size, buffer_occupancy);
        free(buffer);
        return NULL;
    }
    
    BZip2 *bzip = malloc(sizeof(BZip2));
    bzip->data = data;
    bzip->size = buffer_occupancy;
    return bzip;
}

BZip2 *__BZip2_normil_new(char *filename) {
    Blocks *blocks = Blocks_new(filename);
    
    if (blocks->bad_blocks > 0) {
        REPORT("UFO some blocks could not be read. Quitting.\n");
        Blocks_free(blocks);
        return NULL;
    }

    unsigned char *data = (unsigned char *) malloc(sizeof(unsigned char) * blocks->decompressed_size);
    int result = BZip2_populate(blocks, 0, blocks->decompressed_size, data);
    if (result != 0){
        REPORT("UFO could not decompress data. Quitting.\n");
        free(data);
        return NULL;
    }

    BZip2 *bzip = malloc(sizeof(BZip2));
    bzip->data = (char *) data;
    bzip->size = blocks->decompressed_size;
    return bzip;
}

void BZip2_normil_free(BZip2 *object) {
    free(object->data);
    free(object);
}