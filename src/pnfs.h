#ifndef PNFS_H
#define PNFS_H

#include "fs.h"
#include "block.h"
#include "bd.h"

/**
 * In which block info is stored
 */
enum {
	/// Header/Supernode block
	PNFS_BLOCK_HEADER = 0,
	/// Start of nodes block
	PNFS_BLOCK_NODE_FIRST = 1,
	/// Last node block
	PNFS_BLOCK_NODE_LAST = 16,
};

/**
 * The size of a node should be
 * \relates pnfs_node
 */
#define NODE_SIZE 64

/**
 * The amount of datablock a pnfs_node have.
 * \relates pnfs_node
 */
#define PNFS_NODE_BLOCKCOUNT (uint16_t)((NODE_SIZE-sizeof(struct fs_node)+sizeof(void*))/2 - 1)

/**
 * The nodestructure for the PowerNex FileSystem.
 * \relates fs_node
 */
struct pnfs_node {
	///The base pnfs_node extends
	struct fs_node base;


	/// The data block indices 
	fs_block_id dataBlocks[PNFS_NODE_BLOCKCOUNT];

	/// The index of a pnfs_blockBlock, when a file needs more block than there are in \ref dataBlocks
	fs_block_id next;

	/// Storage for runtime objects
	struct {
		/// Pointer to the supernode
		struct pnfs_supernode * sn;
	} runtimeStorage;
};


/**
 * The magic numer for PNFS.
 * You get the number 0x504E4653 from 'PNFS'
 * \relates pnfs_supernode
 */
#define PNFS_MAGIC 0x53464E50

/**
 * The supernode for PNFS.
 */
struct pnfs_supernode {
	/// The base pnfs_node extends
	struct fs_supernode base;

	/// The magic
	uint32_t magic;

	/// Bitmap for storing if a block is used or not
	uint8_t freeBlocksBitmap[32];

	/// Storage for runtime objects
	struct {
		/// Pointer to the blockdevice
		struct fs_blockdevice * bd;
	} runtimeStorage;
};

/**
 * Constructor for the pnfs_supernode.
 * \param bd The blockdevice from where to read the blocks
 * \return The pnfs_supernode instance
 * \relates pnfs_supernode
 */
struct pnfs_supernode * pnfs_init(struct fs_blockdevice * bd);
#endif
