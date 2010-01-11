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
 * resample.inc - Resampling helper template.         / / \  \
 *                                                   | <  /   \_
 * By Bob and entheh.                                |  \/ /\   /
 *                                                    \_  /  > /
 * In order to find a good trade-off between            | \ / /
 * speed and accuracy in this code, some tests          |  ' /
 * were carried out regarding the behaviour of           \__/
 * long long ints with gcc. The following code
 * was tested:
 *
 * int a, b, c;
 * c = ((long long)a * b) >> 16;
 *
 * DJGPP GCC Version 3.0.3 generated the following assembly language code for
 * the multiplication and scaling, leaving the 32-bit result in EAX.
 *
 * movl  -8(%ebp), %eax    ; read one int into EAX
 * imull -4(%ebp)          ; multiply by the other; result goes in EDX:EAX
 * shrdl $16, %edx, %eax   ; shift EAX right 16, shifting bits in from EDX
 *
 * Note that a 32*32->64 multiplication is performed, allowing for high
 * accuracy. On the Pentium 2 and above, shrdl takes two cycles (generally),
 * so it is a minor concern when four multiplications are being performed
 * (the cubic resampler). On the Pentium MMX and earlier, it takes four or
 * more cycles, so this method is unsuitable for use in the low-quality
 * resamplers.
 *
 * Since "long long" is a gcc-specific extension, we use LONG_LONG instead,
 * defined in dumb.h. We may investigate later what code MSVC generates, but
 * if it seems too slow then we suggest you use a good compiler.
 *
 * FIXME: these comments are somewhat out of date now.
 */

#include <math.h>
#include "dumb.h"



/* Compile with -DHEAVYDEBUG if you want to make sure the pick-up function is
 * called when it should be. There will be a considerable performance hit,
 * since at least one condition has to be tested for every sample generated.
 */
#ifdef HEAVYDEBUG
#define HEAVYASSERT(cond) ASSERT(cond)
#else
#define HEAVYASSERT(cond)
#endif



//#define MULSC(a, b) ((int)((LONG_LONG)(a) * (b) >> 16))
//#define MULSC(a, b) ((a) * ((b) >> 2) >> 14)
#define MULSC(a, b) ((int)((LONG_LONG)((a) << 4) * ((b) << 12) >> 32))



void dumb_reset_resampler(DUMB_RESAMPLER *resampler, SRCTYPE *src, long pos, long start, long end, int quality)
{
	resampler->src = src;
	resampler->pos = pos;
	resampler->subpos = 0;
	resampler->start = start;
	resampler->end = end;
	resampler->dir = 1;
	resampler->pickup = NULL;
	resampler->pickup_data = NULL;
	if (quality < 0)
	{
		resampler->quality = 0;
	}
	else if (quality > DUMB_RQ_N_LEVELS - 1)
	{
		resampler->quality = DUMB_RQ_N_LEVELS - 1;
	}
	else
	{
		resampler->quality = quality;
	}
	resampler->X[2] = resampler->X[1] = resampler->X[0] = 0;
	resampler->overshot = -1;
}



DUMB_RESAMPLER *dumb_start_resampler(SRCTYPE *src, long pos, long start, long end, int quality)
{
	DUMB_RESAMPLER *resampler = malloc(sizeof(*resampler));
	if (!resampler) return NULL;
	dumb_reset_resampler(resampler, src, pos, start, end, quality);
	return resampler;
}



