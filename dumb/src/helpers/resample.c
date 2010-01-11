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
 * resample.c - Resampling helpers.                   / / \  \
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



/* Make MSVC shut the hell up about if ( upd ) UPDATE_VOLUME() conditions being constant */
#ifdef _MSC_VER
#pragma warning(disable:4127 4701)
#endif



/* A global variable for controlling resampling quality wherever a local
 * specification doesn't override it. The following values are valid:
 *
 *  0 - DUMB_RQ_ALIASING - fastest
 *  1 - DUMB_RQ_LINEAR
 *  2 - DUMB_RQ_CUBIC
 *  3 - DUMB_RQ_SINC     - nicest
 *
 * Values outside the range 0-3 will behave the same as the nearest
 * value within the range.
 */
int dumb_resampling_quality = DUMB_RQ_CUBIC;



//#define MULSC(a, b) ((int)((LONG_LONG)(a) * (b) >> 16))
//#define MULSC(a, b) ((a) * ((b) >> 2) >> 14)
#define MULSCV(a, b) ((int)((LONG_LONG)(a) * (b) >> 32))
#define MULSC(a, b) ((int)((LONG_LONG)((a) << 4) * ((b) << 12) >> 32))
#define MULSC16(a, b) ((int)((LONG_LONG)((a) << 12) * ((b) << 12) >> 32))



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



#define PASTERAW(a, b) a ## b /* This does not expand macros in b ... */
#define PASTE(a, b) PASTERAW(a, b) /* ... but b is expanded during this substitution. */

#define X PASTE(x.x, SRCBITS)



/* Cubic resampler: look-up tables
 *
 * a = 1.5*x1 - 1.5*x2 + 0.5*x3 - 0.5*x0
 * b = 2*x2 + x0 - 2.5*x1 - 0.5*x3
 * c = 0.5*x2 - 0.5*x0
 * d = x1
 *
 * x = a*t*t*t + b*t*t + c*t + d
 *   = (-0.5*x0 + 1.5*x1 - 1.5*x2 + 0.5*x3) * t*t*t +
 *     (   1*x0 - 2.5*x1 + 2  *x2 - 0.5*x3) * t*t +
 *     (-0.5*x0          + 0.5*x2         ) * t +
 *     (            1*x1                  )
 *   = (-0.5*t*t*t + 1  *t*t - 0.5*t    ) * x0 +
 *     ( 1.5*t*t*t - 2.5*t*t         + 1) * x1 +
 *     (-1.5*t*t*t + 2  *t*t + 0.5*t    ) * x2 +
 *     ( 0.5*t*t*t - 0.5*t*t            ) * x3
 *   = A0(t) * x0 + A1(t) * x1 + A2(t) * x2 + A3(t) * x3
 *
 * A0, A1, A2 and A3 stay within the range [-1,1].
 * In the tables, they are scaled with 14 fractional bits.
 *
 * Turns out we don't need to store A2 and A3; they are symmetrical to A1 and A0.
 *
 * TODO: A0 and A3 stay very small indeed. Consider different scale/resolution?
 */

static short cubicA0[1025], cubicA1[1025];

/*static*/ void init_cubic(void)
{
	unsigned int t; /* 3*1024*1024*1024 is within range if it's unsigned */
	static int done = 0;
	if (done) return;
	done = 1;
	for (t = 0; t < 1025; t++) {
		/* int casts to pacify warnings about negating unsigned values */
		cubicA0[t] = -(int)(  t*t*t >> 17) + (int)(  t*t >> 6) - (int)(t << 3);
		cubicA1[t] =  (int)(3*t*t*t >> 17) - (int)(5*t*t >> 7)                 + (int)(1 << 14);
	}
}

static short sinc[8192];

