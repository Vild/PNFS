#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stdbool.h>
#include "block.h"

struct fs_node;
struct fs_direntry;
struct fs_supernode;

/**
 * The node index type.
 * \relates fs_node
 */
typedef uint16_t fs_node_id;

/**
 * Contains a few special node ids.
 * \relates fs_node_id
 */
enum {
	/// Invalid node id
	NODE_INVALID = 0,
	/// The id for the root node
	NODE_ROOT = 1
};

/**
 * The different node types.
 * \relates fs_node
 */
enum fs_node_type {
	/// Invalid type
	NODETYPE_INVALID = 0,
	/// File type
	NODETYPE_FILE,
	/// Directory type
	NODETYPE_DIRECTORY,

	/// Will never be valid, Only used for node ::NODE_INVALID
	NODETYPE_NEVER_VALID,
	NODETYPE_MAX
};

// This one needs to be here
#include "fs_supernode.h"
#include "fs_node.h"

/**
 * This is the representation of directory entries.
 * This only exist in nodes when type is NODE_DIRECTORY
 * \relates fs_node
 */
struct fs_direntry {
	/// The id for the entry
	fs_node_id id;
	/// The name for the entry
	char name[62];
};

#endif
