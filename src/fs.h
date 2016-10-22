#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stdbool.h>
#include "block.h"

struct fs_node;
struct fs_direntry;
struct fs_supernode;
typedef uint16_t fs_node_id;

enum {
	NODE_INVALID = 0,
	NODE_ROOT = 1
};

enum fs_node_type {
	NODETYPE_INVALID = 0,
	NODETYPE_FILE,
	NODETYPE_DIRECTORY,
	
	NODETYPE_NEVER_VALID,
	NODETYPE_MAX
};

// This one needs to be here
#include "fs_supernode.h"
#include "fs_node.h"

struct fs_direntry {
	fs_node_id id;
	char name[62];
};

#endif