#define WFIR_QUANTBITS		14
#define WFIR_QUANTSCALE		(1L<<WFIR_QUANTBITS)
#define WFIR_8SHIFT			(WFIR_QUANTBITS-8)
#define WFIR_16BITSHIFT		(WFIR_QUANTBITS)
// log2(number)-1 of precalculated taps range is [4..12]
#define WFIR_FRACBITS		9
#define WFIR_LUTLEN			((1L<<(WFIR_FRACBITS+1))/*+1*/)
// number of samples in window
#define WFIR_LOG2WIDTH		3
#define WFIR_WIDTH			(1L<<WFIR_LOG2WIDTH)
#define WFIR_SMPSPERWING	((WFIR_WIDTH-1)>>1)
// cutoff (1.0 == pi/2)
#define WFIR_CUTOFF			0.95f
// wfir type
#define WFIR_HANN			0
#define WFIR_HAMMING		1
#define WFIR_BLACKMANEXACT	2
#define WFIR_BLACKMAN3T61	3
#define WFIR_BLACKMAN3T67	4
#define WFIR_BLACKMAN4T92	5
#define WFIR_BLACKMAN4T74	6
#define WFIR_KAISER4T		7
#define WFIR_TYPE			WFIR_HANN
// wfir help
#ifndef M_zPI
#define M_zPI			3.1415926535897932384626433832795
#endif
#define M_zEPS			1e-8
#define M_zBESSELEPS	1e-21

static float sinc_coef( int _PCnr, float _POfs, float _PCut, int _PWidth, int _PType ) //float _PPos, float _PFc, int _PLen )
{
	double	_LWidthM1		= _PWidth-1;
	double	_LWidthM1Half	= 0.5*_LWidthM1;
	double	_LPosU			= ((double)_PCnr - _POfs);
	double	_LPos			= _LPosU-_LWidthM1Half;
	double	_LPIdl			= 2.0*M_zPI/_LWidthM1;
	double	_LWc,_LSi;
	if( fabs(_LPos)<M_zEPS )
	{
		_LWc	= 1.0;
		_LSi	= _PCut;
	}
	else
	{
		switch( _PType )
		{
		case WFIR_HANN:
			_LWc = 0.50 - 0.50 * cos(_LPIdl*_LPosU);
			break;
		case WFIR_HAMMING:
			_LWc = 0.54 - 0.46 * cos(_LPIdl*_LPosU);
			break;
		case WFIR_BLACKMANEXACT:
			_LWc = 0.42 - 0.50 * cos(_LPIdl*_LPosU) + 0.08 * cos(2.0*_LPIdl*_LPosU);
			break;
		case WFIR_BLACKMAN3T61:
			_LWc = 0.44959 - 0.49364 * cos(_LPIdl*_LPosU) + 0.05677 * cos(2.0*_LPIdl*_LPosU);
			break;
		case WFIR_BLACKMAN3T67:
			_LWc = 0.42323 - 0.49755 * cos(_LPIdl*_LPosU) + 0.07922 * cos(2.0*_LPIdl*_LPosU);
			break;
		case WFIR_BLACKMAN4T92:
			_LWc = 0.35875 - 0.48829 * cos(_LPIdl*_LPosU) + 0.14128 * cos(2.0*_LPIdl*_LPosU) - 0.01168 * cos(3.0*_LPIdl*_LPosU);
			break;
		case WFIR_BLACKMAN4T74:
			_LWc = 0.40217 - 0.49703 * cos(_LPIdl*_LPosU) + 0.09392 * cos(2.0*_LPIdl*_LPosU) - 0.00183 * cos(3.0*_LPIdl*_LPosU);
			break;
		case WFIR_KAISER4T:
			_LWc = 0.40243 - 0.49804 * cos(_LPIdl*_LPosU) + 0.09831 * cos(2.0*_LPIdl*_LPosU) - 0.00122 * cos(3.0*_LPIdl*_LPosU);
			break;
		default:
			_LWc = 1.0;
			break;
		}
		_LPos	 *= M_zPI;
		_LSi	 = sin(_PCut*_LPos)/_LPos;
	}
	return (float)(_LWc*_LSi);
}

