#include "pnfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "fs_supernode.h"

#define min(x_, y_) ({													\
			__typeof__ (x_) x = (x_);									\
			__typeof__ (y_) y = (y_);									\
			(void)(&x == &y);													\
			x < y ? x : y; })

// VTables functions
static struct fs_node * pnfs_supernode_getNode(struct fs_supernode * sn, fs_node_id id);
static void pnfs_supernode_saveNode(struct fs_supernode * sn, struct fs_node * node);

static struct fs_node * pnfs_supernode_addNode(struct fs_supernode * sn, struct fs_node * parent, enum fs_node_type type, const char * name);
static bool pnfs_supernode_removeNode(struct fs_supernode * sn, struct fs_node * parent, fs_node_id id);

static fs_node_id pnfs_supernode_getFreeNodeID(struct fs_supernode * sn);
static fs_block_id pnfs_supernode_getFreeBlockID(struct fs_supernode * sn);

static void pnfs_supernode_setBlockUsed(struct fs_supernode * sn, fs_block_id id);
static void pnfs_supernode_setBlockFree(struct fs_supernode * sn, fs_block_id id);

static uint16_t pnfs_node_readData(struct fs_node * node, void * buffer, uint16_t offset, uint16_t size);
static uint16_t pnfs_node_writeData(struct fs_node * node, const void * buffer, uint16_t offset, uint16_t size);

static struct fs_direntry * pnfs_node_directoryEntries(struct fs_node * node, uint16_t * amount);
static struct fs_node * pnfs_node_findNode(struct fs_node * node, const char * path);

static char * pnfs_node_getName(struct fs_node * node, struct fs_node * parent);
static struct fs_node * pnfs_node_getParent(struct fs_node * node);

// VTables
static struct fs_supernode_vtbl pnfs_supernode_vtbl = {
	.getNode = &pnfs_supernode_getNode,
	.saveNode = &pnfs_supernode_saveNode,

	.addNode = &pnfs_supernode_addNode,
	.removeNode = &pnfs_supernode_removeNode,

	.getFreeNodeID = &pnfs_supernode_getFreeNodeID,
	.getFreeBlockID = &pnfs_supernode_getFreeBlockID,

	.setBlockUsed = &pnfs_supernode_setBlockUsed,
	.setBlockFree = &pnfs_supernode_setBlockFree
};

static struct fs_node_vtbl pnfs_node_vtbl = {
	.readData = &pnfs_node_readData,
	.writeData = &pnfs_node_writeData,

	.directoryEntries = &pnfs_node_directoryEntries,
	.findNode = &pnfs_node_findNode,

	.getName = &pnfs_node_getName,
	.getParent = &pnfs_node_getParent
};

// Local helper type

/**
 * Helper structure reference for how the node blocks should look like
 */
struct pnfs_nodeBlock { // This is to help with the reading
	/// 8 nodes in raw form
	struct {
		uint8_t _[sizeof(struct pnfs_node) - sizeof(void * /* Vtbl */)-sizeof(((struct pnfs_node *)NULL)->runtimeStorage)];
	} blocks[8];
};



/**
 * The amount of datablock a pnfs_blockBlock have.
 * \relates pnfs_blockBlock
 */
#define PNFS_BLOCKBLOCK_BLOCKCOUNT (uint16_t)(NODE_SIZE/2 - 1)

/**
 * Helper struct for handling block blocks.
 * This is what nodes reference in the next pointer.
 */
struct pnfs_blockBlock {
	/// Extra data blocks
	fs_block_id dataBlocks[PNFS_BLOCKBLOCK_BLOCKCOUNT];
	/// If the node requires yet another pnfs_blockBlock
	fs_block_id next;
};

// Local functions
static struct pnfs_supernode * pnfs_initFS(struct fs_blockdevice * bd, struct pnfs_supernode * sn);
static void pnfs_insertDirEntry(struct pnfs_node * node, struct fs_direntry * entry);
static void pnfs_removeDirEntry(struct pnfs_node * node, fs_node_id id);

static void pnfs_addBlock(struct pnfs_node * node); /// Add one block
static void pnfs_removeBlocks(struct pnfs_node * node); /// Remove all unneeded blocks (Based on size)

// Code
struct pnfs_supernode * pnfs_init(struct fs_blockdevice * bd) {
	struct fs_block block;
	struct pnfs_supernode * sn = malloc(sizeof(struct pnfs_supernode));
	fs_blockdevice_read(bd, PNFS_BLOCK_HEADER, &block);
	memcpy(((void *)sn) + sizeof(void *), &block, sizeof(struct pnfs_supernode)-sizeof(void * /*vtbl*/)-sizeof(sn->runtimeStorage));
	sn->base.vtbl = &pnfs_supernode_vtbl;
	sn->runtimeStorage.bd = bd;

	if (sn->magic != PNFS_MAGIC) {
		printf("[-] No PNFS found on disk!\n");
		sn = pnfs_initFS(bd, sn);
	}

	printf("[+] Loaded PNFS correctly!\n");

	(void)pnfs_removeBlocks;

	return sn;
}


