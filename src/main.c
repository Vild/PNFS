#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "fs.h"
#include "bd.h"
#include "pnfs.h"
#include "block.h"

static void doCommand(char * cmd);

static bool quit;
static struct fs_blockdevice * bd;
static struct fs_supernode * sn;

static struct fs_node * cwd = NULL;

char * getCWD(struct fs_node * current, char * str, int * left) {
	if (!current || *left <= 0)
		return str;

	char * name = NULL;
	if (current->id != NODE_ROOT) {
		struct fs_node * parent = fs_node_getParent(current);

		if (parent) {
			str = getCWD(parent, str, left);
			name = fs_node_getName(current);
			free(parent);
		}
	}

	int len;
	if (!name)
		len = snprintf(str, *left, "/");
	else {
		len = snprintf(str, *left, "/%s", name);
		free(name);
	}
	
	*left -= len;
	str += len;

	return str;
}

int main(int argc, char ** argv) {
	printf("Welcome to the PNFS test shell!\n");

	bd = malloc(sizeof(struct fs_blockdevice));
	fs_blockdevice_clear(bd);

	sn = (struct fs_supernode *)pnfs_init(bd);

	char * PS1 = malloc(0x1000);
	struct fs_node * oldCwd = NULL;

	oldCwd = cwd = fs_supernode_getNode(sn, NODE_ROOT);

	int len = 0x1000;
	getCWD(cwd, PS1, &len);
	strncat(PS1, "$ ", 0x1000);
	quit = false;
	while (!quit) {
		if (oldCwd != cwd) {
			len = 0x1000;
			getCWD(cwd, PS1, &len);
			strncat(PS1, "$ ", 0x1000);
			free(oldCwd);
			oldCwd = cwd;
		}
		char * line = readline(PS1);
		if (!line)
			break;

		add_history(line);
		doCommand(line);
		free(line);
	}
	free(PS1);
	free(cwd);
	free(sn);
	free(bd);
	return 0;
}

static void cat_cmd();
static void cd_cmd();
static void copy_cmd();
static void create_cmd();
static void createImage_cmd();
static void exit_cmd();
static void format_cmd();
static void ls_cmd();
static void mkdir_cmd();
static void pwd_cmd();
static void restoreImage_cmd();
static void rm_cmd();

struct cmd {
	const char * name;
	void (*func)();
	const char * args;
	const char * desc;
};

static void doCommand(char * line) {
	char * part = strtok(line, " ");
	if (!part)
		return;

	struct cmd validCommands[12] = {
		{"cat", &cat_cmd, "<file>", "Print the content of file(s)"},
		{"cd", &cd_cmd, "<path>", "Change the working directory"},
		{"copy", &copy_cmd, "<from> <to>", "Copy a file or directory"},
		{"create", &create_cmd, "<filename>", "Create a text file"},
		{"createImage", &createImage_cmd, "<filename on host>", "Save the HDD to a file on the host"},
		{"exit", &exit_cmd, "", "Exit the shell"},
		{"format", &format_cmd, "", "Format the HDD"},
		{"ls", &ls_cmd, "", "List all the file and folder"},
		{"mkdir", &mkdir_cmd, "<dirname>", "Make a directory"},
		{"pwd", &pwd_cmd, "", "Print the current working directory"},
		{"restoreImage", &restoreImage_cmd, "<filename on host>", "Load the HDD from a file on the host"},
		{"rm", &rm_cmd, "Remove a file or folder"}
	};


	for (int i = 0; i < sizeof(validCommands) / sizeof(*validCommands); i++)
		if (!strcasecmp(part, validCommands[i].name))
			return validCommands[i].func();


	printf("Unknown command!\n");
	printf("Valid commands are:\n");
	for (int i = 0; i < sizeof(validCommands) / sizeof(*validCommands); i++)
		printf("\t%-14s %-20s - %s\n", validCommands[i].name, validCommands[i].args, validCommands[i].desc);
}

// To get the other arguments the function below can use NEXT_TOKEN

#define NEXT_TOKEN strtok(NULL, " ")

static void cat_cmd() {
	char * path = NEXT_TOKEN;
	if (!path) {
		printf("[-] A path is required!\n");
		return;
	}
	
	struct fs_node * node = fs_node_findNode(cwd, path);

	if (!node) {
		printf("Could not find node!");
		return;
	}

	char * buf = malloc(node->size);
	fs_node_readData(node, buf, 0, node->size);
	printf("%*s\n\n", node->size, buf);
	free(buf);
	free(node);
}

static void cd_cmd() {
	char * path = NEXT_TOKEN;
	if (!path) {
		printf("[-] A path is required!\n");
		return;
	}

	struct fs_node * node = fs_node_findNode(cwd, path);

	if (!node)
		printf("[-] Could not find node!\n");
	else if (node->type != NODETYPE_DIRECTORY)
		printf("[-] Can only cd into directories!\n");
	else {
		cwd = node;
		return;
	}
	
	free(node);
}

static void copy_cmd() {

}

static void create_cmd() {

}

