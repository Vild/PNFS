#include "pnfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "fs_supernode.h"

#define min(x_, y_) ({ \
    __typeof__ (x_) x = (x_); \
    __typeof__ (y_) y = (y_); \
    (void)(&x == &y); \
    x < y ? x : y; })

// VTables functions
static struct fs_node * pnfs_supernode_getNode(struct fs_supernode * sn, fs_node_id id);
static void pnfs_supernode_saveNode(struct fs_supernode * sn, struct fs_node * node);

static fs_node_id pnfs_supernode_getFreeNodeID(struct fs_supernode * sn);
static fs_block_id pnfs_supernode_getFreeBlockID(struct fs_supernode * sn);

static void pnfs_supernode_setBlockUsed(struct fs_supernode * sn, fs_block_id id);
static void pnfs_supernode_setBlockFree(struct fs_supernode * sn, fs_block_id id);

static void * pnfs_node_readData(struct fs_node * node, void * buffer, uint16_t offset, uint16_t size);
static bool pnfs_node_writeData(struct fs_node * node, void * buffer, uint16_t offset, uint16_t size);

static struct fs_direntry * pnfs_node_directoryEntries(struct fs_node * node, uint16_t * amount);
static struct fs_node * pnfs_node_findNode(struct fs_node * node, char * path);

static char * pnfs_node_getName(struct fs_node * node);
static struct fs_node * pnfs_node_getParent(struct fs_node * node);

// VTables
static struct fs_supernode_vtbl pnfs_supernode_vtbl = (struct fs_supernode_vtbl) {
	.getNode = &pnfs_supernode_getNode,
	.saveNode = &pnfs_supernode_saveNode,

	.getFreeNodeID = &pnfs_supernode_getFreeNodeID,
	.getFreeBlockID = &pnfs_supernode_getFreeBlockID,

	.setBlockUsed = &pnfs_supernode_setBlockUsed,
	.setBlockFree = &pnfs_supernode_setBlockFree
};

static struct fs_node_vtbl pnfs_node_vtbl = (struct fs_node_vtbl) {
	.readData = &pnfs_node_readData,
	.writeData = &pnfs_node_writeData,

	.directoryEntries = &pnfs_node_directoryEntries,
	.findNode = &pnfs_node_findNode,

	.getName = &pnfs_node_getName,
	.getParent = &pnfs_node_getParent
};


// Local helper type

struct pnfs_nodeBlock { // This is to help with the reading
	struct {
		uint8_t _[sizeof(struct pnfs_node) - sizeof(void* /* Vtbl */)-sizeof(((struct pnfs_node*)NULL)->runtimeStorage)];
	} blocks[8];
};

struct pnfs_blockBlock {
	fs_block_id dataBlocks[64/sizeof(fs_block_id) - 1];
	fs_block_id next;
};

// Local functions
static struct pnfs_supernode * pnfs_initFS(struct fs_blockdevice * bd, struct pnfs_supernode * sn);

// Code
struct pnfs_supernode * pnfs_init(struct fs_blockdevice * bd) {
	struct fs_block block;
	struct pnfs_supernode * sn = malloc(sizeof(struct pnfs_supernode));
	fs_blockdevice_read(bd, PNFS_BLOCK_HEADER, &block);
	memcpy(((void*)sn) + sizeof(void*), &block, sizeof(struct pnfs_supernode)-sizeof(void*/*vtbl*/)-sizeof(sn->runtimeStorage));
	sn->base.vtbl = &pnfs_supernode_vtbl;
	sn->runtimeStorage.bd = bd;
	
	if (sn->magic != PNFS_MAGIC) {
		printf("[-] No PNFS found on disk!\n");
		sn = pnfs_initFS(bd, sn);
	}
	