struct pnfs_supernode * pnfs_initFS(struct fs_blockdevice * bd, struct pnfs_supernode * sn) {
	printf("[*] Initializing filesystem...\n");

	sn->magic = PNFS_MAGIC;

	// Setup freeBlocksBitmap
	printf("[*] Initializing blocks...\n");
	memset(sn->freeBlocksBitmap, 0, sizeof(sn->freeBlocksBitmap));
	{
		struct fs_block block;
		memset(&block, 0, sizeof(struct fs_block));
		memcpy(&block, ((void *)sn) + sizeof(void *), sizeof(struct pnfs_supernode) - sizeof(void *) - sizeof(sn->runtimeStorage));
		fs_blockdevice_write(bd, PNFS_BLOCK_HEADER, &block);
	}
	fs_supernode_setBlockUsed((struct fs_supernode *)sn, PNFS_BLOCK_HEADER);

	for (fs_block_id b = PNFS_BLOCK_NODE_FIRST; b <= PNFS_BLOCK_NODE_LAST; b++)
		fs_supernode_setBlockUsed((struct fs_supernode *)sn, b);

	// Setup nodes
	printf("[*] Initializing nodes...\n");
	struct pnfs_nodeBlock emptyNodeBlock;
	memset(&emptyNodeBlock, 0, sizeof(struct pnfs_nodeBlock));
	for (int i = 0; i < 8; i++) {
		struct pnfs_node * node = (struct pnfs_node *) (&emptyNodeBlock.blocks[i] - sizeof(void* /* VTbl */));
		node->base.type = NODETYPE_INVALID;
		node->base.size = 0;
		node->base.blockCount = 0;
	}

	for (fs_block_id b = PNFS_BLOCK_NODE_FIRST; b <= PNFS_BLOCK_NODE_LAST; b++)
		fs_blockdevice_write(bd, b, (struct fs_block *)&emptyNodeBlock);


	printf("[*] \tCreating NODE_INVALID...\n");
	{
		struct pnfs_node * node = (struct pnfs_node *)fs_supernode_getNode((struct fs_supernode *)sn, NODE_INVALID);
		node->base.id = NODE_INVALID;
		node->base.type = NODETYPE_NEVER_VALID;
		fs_supernode_saveNode((struct fs_supernode *)sn, (struct fs_node *)node);
		free(node);
	}

	printf("[*] \tCreating NODE_ROOT...\n");
	{
		struct pnfs_node * node = (struct pnfs_node *)fs_supernode_getNode((struct fs_supernode *)sn, NODE_ROOT);
		node->base.id = NODE_ROOT;
		node->base.type = NODETYPE_DIRECTORY;
		node->base.size = sizeof(struct fs_direntry) * 2;
		node->base.blockCount = 1;
		fs_block_id id = node->dataBlocks[0] = fs_supernode_getFreeBlockID((struct fs_supernode *)sn);
		fs_supernode_setBlockUsed((struct fs_supernode *)sn, id);
		fs_supernode_saveNode((struct fs_supernode *)sn, (struct fs_node *)node);
		free(node);

		struct fs_direntry entries[8];
		memset(entries, 0, sizeof(entries));
		entries[0].id = NODE_ROOT;
		strncpy(entries[0].name, ".", sizeof(entries[0].name));

		entries[1].id = NODE_ROOT;
		strncpy(entries[1].name, "..", sizeof(entries[1].name));
		fs_blockdevice_write(bd, id, (struct fs_block *)&entries);
	}

	printf("[+] Creation done!\n");

	return sn;
}

static struct fs_node * pnfs_supernode_getNode(struct fs_supernode * sn_, fs_node_id id) {
	struct pnfs_supernode * sn = (struct pnfs_supernode *)sn_;
	struct pnfs_node * node = malloc(sizeof(struct pnfs_node));

	node->base.vtbl = &pnfs_node_vtbl;
	node->runtimeStorage.sn = sn;

	struct pnfs_nodeBlock block;
	fs_blockdevice_read(sn->runtimeStorage.bd, id / 8 + 1, (struct fs_block *)&block);

	memcpy((void *)node + sizeof(void *), &(block.blocks[id%8]), sizeof(struct pnfs_node) - sizeof(void *)-sizeof(sn->runtimeStorage));
	return (struct fs_node *)node;
}


static void pnfs_supernode_saveNode(struct fs_supernode * sn_, struct fs_node * node) {
	struct pnfs_supernode * sn = (struct pnfs_supernode *)sn_;
	struct pnfs_nodeBlock block;
	fs_blockdevice_read(sn->runtimeStorage.bd, node->id / 8 + 1, (struct fs_block *)&block);
	memcpy(&block.blocks[node->id % 8], (void *)node + sizeof(void *), sizeof(struct pnfs_node) - sizeof(void *)-sizeof(sn->runtimeStorage));
	fs_blockdevice_write(sn->runtimeStorage.bd, node->id / 8 + 1, (struct fs_block *)&block);
}

