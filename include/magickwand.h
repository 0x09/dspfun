/*
 * magickwand.h - Preprocessor #include wrapper for compatibility between ImageMagick versions 6 and 7.
 */

#if MAGICKWAND_VERSION > 6
#include <MagickWand/MagickWand.h>
#else
#include <wand/MagickWand.h>
#endif
