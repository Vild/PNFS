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
	if (current->id == NODE_ROOT)
		return str;
	
	struct fs_node * parent = fs_node_getParent(current);

	if (!parent)
		return str;
	
	str = getCWD(parent, str, left);
	name = fs_node_getName(current, parent);
	free(parent);			
	if (!name)
		return str;

	int len = snprintf(str, *left, "/%s", name);
	free(name);
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
	PS1[0] = '/';
	PS1[1] = '\0';
	struct fs_node * oldCwd = NULL;

	oldCwd = cwd = fs_supernode_getNode(sn, NODE_ROOT);

	int len = 0x1000;
	getCWD(cwd, PS1, &len);
	strncat(PS1, "$ ", 0x1000);
	quit = false;
	while (!quit) {
		if (oldCwd != cwd) {
			len = 0x1000;
			PS1[0] = '/';
			PS1[1] = '\0';
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

	struct cmd validCommands[13] = {
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
		{"rm", &rm_cmd, "Remove a file or folder"},
		{"quit", &exit_cmd, "", "Quit the shell"}
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
		printf("Could not find node!\n");
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
		[NODETYPE_NEVER_VALID] = "Never Valid"
	};
	if (!dir) {
		printf("[-] There are no entries in the current directory, something is really wrong!\n");
		return;
	}

	printf("| %-8s | %-62s | %-16s | %-16s |\n", "ID", "Name", "Type", "Size");
	for (uint16_t i = 0; i < amount; i++) {
		struct fs_node * node = fs_supernode_getNode(sn, dir[i].id);
		if (!node)
			continue;
		printf("| %-8d | %-62s | %-16s | %-16u |\n", dir[i].id, dir[i].name, nodetypeName[node->type], node->size);
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
	
	{
		struct fs_node * n = fs_node_findNode(cwd, filename);
		if (n) {
			free(n);
			printf("[-] There is already a node with that name\n");
			return;
		}
	}
	fs_supernode_addNode(sn, cwd, NODETYPE_DIRECTORY, filename);
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
	char * path = NEXT_TOKEN;
	if (!path) {
		printf("[-] A path is required!\n");
		return;
	}

	struct fs_node * node = fs_node_findNode(cwd, path);
	if (!node) {
		printf("Could not find node!\n");
		return;
	}

	fs_node_id id = node->id;
	free(node);
	fs_supernode_removeNode(sn, id);
}

#undef NEXT_TOKEN

