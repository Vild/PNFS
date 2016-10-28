#ifndef BLOCKDEVICE_H
#define BLOCKDEVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "block.h"

/**
 * The maximum amount of blocks.
 * \relates fs_blockdevice
 */
#define BLOCKDEVICE_COUNT 250

/**
 * The block index type.
 * \relates fs_blockdevice
 */
typedef uint16_t fs_block_id;

/**
 * Helper class for writing and reading blocks from the file.
 */
struct fs_blockdevice {
	/// The blocks
	struct fs_block blocks[BLOCKDEVICE_COUNT];
};


/**
 * This function sets all the blocks to zero, to emulate a format.
 * \param bd The blockdevice class instance
 * \relates fs_blockdevice
 */
void fs_blockdevice_clear(struct fs_blockdevice * bd);

/**
 * This functions loads all the block from a file on the host computer.
 * \param bd The blockdevice class instance
 * \param file The file on the host computer
 * \return If the load was successful
 * \relates fs_blockdevice
 */
bool fs_blockdevice_load(struct fs_blockdevice * bd, char * file);

/**
 * This functions saves all the block to a file on the host computer.
 * \param bd The blockdevice class instance
 * \param file The file on the host computer
 * \return If the save was successful
 * \relates fs_blockdevice
 */
bool fs_blockdevice_save(struct fs_blockdevice * bd, char * file);


/**
 * This functions reads a block at the index \a idx from the blockdevice and writes the data to \a block.
 * \param bd The blockdevice class instance
 * \param idx The blocks index
 * \param block Where to write the block to
 * \relates fs_blockdevice
 * \relates fs_block
 */
void fs_blockdevice_read(struct fs_blockdevice * bd, fs_block_id idx, struct fs_block * block);

/**
 * This functions writes the block \a block to the index \a idx in the blockdevice.
 * \param bd The blockdevice class instance
 * \param idx The blocks index
 * \param block The block
 * \relates fs_blockdevice
 * \relates fs_block
 */
void fs_blockdevice_write(struct fs_blockdevice * bd, fs_block_id idx, struct fs_block * block);

#endif
