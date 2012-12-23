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
 * readany.c - Code to detect and read any of the     / / \  \
 *             module formats supported by DUMB.     | <  /   \_
 *                                                   |  \/ /\   /
 * By Chris Moeller.                                  \_  /  > /
 *                                                      | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */

#include <stdlib.h>
#include <string.h>

#include "dumb.h"

#ifdef _MSC_VER
#define strnicmp _strnicmp
#else
#define strnicmp strncasecmp
#endif

typedef struct BUFFERED_MOD BUFFERED_MOD;

struct BUFFERED_MOD
{
    unsigned char *buffered;
    long ptr, len;
    DUMBFILE *remaining;
};



static int buffer_mod_skip(void *f, long n)
{
    BUFFERED_MOD *bm = f;
    if (bm->buffered) {
        bm->ptr += n;
        if (bm->ptr >= bm->len) {
            free(bm->buffered);
            bm->buffered = NULL;
            return dumbfile_skip(bm->remaining, bm->ptr - bm->len);
        }
        return 0;
    }
    return dumbfile_skip(bm->remaining, n);
}



static int buffer_mod_getc(void *f)
{
    BUFFERED_MOD *bm = f;
    if (bm->buffered) {
        int rv = bm->buffered[bm->ptr++];
        if (bm->ptr >= bm->len) {
            free(bm->buffered);
            bm->buffered = NULL;
        }
        return rv;
    }
    return dumbfile_getc(bm->remaining);
}



static long buffer_mod_getnc(char *ptr, long n, void *f)
{
    BUFFERED_MOD *bm = f;
    if (bm->buffered) {
        int left = bm->len - bm->ptr;
        if (n >= left) {
            memcpy(ptr, bm->buffered + bm->ptr, left);
            free(bm->buffered);
            bm->buffered = NULL;
            if (n - left) {
                int rv = dumbfile_getnc(ptr + left, n - left, bm->remaining);
                return left + MAX(rv, 0);
            } else {
                return left;
            }
        }
        memcpy(ptr, bm->buffered + bm->ptr, n);
        bm->ptr += n;
        return n;
    }
    return dumbfile_getnc(ptr, n, bm->remaining);
}



static void buffer_mod_close(void *f)
{
    BUFFERED_MOD *bm = f;
    if (bm->buffered) free(bm->buffered);
    /* Do NOT close bm->remaining */
    free(f);
}



static DUMBFILE_SYSTEM buffer_mod_dfs = {
    NULL,
    &buffer_mod_skip,
    &buffer_mod_getc,
    &buffer_mod_getnc,
    &buffer_mod_close
};



static DUMBFILE *dumbfile_buffer_mod(DUMBFILE *f, unsigned char const* * signature, unsigned long * signature_size)
{
    long read;
    BUFFERED_MOD *bm = malloc(sizeof(*bm));
    if (!bm) return NULL;

    bm->buffered = malloc(32768);
    if (!bm->buffered) {
        free(bm);
        return NULL;
    }

    bm->len = 0;
    *signature_size = 0;

    read = dumbfile_getnc(bm->buffered, 32768, f);

    if (read >= 0) {
        bm->len += read;
        *signature_size += read;

        while (read >= 32768) {
            bm->buffered = realloc(bm->buffered, *signature_size + 32768);
            if (!bm->buffered) {
                free(bm);
                return 0;
            }
            read = dumbfile_getnc(bm->buffered + *signature_size, 32768, f);
            if (read >= 0) {
                bm->len += read;
                *signature_size += read;
            }
        }
    }

    if (*signature_size) {
        bm->ptr = 0;
        *signature = bm->buffered;
    } else {
        free(bm->buffered);
        bm->buffered = NULL;
    }

    bm->remaining = f;

    return dumbfile_open_ex(bm, &buffer_mod_dfs);
}



typedef struct tdumbfile_mem_status
{
    const unsigned char * ptr;
    unsigned offset, size;
} dumbfile_mem_status;



static int dumbfile_mem_skip(void * f, long n)
{
    dumbfile_mem_status * s = (dumbfile_mem_status *) f;
    s->offset += n;
    if (s->offset > s->size)
    {
        s->offset = s->size;
        return 1;
    }

    return 0;
}



static int dumbfile_mem_getc(void * f)
{
    dumbfile_mem_status * s = (dumbfile_mem_status *) f;
    if (s->offset < s->size)
    {
        return *(s->ptr + s->offset++);
    }
    return -1;
}