/*static*/ void init_sinc(void)
{
	int _LPcl;
	float _LPcllen;
	float _LNorm;
	float _LCut;
	float _LScale;
	float _LGain,_LCoefs[WFIR_WIDTH];
	static int done = 0;
	if (done) return;
	done = 1;
	_LPcllen  = (float)(1L<<WFIR_FRACBITS);	// number of precalculated lines for 0..1 (-1..0)
	_LNorm    = 1.0f / (float)(2.0f * _LPcllen);
	_LCut     = WFIR_CUTOFF;
	_LScale   = (float)WFIR_QUANTSCALE;
	for( _LPcl=0;_LPcl<WFIR_LUTLEN;_LPcl++ )
	{
		float _LOfs		= ((float)_LPcl-_LPcllen)*_LNorm;
		int _LCc,_LIdx	= _LPcl<<WFIR_LOG2WIDTH;
		for( _LCc=0,_LGain=0.0f;_LCc<WFIR_WIDTH;_LCc++ )
		{
			_LGain	+= (_LCoefs[_LCc] = sinc_coef( _LCc, _LOfs, _LCut, WFIR_WIDTH, WFIR_TYPE ));
		}
		_LGain = 1.0f/_LGain;
		for( _LCc=0;_LCc<WFIR_WIDTH;_LCc++ )
		{
			float _LCoef = (float)floor( 0.5 + _LScale*_LCoefs[_LCc]*_LGain );
			sinc[_LIdx+_LCc] = (signed short)( (_LCoef<-_LScale)?-_LScale:((_LCoef>_LScale)?_LScale:_LCoef) );
		}
	}
}

#undef WFIR_QUANTBITS
#undef WFIR_QUANTSCALE
#undef WFIR_8SHIFT
#undef WFIR_16BITSHIFT
#undef WFIR_FRACBITS
#undef WFIR_LUTLEN
#undef WFIR_LOG2WIDTH
#undef WFIR_WIDTH
#undef WFIR_SMPSPERWING
#undef WFIR_CUTOFF
#undef WFIR_HANN
#undef WFIR_HAMMING
#undef WFIR_BLACKMANEXACT
#undef WFIR_BLACKMAN3T61
#undef WFIR_BLACKMAN3T67
#undef WFIR_BLACKMAN4T92
#undef WFIR_BLACKMAN4T74
#undef WFIR_KAISER4T
#undef WFIR_TYPE
#undef M_zPI
#undef M_zEPS
#undef M_zBESSELEPS

/* Create resamplers for 24-in-32-bit source samples. */

/* #define SUFFIX
 * MSVC warns if we try to paste a null SUFFIX, so instead we define
 * special macros for the function names that don't bother doing the
 * corresponding paste. The more generic definitions are further down.
 */
#define process_pickup PASTE(process_pickup, SUFFIX2)
#define dumb_resample PASTE(PASTE(dumb_resample, SUFFIX2), SUFFIX3)
#define dumb_resample_get_current_sample PASTE(PASTE(dumb_resample_get_current_sample, SUFFIX2), SUFFIX3)

#define SRCTYPE sample_t
#define SRCBITS 24
#define ALIAS(x, vol) MULSC(x, vol)
#define LINEAR(x0, x1) (x0 + MULSC(x1 - x0, subpos))
/*
#define SET_CUBIC_COEFFICIENTS(x0, x1, x2, x3) { \
	a = (3 * (x1 - x2) + (x3 - x0)) >> 1; \
	b = ((x2 << 2) + (x0 << 1) - (5 * x1 + x3)) >> 1; \
	c = (x2 - x0) >> 1; \
}
#define CUBIC(d) MULSC(MULSC(MULSC(MULSC(a, subpos) + b, subpos) + c, subpos) + d, vol)
*/
#define CUBIC(x0, x1, x2, x3) ( \
	MULSC(x0, cubicA0[subpos >> 6] << 2) + \
	MULSC(x1, cubicA1[subpos >> 6] << 2) + \
	MULSC(x2, cubicA1[1 + (subpos >> 6 ^ 1023)] << 2) + \
	MULSC(x3, cubicA0[1 + (subpos >> 6 ^ 1023)] << 2))
