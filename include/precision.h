/*
 * precision - Defines macros for toggling floating point precision of specific types at compile time.
 * Copyright 2014-2018 0x09.net.
 */

#ifndef PRECISION_H
#define PRECISION_H

#define F 1
#define D 2
#define L 4
#define FF (F<<3|F)
#define FD (D<<3|F)
#define DD (D<<3|D)
#define DL (L<<3|D)
#define LL (L<<3|L)

// can be specified individually or together with PRECISION
#ifndef COEFF_PRECISION
	#define COEFF_PRECISION (PRECISION & 7)
#endif

#ifndef INTERMEDIATE_PRECISION
	#define INTERMEDIATE_PRECISION (PRECISION >> 3)
#endif

#if INTERMEDIATE_PRECISION == 0
#undef INTERMEDIATE_PRECISION
#define INTERMEDIATE_PRECISION (COEFF_PRECISION << 1)
#endif

#if COEFF_PRECISION == F
	#define COEFF_TYPE float
	#define COEFF_NAME FLT
	#define COEFF_SUFFIX f
	#define COEFF_SPECIFIER "f"
	#define TypePixel FloatPixel
#elif COEFF_PRECISION == L
	#define COEFF_TYPE long double
	#define COEFF_NAME LDBL
	#define COEFF_SUFFIX l
	#define COEFF_SPECIFIER "Lf"
	// TypePixel not supported in this configuration
#else // D
	#define COEFF_TYPE double
	#define COEFF_NAME DBL
	#define COEFF_SUFFIX
	#define COEFF_SPECIFIER "lf"
	#define TypePixel DoublePixel
#endif

#if INTERMEDIATE_PRECISION == F
	#define INTERMEDIATE_TYPE float
	#define INTERMEDIATE_NAME FLT
	#define INTERMEDIATE_SUFFIX f
	#define INTERMEDIATE_SPECIFIER "f"
#elif INTERMEDIATE_PRECISION == D
	#define INTERMEDIATE_TYPE double
	#define INTERMEDIATE_NAME DBL
	#define INTERMEDIATE_SUFFIX
	#define INTERMEDIATE_SPECIFIER "lf"
#else // L
	#define INTERMEDIATE_TYPE long double
	#define INTERMEDIATE_NAME LDBL
	#define INTERMEDIATE_SUFFIX l
	#define INTERMEDIATE_SPECIFIER "Lf"
#endif

typedef INTERMEDIATE_TYPE intermediate;
typedef COEFF_TYPE coeff;

#define CAT_(x,y) x##y
#define CAT(x,y) CAT_(x,y)

// math.h / general call wrapper
#define mc(call) CAT(call,COEFF_SUFFIX)
#define mi(call) CAT(call,INTERMEDIATE_SUFFIX)

// fftw call wrapper
#define fftw(call) CAT(mc(fftw),_##call)

// for accessing float.h constants
#define COEFF_CONST(c) CAT(COEFF_NAME,_##c)
#define INTERMEDIATE_CONST(c) CAT(INTERMEDIATE_NAME,_##c)

// for namespacing declarations of functions to be called from another TU with a specific precision
#define precision_namespace(f) CAT(CAT(CAT(f##_##pc,COEFF_SUFFIX),i),INTERMEDIATE_SUFFIX)

#undef F
#undef D
#undef L
#undef FF
#undef FD
#undef DD
#undef DL
#undef LL

#endif