static long dumbfile_mem_getnc(char * ptr, long n, void * f)
{
    dumbfile_mem_status * s = (dumbfile_mem_status *) f;
    long max = s->size - s->offset;
    if (max > n) max = n;
    if (max)
    {
        memcpy(ptr, s->ptr + s->offset, max);
        s->offset += max;
    }
    return max;
}



static DUMBFILE_SYSTEM mem_dfs = {
    NULL, // open
    &dumbfile_mem_skip,
    &dumbfile_mem_getc,
    &dumbfile_mem_getnc,
    NULL // close
};



DUH *dumb_read_any_quick(DUMBFILE *f, int restrict, int subsong)
{
    unsigned char const* signature;
    unsigned long signature_size;
    DUMBFILE * rem;
    DUH * duh = NULL;

    rem = dumbfile_buffer_mod( f, &signature, &signature_size );
    if ( !rem ) return NULL;

    if (signature_size >= 4 &&
        signature[0] == 'I' && signature[1] == 'M' &&
        signature[2] == 'P' && signature[3] == 'M')
    {
        duh = dumb_read_it_quick( rem );
    }
    else if (signature_size >= 17 && !memcmp(signature, "Extended Module: ", 17))
    {
        duh = dumb_read_xm_quick( rem );
    }
    else if (signature_size >= 0x30 &&
        signature[0x2C] == 'S' && signature[0x2D] == 'C' &&
        signature[0x2E] == 'R' && signature[0x2F] == 'M')
    {
        duh = dumb_read_s3m_quick( rem );
    }
    else if (signature_size >= 30 &&
        /*signature[28] == 0x1A &&*/ signature[29] == 2 &&
        ( ! strnicmp( ( const char * ) signature + 20, "!Scream!", 8 ) ||
        ! strnicmp( ( const char * ) signature + 20, "BMOD2STM", 8 ) ||
        ! strnicmp( ( const char * ) signature + 20, "WUZAMOD!", 8 ) ) )
    {
        duh = dumb_read_stm_quick( rem );
    }
    else if (signature_size >= 2 &&
        ((signature[0] == 0x69 && signature[1] == 0x66) ||
        (signature[0] == 0x4A && signature[1] == 0x4E)))
    {
        duh = dumb_read_669_quick( rem );
    }
    else if (signature_size >= 0x30 &&
        signature[0x2C] == 'P' && signature[0x2D] == 'T' &&
        signature[0x2E] == 'M' && signature[0x2F] == 'F')
    {
        duh = dumb_read_ptm_quick( rem );
    }
    else if (signature_size >= 4 &&
        signature[0] == 'P' && signature[1] == 'S' &&
        signature[2] == 'M' && signature[3] == ' ')
    {
        duh = dumb_read_psm_quick( rem, subsong );
    }
    else if (signature_size >= 4 &&
        signature[0] == 'P' && signature[1] == 'S' &&
        signature[2] == 'M' && signature[3] == 254)
    {
        duh = dumb_read_old_psm_quick( rem );
    }
    else if (signature_size >= 3 &&
        signature[0] == 'M' && signature[1] == 'T' &&
        signature[2] == 'M')
    {
        duh = dumb_read_mtm_quick( rem );
    }
    else if ( signature_size >= 4 &&
        signature[0] == 'R' && signature[1] == 'I' &&
        signature[2] == 'F' && signature[3] == 'F')
    {
        duh = dumb_read_riff_quick( rem );
    }
    else if ( signature_size >= 24 &&
        !memcmp( signature, "ASYLUM Music Format", 19 ) &&
        !memcmp( signature + 19, " V1.0", 5 ) )
    {
        duh = dumb_read_asy_quick( rem );
    }
    else if ( signature_size >= 3 &&
        signature[0] == 'A' && signature[1] == 'M' &&
        signature[2] == 'F')
    {
        duh = dumb_read_amf_quick( rem );
    }
    else if ( signature_size >= 8 &&
        !memcmp( signature, "OKTASONG", 8 ) )
    {
        duh = dumb_read_okt_quick( rem );
    }

    if ( !duh )
    {
        dumbfile_mem_status memdata;
		DUMBFILE * memf;

        memdata.ptr = signature;
        memdata.offset = 0;
        memdata.size = signature_size;

        memf = dumbfile_open_ex(&memdata, &mem_dfs);
        if ( memf )
        {
            duh = dumb_read_mod_quick( memf, restrict );
            dumbfile_close( memf );
        }
    }

    dumbfile_close( rem );

    return duh;
}