#define CUBICVOL(x, vol) MULSC(x, vol)
#define SINC(x0, x1, x2, x3, x4, x5, x6, x7) ( \
	MULSC(x0, sinc[(subpos >> 3) & 0x1FF8] << 2) + \
	MULSC(x1, sinc[((subpos >> 3) & 0x1FF8) + 1] << 2) + \
	MULSC(x2, sinc[((subpos >> 3) & 0x1FF8) + 2] << 2) + \
	MULSC(x3, sinc[((subpos >> 3) & 0x1FF8) + 3] << 2) + \
	MULSC(x4, sinc[((subpos >> 3) & 0x1FF8) + 4] << 2) + \
	MULSC(x5, sinc[((subpos >> 3) & 0x1FF8) + 5] << 2) + \
	MULSC(x6, sinc[((subpos >> 3) & 0x1FF8) + 6] << 2) + \
	MULSC(x7, sinc[((subpos >> 3) & 0x1FF8) + 7] << 2))
#define SINCVOL(x, vol) MULSC(x, vol)
#include "resample.inc"

/* Undefine the simplified macros. */
#undef dumb_resample_get_current_sample
#undef dumb_resample
#undef process_pickup


/* Now define the proper ones that use SUFFIX. */
#define dumb_reset_resampler PASTE(dumb_reset_resampler, SUFFIX)
#define dumb_start_resampler PASTE(dumb_start_resampler, SUFFIX)
#define process_pickup PASTE(PASTE(process_pickup, SUFFIX), SUFFIX2)
#define dumb_resample PASTE(PASTE(PASTE(dumb_resample, SUFFIX), SUFFIX2), SUFFIX3)
#define dumb_resample_get_current_sample PASTE(PASTE(PASTE(dumb_resample_get_current_sample, SUFFIX), SUFFIX2), SUFFIX3)
#define dumb_end_resampler PASTE(dumb_end_resampler, SUFFIX)

/* Create resamplers for 16-bit source samples. */
#define SUFFIX _16
#define SRCTYPE short
#define SRCBITS 16
#define ALIAS(x, vol) (x * vol >> 8)
#define LINEAR(x0, x1) ((x0 << 8) + MULSC16(x1 - x0, subpos))
/*
#define SET_CUBIC_COEFFICIENTS(x0, x1, x2, x3) { \
	a = (3 * (x1 - x2) + (x3 - x0)) << 7; \
	b = ((x2 << 2) + (x0 << 1) - (5 * x1 + x3)) << 7; \
	c = (x2 - x0) << 7; \
}
#define CUBIC(d) MULSC(MULSC(MULSC(MULSC(a, subpos) + b, subpos) + c, subpos) + (d << 8), vol)
*/
#define CUBIC(x0, x1, x2, x3) ( \
	x0 * cubicA0[subpos >> 6] + \
	x1 * cubicA1[subpos >> 6] + \
	x2 * cubicA1[1 + (subpos >> 6 ^ 1023)] + \
	x3 * cubicA0[1 + (subpos >> 6 ^ 1023)])
#define CUBICVOL(x, vol) (int)((LONG_LONG)(x) * (vol << 10) >> 32)
#define SINC(x0, x1, x2, x3, x4, x5, x6, x7) ( \
	x0 * sinc[(subpos >> 3) & 0x1FF8] + \
	x1 * sinc[((subpos >> 3) & 0x1FF8) + 1] + \
	x2 * sinc[((subpos >> 3) & 0x1FF8) + 2] + \
	x3 * sinc[((subpos >> 3) & 0x1FF8) + 3] + \
	x4 * sinc[((subpos >> 3) & 0x1FF8) + 4] + \
	x5 * sinc[((subpos >> 3) & 0x1FF8) + 5] + \
	x6 * sinc[((subpos >> 3) & 0x1FF8) + 6] + \
	x7 * sinc[((subpos >> 3) & 0x1FF8) + 7])