static struct fs_node * pnfs_supernode_addNode(struct fs_supernode * sn, struct fs_node * parent, enum fs_node_type type, const char * name) {
	struct fs_blockdevice * bd = ((struct pnfs_supernode *)sn)->runtimeStorage.bd;
	fs_node_id id = fs_supernode_getFreeNodeID(sn);

	if (id == NODE_INVALID) {
		printf("[-] No more free nodes\n");
		return NULL;
	}

	struct pnfs_node * node = (struct pnfs_node *)fs_supernode_getNode((struct fs_supernode *)sn, id);
	node->runtimeStorage.sn = (struct pnfs_supernode *)sn;

	if (type == NODETYPE_FILE) {
		node->base.id = id;
		node->base.type = NODETYPE_FILE;
		node->base.size = 0;
		node->base.blockCount = 0;
		fs_supernode_saveNode((struct fs_supernode *)sn, (struct fs_node *)node);
	} else if (type == NODETYPE_DIRECTORY) {
		node->base.id = id;
		node->base.type = NODETYPE_DIRECTORY;
		node->base.size = sizeof(struct fs_direntry) * 2;
		node->base.blockCount = 1;
		fs_block_id blockID = node->dataBlocks[0] = fs_supernode_getFreeBlockID(sn);
		fs_supernode_setBlockUsed(sn, blockID);
		fs_supernode_saveNode((struct fs_supernode *)sn, (struct fs_node *)node);

		struct fs_direntry entries[8];
		memset(entries, 0, sizeof(entries));
		entries[0].id = id;
		strncpy(entries[0].name, ".", sizeof(entries[0].name));

		entries[1].id = parent->id;
		strncpy(entries[1].name, "..", sizeof(entries[1].name));
		fs_blockdevice_write(bd, blockID, (struct fs_block *)&entries);
	} else {
		free(node);
		return NULL;
	}

	struct fs_direntry entry;
	entry.id = id;
	strncpy(entry.name, name, sizeof(entry.name));
	pnfs_insertDirEntry((struct pnfs_node *)parent, &entry);
	return (struct fs_node *)node;
}

static bool pnfs_supernode_removeNode(struct fs_supernode * sn, struct fs_node * parent, fs_node_id id) {
	if (parent->id == id) // Trying to remove '.'
		return false;
	struct pnfs_node * node = (struct pnfs_node *)fs_supernode_getNode(sn, id);
	if (node->base.type == NODETYPE_FILE) {
		for (int i = 0; i < PNFS_NODE_BLOCKCOUNT; i++) {
			if (!node->dataBlocks[i])
				continue;
			fs_supernode_setBlockFree(sn, node->dataBlocks[i]);
			node->dataBlocks[i] = 0;
		}

		fs_block_id blockBlockID = node->next;
		struct pnfs_blockBlock blockBlock;
		struct fs_blockdevice * bd = ((struct pnfs_supernode *)sn)->runtimeStorage.bd;

		while (blockBlockID) {
			fs_blockdevice_read(bd, blockBlockID, (struct fs_block *)&blockBlock);

			for (int i = 0; i < PNFS_BLOCKBLOCK_BLOCKCOUNT; i++) {
				if (!node->dataBlocks[i])
					continue;
				fs_supernode_setBlockFree(sn, node->dataBlocks[i]);
				node->dataBlocks[i] = 0;
			}

			fs_supernode_setBlockFree(sn, blockBlockID);

			blockBlockID = blockBlock.next;
		}
	}	else if (node->base.type == NODETYPE_DIRECTORY) {
		uint16_t amount;
		struct fs_direntry * dir = fs_node_directoryEntries((struct fs_node *)node, &amount);
		if (dir) {
			for (uint16_t i = 0; i < amount; i++) {
				if (!strcmp(dir[i].name, ".") || !strcmp(dir[i].name, ".."))
					continue;
				fs_supernode_removeNode(sn, (struct fs_node *)node, dir[i].id);
			}
			free(dir);
		}
	}

	pnfs_removeDirEntry((struct pnfs_node *)parent, id);

	node->base.type = NODETYPE_INVALID;
	node->base.size = 0;
	node->base.blockCount = 0;

	fs_supernode_saveNode(sn, (struct fs_node *)node);
	free(node);

	parent->size -= sizeof(struct fs_direntry);
	fs_supernode_saveNode(sn, parent);
	return true;
}

static fs_node_id pnfs_supernode_getFreeNodeID(struct fs_supernode * sn) {
	for (fs_node_id i = 0; i < 250; i++) {
		struct fs_node * node = fs_supernode_getNode(sn, i);
		if (node->type == NODETYPE_INVALID) {
			free(node);
			return i;
		}
		free(node);
	}
	return NODE_INVALID;
}


static fs_block_id pnfs_supernode_getFreeBlockID(struct fs_supernode * sn_) {
	struct pnfs_supernode * sn = (struct pnfs_supernode *)sn_;
	for (int i = 0; i < 32; i++)
		if (sn->freeBlocksBitmap[i] != 0xFF) {
			uint8_t row = sn->freeBlocksBitmap[i];
			for (int j = 0; j < 8; j++)
				if (!(row & (1 << j)))
					return i * 8 + j;
		}
	return -1;
}

