#ifndef FS_SUPERNODE_H
#define FS_SUPERNODE_H

#include "fs.h"
#include "bd.h"

struct fs_supernode_vtbl {
	struct fs_node * (*getNode)(struct fs_supernode * sn, fs_node_id id);
	void (*saveNode)(struct fs_supernode * sn, struct fs_node * node);

	struct fs_node * (*addNode)(struct fs_supernode * sn, struct fs_node * parent, enum fs_node_type type, char * name);
	bool (*removeNode)(struct fs_supernode * sn, fs_node_id id);

	fs_node_id (*getFreeNodeID)(struct fs_supernode * sn);
	fs_block_id (*getFreeBlockID)(struct fs_supernode * sn);

	void (*setBlockUsed)(struct fs_supernode * sn, fs_block_id id);
	void (*setBlockFree)(struct fs_supernode * sn, fs_block_id id);
};

struct fs_supernode {
	struct fs_supernode_vtbl * vtbl;
};

struct fs_node * fs_supernode_getNode(struct fs_supernode * sn, fs_node_id id);
void fs_supernode_saveNode(struct fs_supernode * sn, struct fs_node * node);

struct fs_node * fs_supernode_addNode(struct fs_supernode * sn, struct fs_node * parent, enum fs_node_type type, char * name);
bool fs_supernode_removeNode(struct fs_supernode * sn, fs_node_id id);

fs_node_id fs_supernode_getFreeNodeID(struct fs_supernode * sn);
fs_block_id fs_supernode_getFreeBlockID(struct fs_supernode * sn);

void fs_supernode_setBlockUsed(struct fs_supernode * sn, fs_block_id id);
void fs_supernode_setBlockFree(struct fs_supernode * sn, fs_block_id id);

#endif
