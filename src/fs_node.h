#ifndef FS_NODE_H
#define FS_NODE_H

#include "fs.h"

/**
 * The vtable for fs_node
 * \relates fs_node
 */
struct fs_node_vtbl {
	/**
	 * Prototype of fs_node_readData.
	 * \see fs_node_readData
	 */
	uint16_t (*readData)(struct fs_node * node, void * buffer, uint16_t offset, uint16_t size);

	/**
	 * Prototype of fs_node_writeData.
	 * \see fs_node_writeData
	 */
	uint16_t (*writeData)(struct fs_node * node, void * buffer, uint16_t offset, uint16_t size);

	/**
	 * Prototype of fs_node_directoryEntries.
	 * \see fs_node_directoryEntries
	 */
	struct fs_direntry * (*directoryEntries)(struct fs_node * node, uint16_t * amount);

	/**
	 * Prototype of fs_node_findNode.
	 * \see fs_node_findNode
	 */
	struct fs_node * (*findNode)(struct fs_node * node, char * path);

	/**
	 * Prototype of fs_node_getName.
	 * \see fs_node_getName
	 */
	char * (*getName)(struct fs_node * node, struct fs_node * parent);

	/**
	 * Prototype of fs_node_getParent.
	 * \see fs_node_getParent
	 */
	struct fs_node * (*getParent)(struct fs_node * node);
};


/**
 * The node type.
 */
struct fs_node {
	/// Internal vtable stuff
	struct fs_node_vtbl * vtbl;

	/// The node id
	fs_node_id id;

	/// What type the node is
	/// \relates fs_node_type
	uint16_t type;

	/// The size
	uint16_t size;

	/// The amount of nodes it uses
	uint16_t blockCount;
};

/**
 * Read data from the node.
 * \param node The node to read from
 * \param buffer Where to write the data to
 * \param offset Where to start in the node
 * \param size How much to read in the node
 * \return The amount of data read
 * \relates fs_node
 */
uint16_t fs_node_readData(struct fs_node * node, void * buffer, uint16_t offset, uint16_t size);

/**
 * Write data to the node.
 * \param node The node to write to
 * \param buffer Where to read the data from
 * \param offset Where to start in the node
 * \param size How much to write
 * \return The amount of data written
 * \relates fs_node
 */
uint16_t fs_node_writeData(struct fs_node * node, void * buffer, uint16_t offset, uint16_t size);

/**
 * Get a array of all the entries in a directory
 * \param node The directory node
 * \param amount Returns how big the array is
 * \return The directory entry array, if the node is of the type NODETYPE_DIRECTORY, else NULL
 * \relates fs_node
 */
struct fs_direntry * fs_node_directoryEntries(struct fs_node * node, uint16_t * amount);

/**
 * Search for a node based on the \a path.
 * The path be both absolute or relative.
 * \param node The directory node to start the search from
 * \param path The path to be searched
 * \return The node it found else NULL
 * \relates fs_node
 */
struct fs_node * fs_node_findNode(struct fs_node * node, char * path);

/**
 * Get the name a of node.
 * \param node The node you want to get the name from
 * \param parent The parent for that node
 * \return The name or NULL
 * \relates fs_node
 */
char * fs_node_getName(struct fs_node * node, struct fs_node * parent);

/**
 * Get the parent for a directory node.
 * \param node The directory node.
 * \return The parent for that node
 * \relates fs_node
 */
struct fs_node * fs_node_getParent(struct fs_node * node);

#endif