static void pnfs_supernode_setBlockUsed(struct fs_supernode * sn_, fs_block_id id) {
	struct pnfs_supernode * sn = (struct pnfs_supernode *)sn_;
	sn->freeBlocksBitmap[id/8] |= 1 << (id % 8);
	struct fs_block block;
	memset(&block, 0, sizeof(struct fs_block));
	memcpy(&block, ((void *)sn) + sizeof(void *), sizeof(struct pnfs_supernode) - sizeof(void *) - sizeof(sn->runtimeStorage));
	fs_blockdevice_write(sn->runtimeStorage.bd, PNFS_BLOCK_HEADER, &block);
}

static void pnfs_supernode_setBlockFree(struct fs_supernode * sn_, fs_block_id id) {
	struct pnfs_supernode * sn = (struct pnfs_supernode *)sn_;
	sn->freeBlocksBitmap[id/8] &= ~(1 << (id % 8));

	struct fs_block block;
	memset(&block, 0, sizeof(struct fs_block));
	memcpy(&block, ((void *)sn) + sizeof(void *), sizeof(struct pnfs_supernode) - sizeof(void *) - sizeof(sn->runtimeStorage));
	fs_blockdevice_write(sn->runtimeStorage.bd, PNFS_BLOCK_HEADER, &block);
}

static uint16_t pnfs_node_readData(struct fs_node * node_, void * buffer, uint16_t offset, uint16_t size) {
	uint16_t read = 0;
	struct pnfs_node * node = (struct pnfs_node *)node_;
	if (offset > node->base.size)
		return 0;

	struct fs_blockdevice * bd = node->runtimeStorage.sn->runtimeStorage.bd;

	for (uint16_t i = 0; i < PNFS_NODE_BLOCKCOUNT && size; i++) {
		fs_block_id bid = node->dataBlocks[i];
		if (!bid)
			return read;

		if (offset >= sizeof(struct fs_block)) {
			offset -= sizeof(struct fs_block);
			continue;
		}

		struct fs_block block;
		fs_blockdevice_read(bd, bid, &block);

		uint16_t readAmount = min((uint16_t)(sizeof(struct fs_block) - offset), size);

		memcpy(buffer + read, ((void*)&block) + offset, readAmount);

		size -= readAmount;
		read += readAmount;
		offset = 0;
	}

	fs_block_id nextBlockID = node->next;

	while (size && nextBlockID) {
		struct pnfs_blockBlock blockBlock;
		fs_blockdevice_read(bd, nextBlockID, (struct fs_block *)&blockBlock);

		for (uint16_t i = 0; i < PNFS_BLOCKBLOCK_BLOCKCOUNT && size; i++) {
			fs_block_id bid = blockBlock.dataBlocks[i];
			if (!bid)
				return read;

			if (offset >= sizeof(struct fs_block)) {
				offset -= sizeof(struct fs_block);
				continue;
			}

			struct fs_block block;
			fs_blockdevice_read(bd, bid, &block);

			uint16_t readAmount = min((uint16_t)(sizeof(struct fs_block) - offset), size);

			memcpy(buffer + read, ((void*)&block)+offset, readAmount);

			size -= readAmount;
			read += readAmount;
			offset = 0;
		}

		nextBlockID = blockBlock.next;
	}

	return read;
}

static uint16_t pnfs_node_writeData(struct fs_node * node_, const void * buffer, uint16_t offset, uint16_t size) {
	uint16_t wrote = 0;
	struct pnfs_node * node = (struct pnfs_node *)node_;

	uint16_t neededBlocks = (offset+size + sizeof(struct fs_block) - 1) / sizeof(struct fs_block);
	while (node->base.blockCount < neededBlocks)
		pnfs_addBlock(node);

	if (node->base.size < offset + size)
		node->base.size = offset + size;

	struct fs_blockdevice * bd = node->runtimeStorage.sn->runtimeStorage.bd;

	for (uint16_t i = 0; i < PNFS_NODE_BLOCKCOUNT && size; i++) {
		fs_block_id bid = node->dataBlocks[i];
		if (!bid) {
			printf("[-] Need more blocks for file\n");
			goto ret;
		}

		if (offset >= sizeof(struct fs_block)) {
			offset -= sizeof(struct fs_block);
			continue;
		}

		struct fs_block block;
		fs_blockdevice_read(bd, bid, &block);

		uint16_t writeAmount = min((uint16_t)(sizeof(struct fs_block) - offset), size);

		memcpy(((void*)&block) + offset, buffer + wrote, writeAmount);

		fs_blockdevice_write(bd, bid, &block);

		size -= writeAmount;
		wrote += writeAmount;
		offset = 0;
	}

	fs_block_id nextBlockID = node->next;

	while (size && nextBlockID) {
		struct pnfs_blockBlock blockBlock;
		fs_blockdevice_read(bd, nextBlockID, (struct fs_block *)&blockBlock);

		for (uint16_t i = 0; i < PNFS_BLOCKBLOCK_BLOCKCOUNT && size; i++) {
			fs_block_id bid = blockBlock.dataBlocks[i];
			if (!bid) {
				printf("[-] Need more blocks for filesdasd\n");
				goto ret;
			}

			if (offset >= sizeof(struct fs_block)) {
				offset -= sizeof(struct fs_block);
				continue;
			}

			struct fs_block block;
			fs_blockdevice_read(bd, bid, &block);

			uint16_t writeAmount = min((uint16_t)(sizeof(struct fs_block) - offset), size);

			memcpy(((void*)&block) + offset, buffer + wrote, writeAmount);

			fs_blockdevice_write(bd, bid, &block);

			size -= writeAmount;
			wrote += writeAmount;
			offset = 0;
		}

		nextBlockID = blockBlock.next;
	}

ret:
	fs_supernode_saveNode((struct fs_supernode *)node->runtimeStorage.sn, node_);

	return wrote;
}