#define SINCVOL(x, vol) (int)((LONG_LONG)(x) * (vol << 10) >> 32)
#include "resample.inc"

/* Create resamplers for 8-bit source samples. */
#define SUFFIX _8
#define SRCTYPE signed char
#define SRCBITS 8
#define ALIAS(x, vol) (x * vol)
#define LINEAR(x0, x1) ((x0 << 16) + (x1 - x0) * subpos)
/*
#define SET_CUBIC_COEFFICIENTS(x0, x1, x2, x3) { \
	a = 3 * (x1 - x2) + (x3 - x0); \
	b = ((x2 << 2) + (x0 << 1) - (5 * x1 + x3)) << 15; \
	c = (x2 - x0) << 15; \
}
#define CUBIC(d) MULSC(MULSC(MULSC((a * subpos >> 1) + b, subpos) + c, subpos) + (d << 16), vol)
*/
#define CUBIC(x0, x1, x2, x3) (( \
	x0 * cubicA0[subpos >> 6] + \
	x1 * cubicA1[subpos >> 6] + \
	x2 * cubicA1[1 + (subpos >> 6 ^ 1023)] + \
	x3 * cubicA0[1 + (subpos >> 6 ^ 1023)]) << 6)
#define CUBICVOL(x, vol) (int)((LONG_LONG)(x) * (vol << 12) >> 32)
#define SINC(x0, x1, x2, x3, x4, x5, x6, x7) (( \
	x0 * sinc[(subpos >> 3) & 0x1FF8] + \
	x1 * sinc[((subpos >> 3) & 0x1FF8) + 1] + \
	x2 * sinc[((subpos >> 3) & 0x1FF8) + 2] + \
	x3 * sinc[((subpos >> 3) & 0x1FF8) + 3] + \
	x4 * sinc[((subpos >> 3) & 0x1FF8) + 4] + \
	x5 * sinc[((subpos >> 3) & 0x1FF8) + 5] + \
	x6 * sinc[((subpos >> 3) & 0x1FF8) + 6] + \
	x7 * sinc[((subpos >> 3) & 0x1FF8) + 7]) << 6)
#define SINCVOL(x, vol) (int)((LONG_LONG)(x) * (vol << 12) >> 32)
#include "resample.inc"


#undef dumb_reset_resampler
#undef dumb_start_resampler
#undef process_pickup
#undef dumb_resample
#undef dumb_resample_get_current_sample
#undef dumb_end_resampler



void dumb_reset_resampler_n(int n, DUMB_RESAMPLER *resampler, void *src, int src_channels, long pos, long start, long end, int quality)
{
	if (n == 8)
		dumb_reset_resampler_8(resampler, src, src_channels, pos, start, end, quality);
	else if (n == 16)
		dumb_reset_resampler_16(resampler, src, src_channels, pos, start, end, quality);
	else
		dumb_reset_resampler(resampler, src, src_channels, pos, start, end, quality);
}



DUMB_RESAMPLER *dumb_start_resampler_n(int n, void *src, int src_channels, long pos, long start, long end, int quality)
{
	if (n == 8)
		return dumb_start_resampler_8(src, src_channels, pos, start, end, quality);
	else if (n == 16)
		return dumb_start_resampler_16(src, src_channels, pos, start, end, quality);
	else
		return dumb_start_resampler(src, src_channels, pos, start, end, quality);
}



