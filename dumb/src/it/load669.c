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
 * loadmod.c - Code to read a 669 Composer module     / / \  \
 *             file, opening and closing it for      | <  /   \_
 *             you.                                  |  \/ /\   /
 *                                                    \_  /  > /
 * By Chris Moeller, mostly based on loadmod.c          | \ / /
 * by entheh.                                           |  ' /
 *                                                       \__/
 */

#include "dumb.h"
#include "internal/it.h"



/* dumb_load_669(): loads a 669 file into a DUH struct, returning a pointer
 * to the DUH struct. When you have finished with it, you must pass the
 * pointer to unload_duh() so that the memory can be freed.
 */
DUH *dumb_load_669(const char *filename)
{
	DUH *duh;
	DUMBFILE *f = dumbfile_open(filename);

	if (!f)
		return NULL;

	duh = dumb_read_669(f);

	dumbfile_close(f);

	return duh;
}