static struct fs_direntry * pnfs_node_directoryEntries(struct fs_node * node_, uint16_t * amount) {
	struct pnfs_node * node = (struct pnfs_node *)node_;
	if (node->base.type != NODETYPE_DIRECTORY)
		goto error;

	struct fs_blockdevice * bd = node->runtimeStorage.sn->runtimeStorage.bd;

	int count = node->base.size / sizeof(struct fs_direntry);
	*amount = count;
	int added = 0;

	struct fs_direntry * dir = malloc(node->base.size);

	const uint16_t blocksInNode = PNFS_NODE_BLOCKCOUNT;

	for (int i = 0; i < min(node->base.blockCount, blocksInNode); i++) {
		struct fs_direntry entries[8];
		fs_blockdevice_read(bd, node->dataBlocks[i], (struct fs_block *)&entries);
		for (int j = 0; j < 8 && added < count; j++)
			memcpy(&dir[added++], &entries[j], sizeof(struct fs_direntry));
	}

	if (added < count) {
		struct pnfs_blockBlock block;
		const int blocksInBlockBlock = sizeof(block.dataBlocks) / sizeof(fs_block_id);

		fs_blockdevice_read(bd, node->next, (struct fs_block *)&block);

		int blocksLeft = node->base.blockCount - blocksInNode;
		while (added < count) {
			for (int i = 0; i < min(blocksLeft, blocksInBlockBlock); i++) {
				struct fs_direntry entries[8];
				fs_blockdevice_read(bd, block.dataBlocks[i], (struct fs_block *)&entries);
				for (int j = 0; j < 8 && added < count; j++)
					memcpy(&dir[added], &entries[j], sizeof(struct fs_direntry));
			}
			blocksLeft -= blocksInBlockBlock;
			if (blocksLeft > 0)
				fs_blockdevice_read(bd, block.next, (struct fs_block *)&block);
		}
	}
	return dir;

error:
	amount = 0;
	return NULL;
}

