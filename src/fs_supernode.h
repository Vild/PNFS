#ifndef FS_SUPERNODE_H
#define FS_SUPERNODE_H

#include "fs.h"
#include "bd.h"

/**
 * The vtable for fs_supernode_node.
 * \relates fs_supernode_node
 */
struct fs_supernode_vtbl {
	/**
	 * Prototype of fs_supernode_getNode.
	 * \see fs_supernode_getNode
	 */
	struct fs_node * (*getNode)(struct fs_supernode * sn, fs_node_id id);

	/**
	 * Prototype of fs_supernode_saveNode.
	 * \see fs_supernode_saveNode
	 */
	void (*saveNode)(struct fs_supernode * sn, struct fs_node * node);

	/**
	 * Prototype of fs_supernode_addNode.
	 * \see fs_supernode_addNode
	 */
	struct fs_node * (*addNode)(struct fs_supernode * sn, struct fs_node * parent, enum fs_node_type type, const char * name);

	/**
	 * Prototype of fs_supernode_removeNode.
	 * \see fs_supernode_removeNode
	 */
	bool (*removeNode)(struct fs_supernode * sn, struct fs_node * parent, fs_node_id id);

	/**
	 * Prototype of fs_supernode_getFreeNodeID.
	 * \see fs_supernode_getFreeNodeID
	 */
	fs_node_id (*getFreeNodeID)(struct fs_supernode * sn);

	/**
	 * Prototype of fs_supernode_getFreeBlockID.
	 * \see fs_supernode_getFreeBlockID
	 */
	fs_block_id (*getFreeBlockID)(struct fs_supernode * sn);

	/**
	 * Prototype of fs_supernode_setBlockUsed.
	 * \see fs_supernode_setBlockUsed
	 */
	void (*setBlockUsed)(struct fs_supernode * sn, fs_block_id id);

	/**
	 * Prototype of fs_supernode_setBlockFree.
	 * \see fs_supernode_setBlockFree
	 */
	void (*setBlockFree)(struct fs_supernode * sn, fs_block_id id);
};

/**
 * The supernode type.
 */
struct fs_supernode {
	/// Internal vtable stuff
	struct fs_supernode_vtbl * vtbl;
};

/**
 * Get the node corresponding to the \a id.
 * \param sn The supernode
 * \param id The index
 * \return The node
 * \relates fs_supernode
 */
struct fs_node * fs_supernode_getNode(struct fs_supernode * sn, fs_node_id id);

/**
 * Save the changes of a node to disk.
 * \param sn The supernode
 * \param node The node to save
 * \relates fs_supernode
 */
void fs_supernode_saveNode(struct fs_supernode * sn, struct fs_node * node);

/**
 * Create a new node.
 * \param sn The supernode
 * \param parent The parent for the new node
 * \param type The type for the new node
 * \param name The name for the new node
 * \return The new node
 * \relates fs_supernode
 */
struct fs_node * fs_supernode_addNode(struct fs_supernode * sn, struct fs_node * parent, enum fs_node_type type, const char * name);

/**
 * Remove a node.
 * \param sn The supernode
 * \param parent The parent for the node
 * \param id The node id
 * \return If the removal was successful
 * \relates fs_supernode
 */
bool fs_supernode_removeNode(struct fs_supernode * sn, struct fs_node * parent, fs_node_id id);

/**
 * Get a free node id
 * \param sn The supernode
 * \return The free node id, 0 if it failed
 * \relates fs_supernode
 */
fs_node_id fs_supernode_getFreeNodeID(struct fs_supernode * sn);

/**
 * Get a free block id
 * \param sn The supernode
 * \return The free block id, 0 if it failed
 * \relates fs_supernode
 */
fs_block_id fs_supernode_getFreeBlockID(struct fs_supernode * sn);

/**
 * Set a block status as used.
 * \param sn The supernode
 * \param id The block id
 * \relates fs_supernode
 */
void fs_supernode_setBlockUsed(struct fs_supernode * sn, fs_block_id id);

/**
 * Set a block status as free.
 * \param sn The supernode
 * \param id The block id
 * \relates fs_supernode
 */
void fs_supernode_setBlockFree(struct fs_supernode * sn, fs_block_id id);

#endif
