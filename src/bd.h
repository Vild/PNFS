#ifndef BLOCKDEVICE_H
#define BLOCKDEVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "block.h"

#define BLOCKDEVICE_COUNT 250
typedef uint16_t fs_block_id;

struct fs_blockdevice {
	struct fs_block blocks[BLOCKDEVICE_COUNT];
};

void fs_blockdevice_clear(struct fs_blockdevice * bd);

bool fs_blockdevice_load(struct fs_blockdevice * bd, char * file);
bool fs_blockdevice_save(struct fs_blockdevice * bd, char * file);

void fs_blockdevice_read(struct fs_blockdevice * bd, fs_block_id idx, struct fs_block * block);
void fs_blockdevice_write(struct fs_blockdevice * bd, fs_block_id idx, struct fs_block * block);

#endif
