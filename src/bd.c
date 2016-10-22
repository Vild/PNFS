#include <stdio.h>
#include <string.h>
#include "bd.h"

void fs_blockdevice_clear(struct fs_blockdevice * bd) {
	memset(bd->blocks, 0, sizeof(bd->blocks));
}

bool fs_blockdevice_load(struct fs_blockdevice * bd, char * file) {
	FILE * fp = fopen(file, "rb");

	if (!fp)
		return false;

	fread(bd->blocks, 1, sizeof(bd->blocks), fp);
	fclose(fp);
	return true;
}

bool fs_blockdevice_save(struct fs_blockdevice * bd, char *file) {
	FILE * fp = fopen(file, "wb");

	if (!fp)
		return false;

	fwrite(bd->blocks, 1, sizeof(bd->blocks), fp);
	fclose(fp);
	return true;
}

void fs_blockdevice_read(struct fs_blockdevice * bd, fs_block_id idx, struct fs_block * block) {
	memcpy(block, &bd->blocks[idx], sizeof(*block));
}

void fs_blockdevice_write(struct fs_blockdevice * bd, fs_block_id idx, struct fs_block * block) {
	memcpy(&bd->blocks[idx], block, sizeof(*block));
}

