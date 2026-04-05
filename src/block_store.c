#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "bitmap.h"
#include "block_store.h"
// include more if you need

// Block store struct with data array and FBM
struct block_store {
	uint8_t data[BLOCK_STORE_NUM_BYTES];
	bitmap_t *fbm;
};

// Creates a new block store and returns its pointer, bs
block_store_t *block_store_create()
{
	// Allocates memory and initializes to zero
	block_store_t *bs = calloc(1,sizeof(block_store_t));
	if (bs == NULL) {
		return NULL;
	}

	// Sets bitmap field to overlay of a bitmap with sizes defined in header file
	bs->fbm = bitmap_overlay(BITMAP_SIZE_BITS, &bs->data[BITMAP_START_BLOCK * BLOCK_SIZE_BYTES]);

	if (bs->fbm == NULL) {
        free(bs);
        return NULL;
    }

	// Marks blocks used by bitmap as allocated
	for (size_t i = 0; i < BITMAP_NUM_BLOCKS; ++i) {
        block_store_request(bs, BITMAP_START_BLOCK + i);
    }

	return bs;
}

void block_store_destroy(block_store_t *const bs)
{
	// Check that the block_store_t is not NULL:
	if (bs) {
		// Use bitmap function to free the bitmap:
		bitmap_destroy(bs->fbm);
		// Free the block store itself:
		free(bs);
	}
}

size_t block_store_allocate(block_store_t *const bs)
{
	// Check store is not null
	if(!bs) return SIZE_MAX;
	// Find first zero
	size_t firstZero = bitmap_ffz(bs->fbm);
	if(firstZero != SIZE_MAX){
		// If the first zero is valid, set it
		bitmap_set(bs->fbm, firstZero);
	}
	return firstZero;
}

bool block_store_request(block_store_t *const bs, const size_t block_id)
{
	// Check for valid parameters
	if(!bs || !block_id || block_id >= bitmap_get_bits(bs->fbm)) return false;
	bool testBit = bitmap_test(bs->fbm, block_id);
	// If the bit is not set
	if(!testBit) {
		// Set bit
		bitmap_set(bs->fbm, block_id);
		// Make sure set worked and return
		testBit = bitmap_test(bs->fbm, block_id);
		return testBit;
	} else {
		//return that the bit is already set
		return !testBit;
	}
}

void block_store_release(block_store_t *const bs, const size_t block_id)
{
	// Check that bs is not NULL and that block_id is within valid block indices.
	if (bs == NULL || block_id >= BLOCK_STORE_NUM_BLOCKS)
		return;
	
	// bitmap_reset doesn't check for the bitmap being NULL, so doing it here for safety
	if (bs->fbm != NULL)
		bitmap_reset(bs->fbm, block_id);
}

size_t block_store_get_used_blocks(const block_store_t *const bs)
{
	// Check for Block Store being NULL
	if(!bs)
		return SIZE_MAX;

	// Return the call to bitmap_total_set	
	return bitmap_total_set(bs->fbm);
}

size_t block_store_get_free_blocks(const block_store_t *const bs)
{
	// Check for valid bs
	if(!bs) return SIZE_MAX;

	// Check how many used blocks there are
	size_t free_blocks;
	size_t used_blocks = block_store_get_used_blocks(bs);

	if(!used_blocks) return SIZE_MAX;

	// Minus number of blocks from used blocks to get free blocks
	free_blocks = BLOCK_STORE_NUM_BLOCKS - used_blocks;
	
	return free_blocks;
}

// Returns total number of blocks
size_t block_store_get_total_blocks()
{
	return BLOCK_STORE_NUM_BLOCKS;
}

// Reads contents of a block store into a buffer and returns the number of bytes successfully read
size_t block_store_read(const block_store_t *const bs, const size_t block_id, void *buffer)
{
	// Parameter validation
	if (bs == NULL || buffer == NULL || block_id >= BLOCK_STORE_NUM_BLOCKS) {
                return 0;
        }

    size_t byte_offset = block_id * BLOCK_SIZE_BYTES;
	
	// Reads block store into a buffer and records number of bytes successfully read
    memcpy(buffer, &bs->data[byte_offset], BLOCK_SIZE_BYTES);

	// Returns number of bytes successfully read into buffer
    return BLOCK_SIZE_BYTES;
}

size_t block_store_write(block_store_t *const bs, const size_t block_id, const void *buffer)
{
	// Check that parameters are valid
	if(!bs || !buffer || block_id >= BLOCK_STORE_NUM_BLOCKS) return 0;

	// Create the spot to copy data to
	size_t byte_offset = block_id * BLOCK_SIZE_BYTES;

    memcpy(&bs->data[byte_offset], buffer, BLOCK_SIZE_BYTES);

    return BLOCK_SIZE_BYTES;
}

block_store_t *block_store_deserialize(const char *const filename)
{
	// Param Chaecking
	if (filename == NULL) return NULL;

	// Open the file to read.
	int fd = open(filename, O_RDONLY);
	if (fd < 0) return NULL;

	// Allocate the block_store_t pointer to return
	block_store_t *bs = malloc(sizeof(block_store_t));
	if (!bs) {
		close(fd);
		return NULL;
	}

	// Read in the data with checking the correct amount of bytes were read
	if (read(fd, bs->data, BLOCK_STORE_NUM_BYTES) != BLOCK_STORE_NUM_BYTES) {
		free(bs);
		close(fd);
		return NULL;
	}

	// Calculate the place in memory where the bitmap starts
	uint8_t *bitmap_ptr = &(bs->data[BITMAP_START_BLOCK * BLOCK_SIZE_BYTES]);

	// Import the data from the read using lib function
	bs->fbm = bitmap_import(BLOCK_STORE_NUM_BLOCKS, bitmap_ptr);
	

	if(!bs->fbm) {
		free(bs);
		close(fd);
		return NULL;
	}

	// Close the file stream and return
	close(fd);
	return bs;
}

// Serializes block store using POSIX system calls
size_t block_store_serialize(const block_store_t *const bs, const char *const filename)
{
	// Parameter validation
		if (bs == NULL || filename == NULL) {
			return 0;
		}

		// Opens file with POSIX interface, 0644 for read/write permissions
		int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd < 0) {
			return 0; 
		}

		// Writes block store data to file
		ssize_t bytes_written = write(fd, bs->data, BLOCK_STORE_NUM_BYTES);
		
		// Checks if write failed
		if (bytes_written < 0) {
			close(fd);
			return 0;
		}

		size_t current_file_size = (size_t)bytes_written;
		
		// Pads rest of file with zeros until at expected file size
		if (current_file_size < BLOCK_STORE_NUM_BYTES) {
			uint8_t zero_pad = 0;
			
			while (current_file_size < BLOCK_STORE_NUM_BYTES) {
				ssize_t pad_result = write(fd, &zero_pad, 1);
				
				if (pad_result > 0) {
					current_file_size += pad_result;
				} else {
					break; 
				}
			}
		}

		close(fd);

		return current_file_size;
}