static void pnfs_insertDirEntry(struct pnfs_node * node, struct fs_direntry * entry) {
	struct pnfs_supernode * sn = node->runtimeStorage.sn;
	struct fs_blockdevice * bd = sn->runtimeStorage.bd;
	uint16_t dirPos = node->base.size / sizeof(struct fs_direntry); // What index it has in dirEntries
	uint16_t inBlockIdx = dirPos / 8; // What block it is in

	struct fs_direntry entries[8]; // fs_block
	fs_block_id blockID; // the blockid for the block to write to

	if (inBlockIdx < PNFS_NODE_BLOCKCOUNT) { // Is inBlockId in the node?
		if (inBlockIdx < node->base.blockCount) { // Block is already allocated
			blockID = node->dataBlocks[inBlockIdx];
		}	else { // Allocate needed
			blockID = node->dataBlocks[inBlockIdx] = fs_supernode_getFreeBlockID((struct fs_supernode *)sn);
			fs_supernode_setBlockUsed((struct fs_supernode *)sn, blockID);
			node->base.blockCount++;
			fs_supernode_saveNode((struct fs_supernode *)sn, (struct fs_node *)node);

			struct fs_block block;
			memset(&block, 0, sizeof(struct fs_block));
			fs_blockdevice_write(bd, blockID, &block);
		}
	} else {
		struct pnfs_blockBlock blockBlock;
		// Need to add another block?
		if (!node->next) { // We need to allocate .next --> .next.block[0] is the entry that is neede
			fs_block_id blockBlockIdx = fs_supernode_getFreeBlockID((struct fs_supernode *)sn);
			fs_supernode_setBlockUsed((struct fs_supernode *)sn, blockBlockIdx);
			node->next = blockBlockIdx;
			node->base.blockCount++;
			fs_supernode_saveNode((struct fs_supernode *)sn, (struct fs_node *)node);

			memset(&blockBlock, 0, sizeof(struct pnfs_blockBlock));
			blockID = blockBlock.dataBlocks[0] = fs_supernode_getFreeBlockID((struct fs_supernode *)sn);
			fs_supernode_setBlockUsed((struct fs_supernode *)sn, blockID);
			fs_blockdevice_write(bd, blockBlockIdx, (struct fs_block *)&blockBlock);
		} else {
			fs_blockdevice_read(bd, node->next, (struct fs_block *)&blockBlock);

			uint16_t walker = inBlockIdx - (PNFS_NODE_BLOCKCOUNT);
			uint16_t walkNextCount = walker / (PNFS_BLOCKBLOCK_BLOCKCOUNT);

			fs_block_id blockBlockIdx = node->next;
			for (uint16_t i = 0; i < walkNextCount; i++) {
				if (!blockBlock.next) { // Need to add block, should be last
					struct pnfs_blockBlock tmp;
					memset(&tmp, 0, sizeof(struct pnfs_blockBlock));
					fs_block_id newBlock = blockBlock.next = fs_supernode_getFreeBlockID((struct fs_supernode *)sn);
					fs_supernode_setBlockUsed((struct fs_supernode *)sn, newBlock);
					fs_blockdevice_write(bd, blockBlockIdx, (struct fs_block *)&blockBlock);
					blockBlockIdx = newBlock;
					break;
				}

				blockBlockIdx = blockBlock.next;
				fs_blockdevice_read(bd, blockBlockIdx, (struct fs_block *)&blockBlock);
			}

			uint16_t idx = walker % (PNFS_BLOCKBLOCK_BLOCKCOUNT);
			if (inBlockIdx < node->base.blockCount)
				blockID = blockBlock.dataBlocks[idx];
			else { // Allocate needed
				blockID = blockBlock.dataBlocks[idx] = fs_supernode_getFreeBlockID((struct fs_supernode *)sn);
				fs_supernode_setBlockUsed((struct fs_supernode *)sn, blockID);
				node->base.blockCount++;
				fs_supernode_saveNode((struct fs_supernode *)sn, (struct fs_node *)node);

				struct fs_block block;
				memset(&block, 0, sizeof(struct fs_block));
				fs_blockdevice_write(bd, blockID, &block);
			}
		}
	}

	fs_blockdevice_read(bd, blockID, (struct fs_block *)&entries);
	memcpy(&entries[dirPos%8], entry, sizeof(struct fs_direntry));
	fs_blockdevice_write(bd, blockID, (struct fs_block *)&entries);
	node->base.size += sizeof(struct fs_direntry);
	fs_supernode_saveNode((struct fs_supernode *)sn, (struct fs_node *)node);
}

static void pnfs_removeDirEntry(struct pnfs_node * node, fs_node_id id) {
	struct fs_blockdevice * bd = node->runtimeStorage.sn->runtimeStorage.bd;

	fs_block_id curBlock = 0;
	struct fs_direntry curBlockData[8];

	fs_block_id oldBlock = 0;
	struct fs_direntry oldBlockData[8];

	if (node->base.type != NODETYPE_DIRECTORY)
		return;

	for (int i = 0; i < PNFS_NODE_BLOCKCOUNT; i++) {
		curBlock = node->dataBlocks[i];
		if (!curBlock)
			return;

		fs_blockdevice_read(bd, curBlock, (struct fs_block *)&curBlockData);

		if (oldBlock) {
			memcpy(&oldBlockData[7], &curBlockData[0], sizeof(struct fs_direntry));
			fs_blockdevice_write(bd, oldBlock, (struct fs_block *)&oldBlockData);

			// Shift 1 step right
			memmove(&curBlockData[0], &curBlockData[1], sizeof(struct fs_direntry) * 7);
			fs_blockdevice_write(bd, curBlock, (struct fs_block *)&curBlockData);

			// Setup for next move
			oldBlock = curBlock;
			memcpy(&oldBlockData, &curBlockData, sizeof(curBlockData));
		} else
			for (int j = 0; j < 8; j++) {
				if (curBlockData[j].id == id) {
					if (j < 7) {
						memmove(&curBlockData[j], &curBlockData[j+1], sizeof(struct fs_direntry) * (7 - j));
						fs_blockdevice_write(bd, curBlock, (struct fs_block *)&curBlockData);
					}
					oldBlock = curBlock;
					memcpy(&oldBlockData, &curBlockData, sizeof(curBlockData));
				}
			}
	}


	struct pnfs_blockBlock blockBlock;
	blockBlock.next = node->next; // Hack ;)

	while (blockBlock.next) {
		fs_blockdevice_read(bd, blockBlock.next, (struct fs_block *)&blockBlock);
		for (int i = 0; i < sizeof(blockBlock.dataBlocks)/sizeof(fs_block_id); i++) {
			curBlock = blockBlock.dataBlocks[i];
			if (!curBlock)
				return;

			fs_blockdevice_read(bd, curBlock, (struct fs_block *)&curBlockData);

			if (oldBlock) {
				memcpy(&oldBlockData[7], &curBlockData[0], sizeof(struct fs_direntry));
				fs_blockdevice_write(bd, oldBlock, (struct fs_block *)&oldBlockData);

				// Shift 1 step right
				memmove(&curBlockData[0], &curBlockData[1], sizeof(struct fs_direntry) * 7);
				fs_blockdevice_write(bd, curBlock, (struct fs_block *)&curBlockData);

				// Setup for next move
				oldBlock = curBlock;
				memcpy(&oldBlockData, &curBlockData, sizeof(curBlockData));
			} else
				for (int j = 0; j < 8; j++) {
					if (curBlockData[j].id == id) {
						if (j < 7) {
							memmove(&curBlockData[j], &curBlockData[j+1], sizeof(struct fs_direntry) * (7 - j));
							fs_blockdevice_write(bd, curBlock, (struct fs_block *)&curBlockData);
						}

						oldBlock = curBlock;
						memcpy(&oldBlockData, &curBlockData, sizeof(curBlockData));
					}
				}
		}
	}

	memset(&oldBlockData[7], 0, sizeof(struct fs_direntry));
	fs_blockdevice_read(bd, oldBlock, (struct fs_block *)&oldBlockData);

	node->base.size -= sizeof(struct fs_direntry);
	fs_supernode_saveNode((struct fs_supernode *)node->runtimeStorage.sn, (struct fs_node *)node);
}


