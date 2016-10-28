#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>

/**
 * The size of each block.
 * \relates fs_block
 */
#define BLOCK_SIZE 512

/**
 * The block representation struct.
 * It contains a byte array that define how big each block is.
 */
struct fs_block {
	/// The data
	uint8_t data[BLOCK_SIZE];
};

#endif
