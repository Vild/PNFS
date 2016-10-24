#ifndef FS_NODE_H
#define FS_NODE_H

#include "fs.h"

struct fs_node_vtbl {
	uint16_t (*readData)(struct fs_node * node, void * buffer, uint16_t offset, uint16_t size);
	bool (*writeData)(struct fs_node * node, void * buffer, uint16_t offset, uint16_t size);

	struct fs_direntry * (*directoryEntries)(struct fs_node * node, uint16_t * amount);
	struct fs_node * (*findNode)(struct fs_node * node, char * path);

	char * (*getName)(struct fs_node * node, struct fs_node * parent);
	struct fs_node * (*getParent)(struct fs_node * node);
};

struct fs_node {
	struct fs_node_vtbl * vtbl;
	uint16_t id;
	uint16_t type; // enum fs_node_type
	uint16_t size;
	uint16_t blockCount;
};

uint16_t fs_node_readData(struct fs_node * node, void * buffer, uint16_t offset, uint16_t size);
bool fs_node_writeData(struct fs_node * node, void * buffer, uint16_t offset, uint16_t size);

struct fs_direntry * fs_node_directoryEntries(struct fs_node * node, uint16_t * amount);
struct fs_node * fs_node_findNode(struct fs_node * node, char * path);

char * fs_node_getName(struct fs_node * node, struct fs_node * parent);
struct fs_node * fs_node_getParent(struct fs_node * node);

#endif