/* For convenience, returns nonzero on stop. */
static int process_pickup(DUMB_RESAMPLER *resampler)
{
	if (resampler->overshot < 0) {
		resampler->overshot = 0;
		dumb_resample(resampler, NULL, 2, 0, 0, 0, 0, 1.0f);
		resampler->X[0] = resampler->X[1];
	}

	for (;;) {
		SRCTYPE *src = resampler->src;

		if (resampler->dir < 0) {
			if (resampler->overshot >= 3 && resampler->pos+3 >= resampler->start) resampler->X[0] = src[resampler->pos+3];
			if (resampler->overshot >= 2 && resampler->pos+2 >= resampler->start) resampler->X[1] = src[resampler->pos+2];
			if (resampler->overshot >= 1 && resampler->pos+1 >= resampler->start) resampler->X[2] = src[resampler->pos+1];
			resampler->overshot = resampler->start - resampler->pos - 1;
		} else {
			if (resampler->overshot >= 3 && resampler->pos-3 < resampler->end) resampler->X[0] = src[resampler->pos-3];
			if (resampler->overshot >= 2 && resampler->pos-2 < resampler->end) resampler->X[1] = src[resampler->pos-2];
			if (resampler->overshot >= 1 && resampler->pos-1 < resampler->end) resampler->X[2] = src[resampler->pos-1];
			resampler->overshot = resampler->pos - resampler->end;
		}

		if (resampler->overshot < 0) {
			resampler->overshot = 0;
			return 0;
		}

		if (!resampler->pickup) {
			resampler->dir = 0;
			return 1;
		}
		(*resampler->pickup)(resampler, resampler->pickup_data);
		if (resampler->dir == 0) return 1;
		ASSERT(resampler->dir == -1 || resampler->dir == 1);
	}
}



/* Executes the content 'iterator' times.
 * Clobbers the 'iterator' variable.
 * The loop is unrolled by four.
 */
#define LOOP4(iterator, CONTENT) \
{ \
	if ((iterator) & 2) { \
		CONTENT; \
		CONTENT; \
	} \
	if ((iterator) & 1) { \
		CONTENT; \
	} \
	(iterator) >>= 2; \
	while (iterator) { \
		CONTENT; \
		CONTENT; \
		CONTENT; \
		CONTENT; \
		(iterator)--; \
	} \
}

#define UPDATE_VOLUME                                              \
	if (volume) {                                                  \
		if (volume_delta < 0) {                                    \
			if (volume_target - *volume > volume_delta) {          \
				*volume = volume_target;                           \
				volume = NULL;                                     \
				vol = (int)(volume_target * volume_mix * 65536.0 + 0.5);        \
			} else {                                               \
				*volume += volume_delta;                           \
				vol = (int)(*volume * volume_mix * 65536.0 + 0.5);              \
			}                                                      \
		} else {                                                   \
			if (volume_target - *volume < volume_delta) {          \
				*volume = volume_target;                           \
				volume = NULL;                                     \
				vol = (int)(volume_target * volume_mix * 65536.0 + 0.5);        \
			} else {                                               \
				*volume += volume_delta;                           \
				vol = (int)(*volume * volume_mix * 65536.0 + 0.5);              \
			}                                                      \
		}                                                          \
	}