static void createImage_cmd() {
	char * filename = NEXT_TOKEN;
	if (!filename) {
		printf("[-] A filename is required!\n");
		return;
	}

	if (!fs_blockdevice_save(bd, filename)) {
		printf("[-] Failed to save HDD image!\n");
		return;
	}
	
	printf("[+] Saved HDD image correctly\n");
}

static void exit_cmd() {
	quit = true;
}

static void format_cmd() {
	free(sn);
	fs_blockdevice_clear(bd);
	printf("[+] Formatted!\n");
	sn = (struct fs_supernode *)pnfs_init(bd);
	cwd = fs_supernode_getNode(sn, NODE_ROOT);
}

static void ls_cmd() {
	uint16_t amount;
	struct fs_direntry * dir = fs_node_directoryEntries(cwd, &amount);
	const char* nodetypeName[] = {
		[NODETYPE_INVALID] = "Invalid",
		[NODETYPE_FILE] = "File",
		[NODETYPE_DIRECTORY] = "Directory",
		[NODETYPE_INLINE_FILE] = "Inline File",
		[NODETYPE_INLINE_DIRECTORY] = "Inline Directory",
		[NODETYPE_NEVER_VALID] = "Never Valid"
	};
	if (!dir) {
		printf("[-] There are no entries in the current directory, something is really wrong!\n");
		return;
	}

	printf("| %-62s | %-16s | %-16s |\n", "Name", "Type", "Size");
	for (uint16_t i = 0; i < amount; i++) {
		struct fs_node * node = fs_supernode_getNode(sn, dir[i].id);
		if (!node)
			continue;
		printf("| %-62s | %-16s | %-16u |\n", dir[i].name, nodetypeName[node->type], node->size);
		free(node);
	}
	
	free(dir);
}

static void mkdir_cmd() {	
	char * filename = NEXT_TOKEN;
	if (!filename) {
		printf("[-] A filename is required!\n");
		return;
	}

	fs_node_id id = fs_supernode_getFreeNodeID(sn);

	if (id == NODE_INVALID) {
		printf("[-] No more free nodes");
		return;
	}
	
	{	
		struct pnfs_node * node = (struct pnfs_node *)fs_supernode_getNode((struct fs_supernode *)sn, id);
		node->base.id = id;
		node->base.type = NODETYPE_DIRECTORY;
		node->base.size = sizeof(struct fs_direntry) * 2;
		node->base.blockCount = 1;
		fs_block_id blockID = node->regular.dataBlocks[0] = fs_supernode_getFreeBlockID(sn);
		fs_supernode_saveNode((struct fs_supernode *)sn, (struct fs_node *)node);
		free(node);
		
		struct fs_direntry entries[8];
		memset(entries, 0, sizeof(entries));
		entries[0].id = id;
		strncpy(entries[0].name, ".", sizeof(entries[0].name));
		
		entries[1].id = cwd->id;
		strncpy(entries[1].name, "..", sizeof(entries[1].name));
		fs_blockdevice_write(bd, blockID, (struct fs_block *)&entries);
	}

	// Now add the directory to cwd


	//TODO: EXTRACT TO FUNCTION!

	uint16_t dirPos = cwd->size / sizeof(fs_direntry);
	uint16_t inBlockIdx = dirPos / 8;
	
	struct fs_direntry entries[8];
	fs_block_id blockID;

	if (inBlockIdx < sizeof(cwd->blocks)/sizeof(fs_block_id)) {
		if (inBlockIdx < cwd->blockCount)
			blockID = cwd->blocks[inBlockIdx];
		else {
			// Allocate needed
			blockID = cwd->blocks[inBlockIdx] = fs_supernode_getFreeBlockID(sn);
			struct fs_block block;
			memset(&block, 0, sizeof(struct fs_block));
			fs_blockdevice_write(bd, blockID, &block);	
		}
	} else {
	
		// Need to add another block?
		if (inBlockIdx + 1 > cwd->blockCount) {
			struct pnfs_node * addTo = (struct pnfs_node *)cwd;
			blockID = fs_supernode_getFreeBlockID(sn);
			// It is in a .next blocknode

		
		}
	}
	

	fs_blockdevice_read(bd, blockID, (struct fs_block *)&entries);
	entries[dirPos%8].id = id;
	strncpy(entries[dirPos%8].name, filename, sizeof(entries[dirPos%8].name));
	fs_blockdevice_write(bd, blockID, (struct fs_block *)&entries);	
}

static void pwd_cmd() {
	char buf[0x1000];
	int len = sizeof(buf);
	getCWD(cwd, buf, &len);
	printf("ID: %d,\tPath: %s\n", cwd->id, buf);
}

static void restoreImage_cmd() {
	char * filename = NEXT_TOKEN;
	if (!filename) {
		printf("[-] A filename is required!\n");
		return;
	}

	free(sn);
	
	if (!fs_blockdevice_load(bd, filename)) {
		printf("[-] Failed to loaded HDD image!\n");
		return;
	}
	
	printf("[+] Loaded HDD image correctly\n");
	sn = (struct fs_supernode *)pnfs_init(bd);
	cwd = fs_supernode_getNode(sn, NODE_ROOT);
}

static void rm_cmd() {
	
}

#undef NEXT_TOKEN

