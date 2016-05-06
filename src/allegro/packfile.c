/*  _______         ____    __         ___    ___
 * \    _  \       \    /  \  /       \   \  /   /       '   '  '
 *  |  | \  \       |  |    ||         |   \/   |         .      .
 *  |  |  |  |      |  |    ||         ||\  /|  |
 *  |  |  |  |      |  |    ||         || \/ |  |         '  '  '
 *  |  |  |  |      |  |    ||         ||    |  |         .      .
 *  |  |_/  /        \  \__//          ||    |  |
 * /_______/ynamic    \____/niversal  /__\  /____\usic   /|  .  . ibliotheque
 *                                                      /  \
 *                                                     / .  \
 * packfile.c - Packfile input module.                / / \  \
 *                                                   | <  /   \_
 * By entheh.                                        |  \/ /\   /
 *                                                    \_  /  > /
 * Note that this does not use file compression;        | \ / /
 * for that you must open the file yourself and         |  ' /
 * then use dumbfile_open_packfile().                    \__/
 */

#include <allegro.h>

#include "aldumb.h"


typedef struct dumb_packfile
{
	FILE * file;
	long size;
} dumb_packfile;


static void *dumb_packfile_open(const char *filename)
{
	dumb_packfile * file = ( dumb_packfile * ) malloc( sizeof(dumb_packfile) );
	if ( !file ) return 0;
	file->file = fopen(filename, "rb");
	fseek(file->file, 0, SEEK_END);
	file->size = ftell(file->file);
	fseek(file->file, 0, SEEK_SET);
	return file;
}



static int dumb_packfile_skip(void *f, long n)
{
	dumb_packfile * file = ( dumb_packfile * ) f;
	return fseek(file->file, n, SEEK_CUR);
}



static int dumb_packfile_getc(void *f)
{
	dumb_packfile * file = ( dumb_packfile * ) f;
	return fgetc(file->file);
}



static long dumb_packfile_getnc(char *ptr, long n, void *f)
{
	dumb_packfile * file = ( dumb_packfile * ) f;
	return fread(ptr, 1, n, file->file);
}



static void dumb_packfile_close(void *f)
{
	dumb_packfile * file = ( dumb_packfile * ) f;
	fclose(file->file);
	free(f);
}

static void dumb_packfile_noclose(void *f)
{
	free(f);
}

static int dumb_packfile_seek(void *f, long n)
{
	dumb_packfile * file = (dumb_packfile *) f;
	return fseek(file->file, n, SEEK_SET);
}

static long dumb_packfile_get_size(void *f)
{
	dumb_packfile * file = (dumb_packfile *) f;
	return file->size;
}

static DUMBFILE_SYSTEM packfile_dfs = {
	&dumb_packfile_open,
	&dumb_packfile_skip,
	&dumb_packfile_getc,
	&dumb_packfile_getnc,
	&dumb_packfile_close,
	&dumb_packfile_seek,
	&dumb_packfile_get_size
};



void dumb_register_packfiles(void)
{
	register_dumbfile_system(&packfile_dfs);
}



static DUMBFILE_SYSTEM packfile_dfs_leave_open = {
	NULL,
	&dumb_packfile_skip,
	&dumb_packfile_getc,
	&dumb_packfile_getnc,
	&dumb_packfile_noclose,
	&dumb_packfile_seek,
	&dumb_packfile_get_size
};



DUMBFILE *dumbfile_open_packfile(PACKFILE *p)
{
	return dumbfile_open_ex(p, &packfile_dfs_leave_open);
}



DUMBFILE *dumbfile_from_packfile(PACKFILE *p)
{
	return p ? dumbfile_open_ex(p, &packfile_dfs) : NULL;
}
