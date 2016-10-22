#include "fs_node.h"

void * fs_node_readData(struct fs_node * node, void * buffer, uint16_t offset, uint16_t size) {
	return node->vtbl->readData(node, buffer, offset, size);
}

bool fs_node_writeData(struct fs_node * node, void * buffer, uint16_t offset, uint16_t size) {
	return node->vtbl->writeData(node, buffer, offset, size);
}

struct fs_direntry * fs_node_directoryEntries(struct fs_node * node, uint16_t * amount) {
	return node->vtbl->directoryEntries(node, amount);
}

struct fs_node * fs_node_findNode(struct fs_node * node, char * path) {
	return node->vtbl->findNode(node, path);
}

char * fs_node_getName(struct fs_node * node) {
	return node->vtbl->getName(node);
}

struct fs_node * fs_node_getParent(struct fs_node * node) {
	return node->vtbl->getParent(node);
}
