#include <stdio.h>
#include <stdint.h>

#include "bitmap.h"
#include "block_store.h"
// include more if you need

struct block_store {
	uint8_t data[BLOCK_STORE_NUM_BYTES];
	bitmap_t *fbm;
};
// You might find this handy. I put it around unused parameters, but you should
// remove it before you submit. Just allows things to compile initially.
#define UNUSED(x) (void)(x)

block_store_t *block_store_create()
{
	block_store_t *bs = calloc(1,sizeof(block_store_t));
	if (bs == NULL) {
		return NULL;
	}
	bs->fbm = bitmap_overlay(BITMAP_SIZE_BITS, &bs->data[BITMAP_START_BLOCK * BLOCK_SIZE_BYTES]);

	if (bs->fbm == NULL) {
        free(bs);
        return NULL;
    }

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
	if(!bs) return SIZE_MAX;
	size_t firstZero = bitmap_ffz(bs->fbm);
	if(firstZero != SIZE_MAX){
		bitmap_set(bs->fbm, firstZero);
	}
	return firstZero;
}

bool block_store_request(block_store_t *const bs, const size_t block_id)
{
	if(!bs || !block_id || block_id >= bitmap_get_bits(bs->fbm)) return false;
	bool testBit = bitmap_test(bs->fbm, block_id);
	if(!testBit) {
		bitmap_set(bs->fbm, block_id);
		testBit = bitmap_test(bs->fbm, block_id);
		return testBit;
	} else {
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
	UNUSED(bs);
	return 0;
}

size_t block_store_get_free_blocks(const block_store_t *const bs)
{
	if(!bs) return NULL;

	size_t free_blocks;
	size_t used_blocks = block_store_get_used_blocks(bs);

	if(!used_blocks) return NULL;

	free_blocks = BLOCK_STORE_NUM_BLOCKS - used_blocks;
	
	return free_blocks;
}

size_t block_store_get_total_blocks()
{
	return BLOCK_STORE_NUM_BLOCKS;
}

size_t block_store_read(const block_store_t *const bs, const size_t block_id, void *buffer)
{
	UNUSED(bs);
	UNUSED(block_id);
	UNUSED(buffer);
	return 0;
}

size_t block_store_write(block_store_t *const bs, const size_t block_id, const void *buffer)
{
	UNUSED(bs);
	UNUSED(block_id);
	UNUSED(buffer);
	return 0;
}

block_store_t *block_store_deserialize(const char *const filename)
{
	UNUSED(filename);
	return NULL;
}

size_t block_store_serialize(const block_store_t *const bs, const char *const filename)
{
	UNUSED(bs);
	UNUSED(filename);
	return 0;
}