long dumb_resample(DUMB_RESAMPLER *resampler, sample_t *dst, long dst_size, float * volume, float volume_delta, float volume_target, float volume_mix, float delta)
{
	int dt;
	int vol;
	long done;
	long todo;
	int quality;

	if (!resampler || resampler->dir == 0) return 0;
	ASSERT(resampler->dir == -1 || resampler->dir == 1);

	done = 0;
	dt = (int)(delta * 65536.0 + 0.5);
	if (volume) {
		vol = (int)(*volume * volume_mix * 65536.0 + 0.5);
		if (*volume == volume_target) volume = NULL;
	}
	else vol = 0;

	if (vol == 0 && volume_target == 0) dst = NULL;

	init_cubic();

	quality = resampler->quality;

	while (done < dst_size) {
		if (process_pickup(resampler)) return done;

		if ((resampler->dir ^ dt) < 0)
			dt = -dt;

		if (!dt)
			todo = 0;
		else if (resampler->dir < 0)
			todo = (long)((((LONG_LONG)(resampler->pos - resampler->start) << 16) + resampler->subpos - dt) / -dt);
		else
			todo = (long)((((LONG_LONG)(resampler->end - resampler->pos) << 16) - resampler->subpos - 1 + dt) / dt);

		if ((todo <= 0) ||
			(todo > dst_size - done))
			todo = dst_size - done;

		done += todo;

		{
			SRCTYPE *src = resampler->src;
			long pos = resampler->pos;
			int subpos = resampler->subpos;
			long diff = pos;
			long overshot;
			if (resampler->dir < 0) {
				if (!dst) {
					/* Silence or simulation */
					LONG_LONG new_subpos = subpos + (LONG_LONG)dt * todo;
					pos += (long)(new_subpos >> 16);
					subpos = (long)new_subpos & 65535;
				} else if (quality <= DUMB_RQ_ALIASING) {
					/* Aliasing, backwards */
					SRCTYPE xbuf[2];
					SRCTYPE *x = &xbuf[0];
					SRCTYPE *xstart;
					xbuf[0] = resampler->X[1];
					xbuf[1] = resampler->X[2];
					while (todo && x < &xbuf[2]) {
						HEAVYASSERT(pos >= resampler->start);
						*dst++ += ALIAS(x[0]);
						subpos += dt;
						pos += subpos >> 16;
						x -= subpos >> 16;
						subpos &= 65535;
						todo--;
						UPDATE_VOLUME;
					}
					x = xstart = &src[pos];
					LOOP4(todo,
						*dst++ += ALIAS(x[2]);
						subpos += dt;
						x += subpos >> 16;
						subpos &= 65535;
						UPDATE_VOLUME;
					);
					pos += x - xstart;
				} else if (quality <= DUMB_RQ_LINEAR) {
					/* Linear interpolation, backwards */
					SRCTYPE xbuf[3];
					SRCTYPE *x = &xbuf[1];
					xbuf[0] = resampler->X[1];
					xbuf[1] = resampler->X[2];
					xbuf[2] = src[pos];
					while (todo && x < &xbuf[3]) {
						HEAVYASSERT(pos >= resampler->start);
						*dst++ += LINEAR(x[0], x[-1]);
						subpos += dt;
						pos += subpos >> 16;
						x -= subpos >> 16;
						subpos &= 65535;
						todo--;
						UPDATE_VOLUME;
					}
					x = &src[pos];
					LOOP4(todo,
						HEAVYASSERT(pos >= resampler->start);
						*dst++ += LINEAR(x[1], x[2]);
						subpos += dt;
						pos += subpos >> 16;
						x += subpos >> 16;
						subpos &= 65535;
						UPDATE_VOLUME;
					);
				} else {
					/* Cubic interpolation, backwards */
					SRCTYPE xbuf[6];
					SRCTYPE *x = &xbuf[3];
					//SRCTYPE *lastx = NULL;
					//int a = 0, b = 0, c = 0;
					xbuf[0] = resampler->X[0];
					xbuf[1] = resampler->X[1];
					xbuf[2] = resampler->X[2];
					xbuf[3] = src[pos];
					if (pos-1 >= resampler->start) xbuf[4] = src[pos-1];
					if (pos-2 >= resampler->start) xbuf[5] = src[pos-2];
					while (todo && x < &xbuf[6]) {
						HEAVYASSERT(pos >= resampler->start);
						//if (lastx != x) {
						//	lastx = x;
						//	SET_CUBIC_COEFFICIENTS(x[0], x[-1], x[-2], x[-3]);
						//}
						*dst++ += CUBIC(x[0], x[-1], x[-2], x[-3]);
						subpos += dt;
						pos += subpos >> 16;
						x -= subpos >> 16;
						subpos &= 65535;
						todo--;
						UPDATE_VOLUME;
					}
					x = &src[pos];
					//lastx = NULL;
					LOOP4(todo,
						HEAVYASSERT(pos >= resampler->start);
						//if (lastx != x) {
						//	lastx = x;
						//	SET_CUBIC_COEFFICIENTS(x[0], x[1], x[2], x[3]);
						//}
						*dst++ += CUBIC(x[0], x[1], x[2], x[3]);
						subpos += dt;
						pos += subpos >> 16;
						x += subpos >> 16;
						subpos &= 65535;
						UPDATE_VOLUME;
					);
				}
				diff = diff - pos;
				overshot = resampler->start - pos - 1;
				if (diff >= 3) {
					resampler->X[0] = overshot >= 3 ? 0 : src[pos+3];
					resampler->X[1] = overshot >= 2 ? 0 : src[pos+2];
					resampler->X[2] = overshot >= 1 ? 0 : src[pos+1];
				} else if (diff >= 2) {
					resampler->X[0] = resampler->X[2];
					resampler->X[1] = overshot >= 2 ? 0 : src[pos+2];
					resampler->X[2] = overshot >= 1 ? 0 : src[pos+1];
				} else if (diff >= 1) {
					resampler->X[0] = resampler->X[1];
					resampler->X[1] = resampler->X[2];
					resampler->X[2] = overshot >= 1 ? 0 : src[pos+1];
				}
			} else {
				if (!dst) {
					/* Silence or simulation */
					LONG_LONG new_subpos = subpos + (LONG_LONG)dt * todo;
					pos += (long)(new_subpos >> 16);
					subpos = (long)new_subpos & 65535;
				} else if (quality <= DUMB_RQ_ALIASING) {
					/* Aliasing, forwards */
					SRCTYPE xbuf[2];
					SRCTYPE *x = &xbuf[0];
					SRCTYPE *xstart;
					xbuf[0] = resampler->X[1];
					xbuf[1] = resampler->X[2];
					while (todo && x < &xbuf[2]) {
						HEAVYASSERT(pos < resampler->end);
						*dst++ += ALIAS(x[0]);
						subpos += dt;
						pos += subpos >> 16;
						x += subpos >> 16;
						subpos &= 65535;
						todo--;
						UPDATE_VOLUME;
					}
					x = xstart = &src[pos];
					LOOP4(todo,
						*dst++ += ALIAS(x[-2]);
						subpos += dt;
						x += subpos >> 16;
						subpos &= 65535;
						UPDATE_VOLUME;
					);
					pos += x - xstart;
				} else if (quality <= DUMB_RQ_LINEAR) {
					/* Linear interpolation, forwards */
					SRCTYPE xbuf[3];
					SRCTYPE *x = &xbuf[1];
					xbuf[0] = resampler->X[1];
					xbuf[1] = resampler->X[2];
					xbuf[2] = src[pos];
					while (todo && x < &xbuf[3]) {
						HEAVYASSERT(pos < resampler->end);
						*dst++ += LINEAR(x[-1], x[0]);
						subpos += dt;
						pos += subpos >> 16;
						x += subpos >> 16;
						subpos &= 65535;
						todo--;
						UPDATE_VOLUME;
					}
					x = &src[pos];
					LOOP4(todo,
						HEAVYASSERT(pos < resampler->end);
						*dst++ += LINEAR(x[-2], x[-1]);
						subpos += dt;
						pos += subpos >> 16;
						x += subpos >> 16;
						subpos &= 65535;
						UPDATE_VOLUME;
					);
				} else {
					/* Cubic interpolation, forwards */
					SRCTYPE xbuf[6];
					SRCTYPE *x = &xbuf[3];
					//SRCTYPE *lastx = NULL;
					//int a = 0, b = 0, c = 0;
					xbuf[0] = resampler->X[0];
					xbuf[1] = resampler->X[1];
					xbuf[2] = resampler->X[2];
					xbuf[3] = src[pos];
					if (pos+1 < resampler->end) xbuf[4] = src[pos+1];
					if (pos+2 < resampler->end) xbuf[5] = src[pos+2];
					while (todo && x < &xbuf[6]) {
						HEAVYASSERT(pos < resampler->end);
						//if (lastx != x) {
						//	lastx = x;
						//	SET_CUBIC_COEFFICIENTS(x[-3], x[-2], x[-1], x[0]);
						//}
						*dst++ += CUBIC(x[-3], x[-2], x[-1], x[0]);
						subpos += dt;
						pos += subpos >> 16;
						x += subpos >> 16;
						subpos &= 65535;
						todo--;
						UPDATE_VOLUME;
					}
					x = &src[pos];
					//lastx = NULL;
					LOOP4(todo,
						HEAVYASSERT(pos < resampler->end);
						//if (lastx != x) {
						//	lastx = x;
						//	SET_CUBIC_COEFFICIENTS(x[-3], x[-2], x[-1], x[0]);
						//}
						*dst++ += CUBIC(x[-3], x[-2], x[-1], x[0]);
						subpos += dt;
						pos += subpos >> 16;
						x += subpos >> 16;
						subpos &= 65535;
						UPDATE_VOLUME;
					);
				}
				diff = pos - diff;
				overshot = pos - resampler->end;
				if (diff >= 3) {
					resampler->X[0] = overshot >= 3 ? 0 : src[pos-3];
					resampler->X[1] = overshot >= 2 ? 0 : src[pos-2];
					resampler->X[2] = overshot >= 1 ? 0 : src[pos-1];
				} else if (diff >= 2) {
					resampler->X[0] = resampler->X[2];
					resampler->X[1] = overshot >= 2 ? 0 : src[pos-2];
					resampler->X[2] = overshot >= 1 ? 0 : src[pos-1];
				} else if (diff >= 1) {
					resampler->X[0] = resampler->X[1];
					resampler->X[1] = resampler->X[2];
					resampler->X[2] = overshot >= 1 ? 0 : src[pos-1];
				}
			}
			resampler->pos = pos;
			resampler->subpos = subpos;
		}
	}

	return done;
}



