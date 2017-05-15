#include "fs_supernode.h"

struct fs_node * fs_supernode_getNode(struct fs_supernode * sn, fs_node_id id) {
	return sn->vtbl->getNode(sn, id);
}

void fs_supernode_saveNode(struct fs_supernode * sn, struct fs_node * node) {
	return sn->vtbl->saveNode(sn, node);
}

struct fs_node * fs_supernode_addNode(struct fs_supernode * sn, struct fs_node * parent, enum fs_node_type type, const char * name) {
	return sn->vtbl->addNode(sn, parent, type, name);
}

bool fs_supernode_removeNode(struct fs_supernode * sn, struct fs_node * parent, fs_node_id id) {
	return sn->vtbl->removeNode(sn, parent, id);
}

fs_node_id fs_supernode_getFreeNodeID(struct fs_supernode * sn) {
	return sn->vtbl->getFreeNodeID(sn);
}

fs_block_id fs_supernode_getFreeBlockID(struct fs_supernode * sn) {
	return sn->vtbl->getFreeBlockID(sn);
}

void fs_supernode_setBlockUsed(struct fs_supernode * sn, fs_block_id id) {
	return sn->vtbl->setBlockUsed(sn, id);
}

void fs_supernode_setBlockFree(struct fs_supernode * sn, fs_block_id id) {
	return sn->vtbl->setBlockFree(sn, id);
}
