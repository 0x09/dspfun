/*
 * speclib - Library for scaling DCT spectrograms.
 */

#include "precision.h"
#include "keyed_enum.h"

#include <stddef.h>
#include <stdbool.h>

#define signtype(X,type)\
	X(type,abs)\
	X(type,shift)\
	X(type,saturate)

#define scaletype(X,type)\
	X(type,linear)\
	X(type,log)

enum_gen_enum(signtype)
enum_gen_enum(scaletype)

struct spec_params {
	enum scaletype scaletype;
	enum signtype signtype;
};

struct spec_scaler;

#define spec_normalization    precision_namespace(spec_normalization)
#define spec_normalization_nd precision_namespace(spec_normalization_nd)
#define spec_create           precision_namespace(spec_create)
#define spec_scale            precision_namespace(spec_scale)
#define spec_descale          precision_namespace(spec_descale)

const char** spec_options(void);
const char** spec_option_values(const char* option);

// NULL on success or the pointer to the invalid input field
const char* spec_param_parse(struct spec_params* p, const char* key, const char* val);
// NULL on success or the index in the options string if an option is invalid
const char* spec_params_parse(struct spec_params*, const char* options, const char* key_val_sep, const char* pairs_sep);

// returns a coefficient for putting unnormalized transforms into uniform range
// normalization depends on the number of AC components for a given coordinate
// n-d normalization can be defined as 1d(x) * 1d(y) * ..., but using the sum of nonzero indices gives us greater precision
intermediate spec_normalization(size_t n);
#define spec_normalization_3d(x,y,z) spec_normalization(!!(x)+!!(y)+!!(z))
#define spec_normalization_2d(x,y) spec_normalization_3d(x,y,0)
#define spec_normalization_1d(x) spec_normalization_2d(x,0)
intermediate spec_normalization_nd(size_t ndims, size_t dims[ndims]);

struct spec_scaler* spec_create(struct spec_params* params, coeff max, coeff gain);
void spec_destroy(struct spec_scaler*);

intermediate spec_scale(struct spec_scaler*, intermediate);
intermediate spec_unscale(struct spec_scaler*, intermediate);

// returns c with the sign "sign" from a corresponding spectrogram with spectype_sign
// apply this before unscaling signtype_abs coefficients
intermediate spec_copysign(intermediate c, intermediate sign);