sample_t dumb_resample_get_current_sample(DUMB_RESAMPLER *resampler, float volume)
{
	int vol;
	SRCTYPE *src;
	long pos;
	int subpos;
	int quality;

	if (!resampler || resampler->dir == 0) return 0;
	ASSERT(resampler->dir == -1 || resampler->dir == 1);

	if (process_pickup(resampler)) return 0;

	vol = (int)floor(volume * 65536.0 + 0.5);
	if (vol == 0) return 0;

	init_cubic();

	quality = resampler->quality;

	src = resampler->src;
	pos = resampler->pos;
	subpos = resampler->subpos;

	if (resampler->dir < 0) {
		HEAVYASSERT(pos >= resampler->start);
		if (quality <= DUMB_RQ_ALIASING) {
			/* Aliasing, backwards */
			return ALIAS(src[pos]);
		} else if (quality <= DUMB_RQ_LINEAR) {
			/* Linear interpolation, backwards */
			return LINEAR(resampler->X[2], resampler->X[1]);
		} else {
			/* Cubic interpolation, backwards */
			SRCTYPE *x = resampler->X;
			//int a, b, c;
			//SET_CUBIC_COEFFICIENTS(src[pos], x[2], x[1], x[0]);
			return CUBIC(src[pos], x[2], x[1], x[0]);
		}
	} else {
		HEAVYASSERT(pos < resampler->end);
		if (quality <= DUMB_RQ_ALIASING) {
			/* Aliasing */
			return ALIAS(src[pos]);
		} else if (quality <= DUMB_RQ_LINEAR) {
			/* Linear interpolation, forwards */
			return LINEAR(resampler->X[1], resampler->X[2]);
		} else {
			/* Cubic interpolation, forwards */
			SRCTYPE *x = resampler->X;
			//int a, b, c;
			//SET_CUBIC_COEFFICIENTS(x[0], x[1], x[2], src[pos]);
			return CUBIC(x[0], x[1], x[2], src[pos]);
		}
	}
}



void dumb_end_resampler(DUMB_RESAMPLER *resampler)
{
	if (resampler)
		free(resampler);
}



#undef CUBIC
//#undef SET_CUBIC_COEFFICIENTS
#undef LINEAR
#undef ALIAS
#undef SRCBITS
#undef SRCTYPE
#undef SUFFIX