long dumb_resample_n_1_1(int n, DUMB_RESAMPLER *resampler, sample_t *dst, long dst_size, DUMB_VOLUME_RAMP_INFO * volume, float delta)
{
	if (n == 8)
		return dumb_resample_8_1_1(resampler, dst, dst_size, volume, delta);
	else if (n == 16)
		return dumb_resample_16_1_1(resampler, dst, dst_size, volume, delta);
	else
		return dumb_resample_1_1(resampler, dst, dst_size, volume, delta);
}



long dumb_resample_n_1_2(int n, DUMB_RESAMPLER *resampler, sample_t *dst, long dst_size, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, float delta)
{
	if (n == 8)
		return dumb_resample_8_1_2(resampler, dst, dst_size, volume_left, volume_right, delta);
	else if (n == 16)
		return dumb_resample_16_1_2(resampler, dst, dst_size, volume_left, volume_right, delta);
	else
		return dumb_resample_1_2(resampler, dst, dst_size, volume_left, volume_right, delta);
}



long dumb_resample_n_2_1(int n, DUMB_RESAMPLER *resampler, sample_t *dst, long dst_size, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, float delta)
{
	if (n == 8)
		return dumb_resample_8_2_1(resampler, dst, dst_size, volume_left, volume_right, delta);
	else if (n == 16)
		return dumb_resample_16_2_1(resampler, dst, dst_size, volume_left, volume_right, delta);
	else
		return dumb_resample_2_1(resampler, dst, dst_size, volume_left, volume_right, delta);
}



long dumb_resample_n_2_2(int n, DUMB_RESAMPLER *resampler, sample_t *dst, long dst_size, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, float delta)
{
	if (n == 8)
		return dumb_resample_8_2_2(resampler, dst, dst_size, volume_left, volume_right, delta);
	else if (n == 16)
		return dumb_resample_16_2_2(resampler, dst, dst_size, volume_left, volume_right, delta);
	else
		return dumb_resample_2_2(resampler, dst, dst_size, volume_left, volume_right, delta);
}



void dumb_resample_get_current_sample_n_1_1(int n, DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume, sample_t *dst)
{
	if (n == 8)
		dumb_resample_get_current_sample_8_1_1(resampler, volume, dst);
	else if (n == 16)
		dumb_resample_get_current_sample_16_1_1(resampler, volume, dst);
	else
		dumb_resample_get_current_sample_1_1(resampler, volume, dst);
}



void dumb_resample_get_current_sample_n_1_2(int n, DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, sample_t *dst)
{
	if (n == 8)
		dumb_resample_get_current_sample_8_1_2(resampler, volume_left, volume_right, dst);
	else if (n == 16)
		dumb_resample_get_current_sample_16_1_2(resampler, volume_left, volume_right, dst);
	else
		dumb_resample_get_current_sample_1_2(resampler, volume_left, volume_right, dst);
}



void dumb_resample_get_current_sample_n_2_1(int n, DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, sample_t *dst)
{
	if (n == 8)
		dumb_resample_get_current_sample_8_2_1(resampler, volume_left, volume_right, dst);
	else if (n == 16)
		dumb_resample_get_current_sample_16_2_1(resampler, volume_left, volume_right, dst);
	else
		dumb_resample_get_current_sample_2_1(resampler, volume_left, volume_right, dst);
}



void dumb_resample_get_current_sample_n_2_2(int n, DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, sample_t *dst)
{
	if (n == 8)
		dumb_resample_get_current_sample_8_2_2(resampler, volume_left, volume_right, dst);
	else if (n == 16)
		dumb_resample_get_current_sample_16_2_2(resampler, volume_left, volume_right, dst);
	else
		dumb_resample_get_current_sample_2_2(resampler, volume_left, volume_right, dst);
}



void dumb_end_resampler_n(int n, DUMB_RESAMPLER *resampler)
{
	if (n == 8)
		dumb_end_resampler_8(resampler);
	else if (n == 16)
		dumb_end_resampler_16(resampler);
	else
		dumb_end_resampler(resampler);
}