	printf("[+] Loaded PNFS correctly!\n");

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
		memcpy(&block, ((void*)sn) + sizeof(void*), sizeof(struct pnfs_supernode) - sizeof(void*) - sizeof(sn->runtimeStorage));
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
		/*
			This uses just a small pointer hack to make sure that the datastructure
			matches the data in the block. A better fix would be to make a separate
			structure for this, but as I'm the only developer so I think this will be
			okey.
		*/
		struct pnfs_node * node = (struct pnfs_node *) (&emptyNodeBlock.blocks[i] - 8 /* VTbl */);
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
		fs_block_id id = node->regular.dataBlocks[0] = fs_supernode_getFreeBlockID((struct fs_supernode *)sn);
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

	
	memcpy((void*)node + sizeof(void*), &(block.blocks[id%8]), sizeof(struct pnfs_node) - sizeof(void*)-sizeof(sn->runtimeStorage));
	return (struct fs_node *)node;
}


static void pnfs_supernode_saveNode(struct fs_supernode * sn_, struct fs_node * node) {
	struct pnfs_supernode * sn = (struct pnfs_supernode *)sn_;
	struct pnfs_nodeBlock block;
	fs_blockdevice_read(sn->runtimeStorage.bd, node->id / 8 + 1, (struct fs_block *)&block);
	memcpy(&block.blocks[node->id % 8], (void*)node + sizeof(void*), sizeof(struct pnfs_node) - sizeof(void*)-sizeof(sn->runtimeStorage));
	fs_blockdevice_write(sn->runtimeStorage.bd, node->id / 8 + 1, (struct fs_block *)&block);
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
}

static void pnfs_supernode_setBlockFree(struct fs_supernode * sn_, fs_block_id id) {
	struct pnfs_supernode * sn = (struct pnfs_supernode *)sn_;
	sn->freeBlocksBitmap[id/8] &= ~(1 << (id % 8));
}

static void * pnfs_node_readData(struct fs_node * node, void * buffer, uint16_t offset, uint16_t size) {
	return NULL;
}

static bool pnfs_node_writeData(struct fs_node * node, void * buffer, uint16_t offset, uint16_t size) {
	return false;
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

	const uint16_t blocksInNode = sizeof(node->regular.dataBlocks) / sizeof(fs_block_id);
	
	for (int i = 0; i < min(node->base.blockCount, blocksInNode); i++) {
		struct fs_direntry entries[8];
		fs_blockdevice_read(bd, node->regular.dataBlocks[i], (struct fs_block *)&entries);
		for (int j = 0; j < 8 && added < count; j++)
			memcpy(&dir[added++], &entries[j], sizeof(struct fs_direntry));
	}

	if (added < count) {
		struct pnfs_blockBlock block;
		const int blocksInBlockBlock = sizeof(block.dataBlocks) / sizeof(fs_block_id);

		fs_blockdevice_read(bd, node->regular.next, (struct fs_block *)&block);

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

static struct fs_node * pnfs_node_findNode(struct fs_node * node, char * path) {
	struct fs_supernode * sn = (struct fs_supernode *)((struct pnfs_node *)node)->runtimeStorage.sn;
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
			return NULL;
		}
		
		fs_node_id id = NODE_INVALID;
		for (uint16_t i = 0; i < amount; i++)
			if (!strcmp(dir[i].name, part)) {
				id = dir[i].id;
				break;
			}

		free(dir);
		if (id == NODE_INVALID)
			return NULL;
		
		struct fs_node * newCur = fs_supernode_getNode(sn, id);
		free(cur);
		cur = newCur;		
		
		part = strtok_r(NULL, "/", &saveptr);
	}
	
	return cur;
}

static char * pnfs_node_getName(struct fs_node * node) {
	struct fs_node * parent = fs_node_getParent(node);
	char * name = NULL;
	if (parent) {
		uint16_t amount;
		struct fs_direntry * dir = fs_node_directoryEntries(node, &amount);
		if (!dir)
			return NULL;

		for (int i = 0; i < amount; i++)
			if (dir[i].id == node->id) {
				name = strdup(dir[i].name);
				break;
			}
		free(dir);
	}
	free(parent);

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
