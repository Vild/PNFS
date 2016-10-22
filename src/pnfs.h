#ifndef PNFS_H
#define PNFS_H

#include "fs.h"
#include "block.h"
#include "bd.h"

enum {
	PNFS_BLOCK_HEADER = 0,
	PNFS_BLOCK_NODE_FIRST = 1,
	PNFS_BLOCK_NODE_LAST = 16,
};

#define NODE_SIZE 64
struct pnfs_node {
	struct fs_node base;

#define sizeLeft (NODE_SIZE-sizeof(struct fs_node)+sizeof(void* /* vtbl */))
	fs_block_id dataBlocks[sizeLeft/sizeof(uint16_t) - 1];
	fs_block_id next;
#undef sizeLeft

	struct {
		struct pnfs_supernode * sn;
	} runtimeStorage;
};


// PNFS --> 0x504E4653
#define PNFS_MAGIC 0x53464E50
struct pnfs_supernode {
	struct fs_supernode base;
	uint32_t magic;

	uint8_t freeBlocksBitmap[32];
	struct {
		struct fs_blockdevice * bd;
	} runtimeStorage;
};

struct pnfs_supernode * pnfs_init(struct fs_blockdevice * bd);
#endif

/*
Block 0: Header
 - Magic 0 - 3
 - FreeBlocksBitmap 4-35

Block 1-16: Node x 8 // Total of 128 Nodes

Block 17: Root DirBlock
 - DirEntries x 8

*/
