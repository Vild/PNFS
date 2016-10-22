#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>

#define BLOCK_SIZE 512

struct fs_block {
	uint8_t data[BLOCK_SIZE];
};

#endif