static void pnfs_addBlock(struct pnfs_node * node) {
	struct fs_supernode * sn = (struct fs_supernode *)node->runtimeStorage.sn;
	for (int i = 0; i < PNFS_NODE_BLOCKCOUNT; i++)
		if (!node->dataBlocks[i]) {
			fs_supernode_setBlockUsed(sn, node->dataBlocks[i] = fs_supernode_getFreeBlockID(sn));
			node->base.blockCount++;
			fs_supernode_saveNode(sn, (struct fs_node*)node);
			return;
		}

	struct fs_blockdevice * bd = node->runtimeStorage.sn->runtimeStorage.bd;
	struct pnfs_blockBlock blockBlock;
	fs_block_id curID = node->next;
	if (!curID) {
		curID = fs_supernode_getFreeBlockID((struct fs_supernode *)sn);
		fs_supernode_setBlockUsed(sn, curID);
		node->next = curID;
		memset(&blockBlock, 0, sizeof(struct pnfs_blockBlock));
		fs_blockdevice_write(bd, curID, (struct fs_block *)&blockBlock);
		fs_supernode_saveNode(sn, (struct fs_node*)node);
	}

	fs_blockdevice_read(bd, curID, (struct fs_block *)&blockBlock);
	while (true) {
		for (int i = 0; i < PNFS_BLOCKBLOCK_BLOCKCOUNT; i++)
			if (!blockBlock.dataBlocks[i]) {
				fs_supernode_setBlockUsed(sn, blockBlock.dataBlocks[i] = fs_supernode_getFreeBlockID(sn));
				fs_blockdevice_write(bd, curID, (struct fs_block *)&blockBlock);

				node->base.blockCount++;
				fs_supernode_saveNode(sn, (struct fs_node*)node);
				return;
			}

		curID = blockBlock.next;
		if (!curID)
			break;

		fs_blockdevice_read(bd, curID, (struct fs_block *)&blockBlock);
	}

	fs_block_id newBlock = fs_supernode_getFreeBlockID((struct fs_supernode *)sn);
	fs_supernode_setBlockUsed(sn, newBlock);
	blockBlock.next = newBlock;
	fs_blockdevice_write(bd, curID, (struct fs_block *)&blockBlock);

	fs_block_id newEntry = fs_supernode_getFreeBlockID((struct fs_supernode *)sn);
	fs_supernode_setBlockUsed(sn, newEntry);

	memset(&blockBlock, 0, sizeof(struct pnfs_blockBlock));
	fs_blockdevice_write(bd, newEntry, (struct fs_block *)&blockBlock);

	blockBlock.dataBlocks[0] = newEntry;
	fs_blockdevice_write(bd, newBlock, (struct fs_block *)&blockBlock);

	node->base.blockCount++;
	fs_supernode_saveNode(sn, (struct fs_node*)node);
}

static void pnfs_removeBlockBlock(struct pnfs_supernode * sn, struct pnfs_blockBlock * blockBlock) {
	struct fs_blockdevice * bd = sn->runtimeStorage.bd;
	fs_block_id next = blockBlock->next;

	for (int i = 0; i < PNFS_BLOCKBLOCK_BLOCKCOUNT; i++)
		if (blockBlock->dataBlocks[i])
			fs_supernode_setBlockFree((struct fs_supernode *)sn, blockBlock->dataBlocks[i]);

	if (!next)
		return;

	fs_blockdevice_read(bd, next, (struct fs_block *)blockBlock);
	fs_supernode_setBlockFree((struct fs_supernode *)sn, next);
	pnfs_removeBlockBlock(sn, blockBlock);
}

#define divRoundUp(a, b) (((a) + (b) - 1) / (b))

