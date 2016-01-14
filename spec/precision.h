/*
 * spec - Generate invertible frequency spectrums for viewing and editing.
 * Copyright 2014-2016 0x09.net.
 */

#ifdef F
	typedef float coeff;
	typedef double intermediate;
	#define TypePixel FloatPixel
	#define INT
#else
	#define F
	typedef double coeff;
	typedef long double intermediate;
	#define TypePixel DoublePixel
	#define INT l
#endif

#define CAT_(x,y) x##y
#define CAT(x,y) CAT_(x,y)
#define mc(call) CAT(call,F)
#define mi(call) CAT(call,INT)
#define fftw(call) CAT(mc(fftw),_##call)