static void pnfs_removeBlocks(struct pnfs_node * node) {
	struct pnfs_supernode * sn = node->runtimeStorage.sn;
	struct fs_blockdevice * bd = sn->runtimeStorage.bd;
	uint16_t blocksNeeded = divRoundUp(node->base.size, BLOCK_SIZE);
	if (blocksNeeded < node->base.blockCount) {
		if (blocksNeeded <= PNFS_NODE_BLOCKCOUNT) {
			for (int i = blocksNeeded; i < PNFS_NODE_BLOCKCOUNT; i++)
				if (node->dataBlocks[i])
					fs_supernode_setBlockFree((struct fs_supernode *)sn, node->dataBlocks[i]);

			if (node->next) {
				struct pnfs_blockBlock blockBlock;
				fs_blockdevice_read(bd, node->next, (struct fs_block *)&blockBlock);

				pnfs_removeBlockBlock(sn, &blockBlock);
				fs_supernode_setBlockFree((struct fs_supernode *)sn, node->next);

				node->next = 0;
			}
		} else if (node->next) {
			struct pnfs_blockBlock blockBlock;
			fs_block_id prevID = 0;
			fs_block_id curID = node->next;
			fs_blockdevice_read(bd, curID, (struct fs_block *)&blockBlock);
			blocksNeeded -= PNFS_NODE_BLOCKCOUNT;

			while (blocksNeeded >= PNFS_BLOCKBLOCK_BLOCKCOUNT) {
				prevID = curID;
				curID = node->next;
				fs_blockdevice_read(bd, curID, (struct fs_block *)&blockBlock);
				blocksNeeded -= PNFS_BLOCKBLOCK_BLOCKCOUNT;
			}

			if (blocksNeeded) { // Leave block
				for (int i = blocksNeeded; i < PNFS_NODE_BLOCKCOUNT; i++)
					if (blockBlock.dataBlocks[i])
						fs_supernode_setBlockFree((struct fs_supernode *)sn, blockBlock.dataBlocks[i]);

				if (blockBlock.next) {
					struct pnfs_blockBlock blockBlock2;
					fs_blockdevice_read(bd, node->next, (struct fs_block *)&blockBlock2);
					pnfs_removeBlockBlock(sn, &blockBlock2);
					fs_supernode_setBlockFree((struct fs_supernode *)sn, node->next);

					blockBlock.next = 0;
					fs_blockdevice_write(bd, curID, (struct fs_block *)&blockBlock);
				}
			} else { // Remove block aswell
				pnfs_removeBlockBlock(sn, &blockBlock);
				fs_supernode_setBlockFree((struct fs_supernode *)sn, curID);

				fs_blockdevice_read(bd, prevID, (struct fs_block *)&blockBlock);
				blockBlock.next = 0;
				fs_blockdevice_write(bd, prevID, (struct fs_block *)&blockBlock);
			}
		}
		fs_supernode_saveNode((struct fs_supernode *)sn, (struct fs_node *)node);
	}
}

#undef divRoundUp

static struct fs_node * pnfs_node_findNode(struct fs_node * node, const char * path_) {
	struct fs_supernode * sn = (struct fs_supernode *)((struct pnfs_node *)node)->runtimeStorage.sn;
	char * path = strdup(path_);
	char * orgPath = path;
	char * saveptr;
	char * part = strtok_r(path, "/", &saveptr);

	struct fs_node * cur;
	if (*path == '/')
		cur = fs_supernode_getNode(sn, NODE_ROOT);
	else 	// This is because we will free cur later, so we need a copy
		cur = fs_supernode_getNode(sn, node->id);

	while (part && cur) {
		uint16_t amount;
		struct fs_direntry * dir = fs_node_directoryEntries(cur, &amount);

		if (!dir) {
			printf("[-] Path '%s' contains a entry which isn't a directory!\n", part);
			free(cur);
			free(orgPath);
			return NULL;
		}

		fs_node_id id = NODE_INVALID;
		for (uint16_t i = 0; i < amount; i++)
			if (!strcmp(dir[i].name, part)) {
				id = dir[i].id;
				break;
			}

		free(dir);
		if (id == NODE_INVALID) {
			free(cur);
			free(orgPath);
			return NULL;
		}

		struct fs_node * newCur = fs_supernode_getNode(sn, id);
		free(cur);
		cur = newCur;

		part = strtok_r(NULL, "/", &saveptr);
	}

	free(orgPath);
	return cur;
}

static char * pnfs_node_getName(struct fs_node * node, struct fs_node * parent) {
	char * name = NULL;
	uint16_t amount;
	struct fs_direntry * dir = fs_node_directoryEntries(parent, &amount);
	if (!dir)
		return NULL;

	for (int i = 0; i < amount; i++)
		if (dir[i].id == node->id) {
			name = strdup(dir[i].name);
			break;
		}
	free(dir);

	return name;
}

static struct fs_node * pnfs_node_getParent(struct fs_node * node) {
	uint16_t amount;
	struct fs_direntry * dir = fs_node_directoryEntries(node, &amount);
	if (!dir)
		return NULL;

	fs_node_id id = NODE_INVALID;
	for (int i = 0; i < amount; i++)
		if (!strcmp(dir[i].name, "..")) {
			id = dir[i].id;
			break;
		}
	free(dir);

	if (id != NODE_INVALID)
		return fs_supernode_getNode((struct fs_supernode *)((struct pnfs_node *)node)->runtimeStorage.sn, id);
	return NULL;
}
