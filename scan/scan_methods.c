/*
 * scan - progressively reconstruct images using various frequency space scans.
 * Copyright 2018 0x09.net.
 */

#include "scan_precomputed.h"
#include "scan_methods.h"
#include "scan.h"

#include <libavutil/eval.h>

#include <string.h>
#include <math.h>
#include <time.h>

struct scan_precomputed* scan_precompute(struct scan_context* ctx);

// Total number of coordinate sets returned by this scan
static size_t limit_width(void* opaque, size_t width, size_t height)  { return width; }
static size_t limit_height(void* opaque, size_t width, size_t height) { return height; }
static size_t limit_max(void* opaque, size_t width, size_t height)    { return width > height ? width : height; }
static size_t limit_min(void* opaque, size_t width, size_t height)    { return width < height ? width : height; }
static size_t limit_sum(void* opaque, size_t width, size_t height)    { return width + height - 1; }
static size_t limit_mirror(void* opaque, size_t width, size_t height) { return (width < height ? width : height)*2-1; }
static size_t limit_precomputed(void* opaque, size_t width, size_t height) { return ((struct scan_precomputed*)opaque)->limit; }

// Exact number of coordinates returned by this scan for a specific index
static size_t interval_diag(void* opaque, size_t width, size_t height, size_t i) {
	size_t min = width < height ? width : height;
	size_t max = width > height ? width : height;
	return i < min ? i+1 : i < max ? min : min - (i - max) - 1;
}
static size_t interval_box(void* opaque, size_t width, size_t height, size_t i) {
	size_t xmax = i < width ? i : width-1;
	size_t ymax = i < height ? i : height-1;
	return xmax + ymax + 1;
}

static size_t interval_mirror(void* opaque, size_t width, size_t height, size_t i) {
	return i ?
			(i < width ? (height < width-i ? height : width-i) : 0) +
			(i < height ? (width < height-i ? width : height-i) : 0) :
		width < height ? width : height;
}
static size_t interval_ibox(void* opaque, size_t width, size_t height, size_t i) { return width + height - i*2; }
static size_t interval_precomputed(void* opaque, size_t width, size_t height, size_t i) { return ((struct scan_precomputed*)opaque)->intervals[i]; }

// Max number of coordinates returned by this scan for all indexes
static size_t max_interval_mirror(void* opaque, size_t width, size_t height) { return (width < height ? width : height)*2-1; }

static size_t max_interval_precomputed(void* opaque, size_t width, size_t height) {
	struct scan_precomputed* p = opaque;
	size_t max = 0;
	for(size_t i = 0; i < p->limit; i++)
		if(p->intervals[i] > max)
			max = p->intervals[i];
	return max;
}

// The scans themselves
static inline void scan_horiz(void* opaque, size_t width, size_t height, size_t i, size_t (*coords)[2]) {
	(*coords)[0] = i / width;
	(*coords)[1] = i % width;
}

static inline void scan_vert(void* opaque, size_t width, size_t height, size_t i, size_t (*coords)[2]) {
	(*coords)[0] = i % height;
	(*coords)[1] = i / height;
}

static inline size_t inv_triangular(size_t i) {
	return sqrt(i*2+0.25)-0.5;
}

static inline size_t triangular(size_t i) {
	return i*(i+1)/2;
}

static inline void scan_zigzag(void* opaque, size_t width, size_t height, size_t i, size_t (*coords)[2]) {
	size_t dx, dy,
	       min = width < height ? width : height,
	       min_triangular = triangular(min),
	       area = width * height;

	if(i < min_triangular) {
		dx = inv_triangular(i);
		dy = i - triangular(dx);
		if(!(dx % 2))
			dy = dx - dy;
		(*coords)[0] = dy;
		(*coords)[1] = dx - dy;
		return;
	}
	if(area - i <= min_triangular) {
		i = area - i - 1;
		dx = inv_triangular(i);
		dy = i - triangular(dx);
		if(!(((width+height-1) - dx - 1) % 2))
			dy = dx - dy;
		(*coords)[0] = (height-1) - dy;
		(*coords)[1] = (width-1) - (dx - dy);
		return;
	}

	dx = (i - min_triangular) / min;
	dy =  min - (i - (((i - min_triangular) / min)*min + min_triangular));
	if(!((dx+min) % 2))
		dy = min - dy + 1;
	if(width < height) {
		dy = min - dy + 1;
		(*coords)[0] = dx + dy;
		(*coords)[1] = width - dy;
		return;
	}
	(*coords)[0] = height - dy;
	(*coords)[1] = dx + dy;
}

static inline void scan_ordered(void* opaque, size_t width, size_t height, size_t i, size_t (*coords)[2]) {
	(*coords)[0] = ((size_t*)opaque)[i] / width;
	(*coords)[1] = ((size_t*)opaque)[i] % width;
}

static void scan_box(void* opaque, size_t width, size_t height, size_t i, size_t (*coords)[2]) {
	size_t ymax = i < height ? i : height-1;
	size_t xmax = i < width ? i : width-1;
	for(size_t y = 0; y < ymax; y++, coords++) {
		(*coords)[0] = y;
		(*coords)[1] = i;
	}
	for(size_t x = 0; x < xmax+1; x++, coords++) {
		(*coords)[0] = ymax;
		(*coords)[1] = x;
	}
}

static void scan_ibox(void* opaque, size_t width, size_t height, size_t i, size_t (*coords)[2]) {
	for(size_t x = i; x < width; x++, coords++) {
		(*coords)[0] = i;
		(*coords)[1] = x;
	}
	for(size_t y = i; y < height; y++, coords++) {
		(*coords)[0] = y;
		(*coords)[1] = i;
	}
}

static void scan_row(void* opaque, size_t width, size_t height, size_t i, size_t (*coords)[2]) {
	for(size_t x = 0; x < width; x++) {
		coords[x][0] = i;
		coords[x][1] = x;
	}
}

static void scan_col(void* opaque, size_t width, size_t height, size_t i, size_t (*coords)[2]) {
	for(size_t y = 0; y < height; y++) {
		coords[y][0] = y;
		coords[y][1] = i;
	}
}

static void scan_diag(void* opaque, size_t width, size_t height, size_t i, size_t (*coords)[2]) {
	for(size_t iy = (i < height ? i : height-1)+1, ix = i - (iy-1); iy > 0 && ix < width; iy--, ix++, coords++) {
		(*coords)[0] = iy-1;
		(*coords)[1] = ix;
	}
}

static void scan_mirror(void* opaque, size_t width, size_t height, size_t i, size_t (*coords)[2]) {
	size_t min = width < height ? width : height;
	if(i > 0) {
		if(i < width)
			for(size_t x = height < width-i ? height : width-i; x > 0; x--, coords++) {
				(*coords)[0] = x-1;
				(*coords)[1] = x+i-1;
			}
		if(i < height)
			for(size_t y = width < height-i ? width : height-i; y > 0; y--, coords++) {
				(*coords)[0] = y+i-1;
				(*coords)[1] = y-1;
			}
	}
	else
		for(size_t d = 0; d < min; d++)
			coords[d][1] = coords[d][0] = d;
}

static void scan_evali(void* opaque, size_t width, size_t height, size_t i, size_t (*coords)[2]) {
	AVExpr** expr = opaque;
	double result;
	double constants[4] = {i,width,height};
	result = rint(av_expr_eval(expr[1],constants,NULL));
	if(isnan(result) || isinf(result) || result < 0)
		(*coords)[0] = 0;
	else
		(*coords)[0] = (size_t)result % height;

	result = rint(av_expr_eval(expr[0],constants,NULL));
	if(isnan(result) || isinf(result) || result < 0)
		(*coords)[1] = 0;
	else
		(*coords)[1] = (size_t)result % width;
}

static void scan_precomputed(void* opaque, size_t width, size_t height, size_t i, size_t (*coords)[2]) {
	struct scan_precomputed* p = opaque;
	memcpy(coords,p->scans[i],sizeof(*coords)*p->intervals[i]);
}

// a-priori data needed by the scan if any
static void* init_random(size_t width, size_t height, size_t channels, coeff* coeffs, const char* args) {
	size_t len = width*height;
	size_t* ctx = malloc(sizeof(size_t)*len);
	if(!ctx)
		goto end;

	unsigned int seed = args ? strtoul(args, NULL, 10) : time(NULL);
	srand(seed);
	for(size_t i = 0; i < len; i++)
		ctx[i] = i;
	for(size_t i = len-1; i > 1; i--) {
		size_t j = rand() % (i+1);
		size_t tmp = ctx[j];
		ctx[j] = ctx[i];
		ctx[i] = tmp;
	}
end:
	return ctx;
}

struct ordered {
	size_t index;
	coeff val;
};
static inline int sort_descending(const void* left, const void* right) {
	coeff l = ((const struct ordered*)left)->val,
	      r = ((const struct ordered*)right)->val;
	return l < r ? 1 : (l > r ? -1 : 0);
}

static void* init_magnitude(size_t width, size_t height, size_t channels, coeff* coeffs, const char* args) {
	intermediate qfactor = 0;
	if(args)
		qfactor = strtod(args,NULL);

	struct scan_precomputed* p = calloc(1,sizeof(*p));

	size_t len = width*height;
	size_t* ctx = NULL;
	struct ordered* sort = malloc(sizeof(*sort)*len);
	if(!(sort && (ctx = malloc(sizeof(size_t)*len))))
		goto end;

	for(size_t y = 0; y < height; y++)
		for(size_t x = 0; x < width; x++) {
			size_t i = y*width+x;
			sort[i].index = i;
			intermediate sum = 0;
			for(size_t z = 0; z < channels; z++)
				sum += mc(fabs)(coeffs[i*channels+z]);
			intermediate normalization = (x ? mi(M_SQRT2) : mi(1.)) * (y ? mi(M_SQRT2) : mi(1.));
			sort[i].val = qfactor ? rint(sum*normalization*qfactor/channels) : sum*normalization;
		}
	qsort(sort,len,sizeof(*sort),sort_descending);

	coeff last_val = -1;
	size_t j = 0;
	for(size_t i = 0; i < len; i++) {
		if(sort[i].val != last_val) {
			j++;
			last_val = sort[i].val;
		}
		if(!scan_precomputed_add_coord(p,j,sort[i].index%width,sort[i].index/width))
			goto error;
	}

end:
	free(sort);
	free(ctx);
	return p;

error:
	scan_precomputed_destroy(p);
	p = NULL;
	goto end;
}

static inline double (*round_function(const char* name))(double) {
	if(name) {
		if(!(strcmp(name,"tonearest") && strcmp(name,"round")))
			return round;
		if(!(strcmp(name,"upward") && strcmp(name,"ceil")))
			return ceil;
		if(!(strcmp(name,"downward") && strcmp(name,"floor")))
			return floor;
	}
	return rint;
}

static void* init_radial(size_t width, size_t height, size_t channels, coeff* coeffs, const char* args) {
	double (*roundfn)(double) = round_function(args);
	struct scan_precomputed* p = calloc(1,sizeof(*p));

	for(size_t y = 0; y < height; y++)
		for(size_t x = 0; x < width; x++)
			if(!scan_precomputed_add_coord(p,roundfn(hypot(x,y)),x,y)) {
				scan_precomputed_destroy(p);
				p = NULL;
				goto end;
			}

end:
	return p;
}

static void* init_iradial(size_t width, size_t height, size_t channels, coeff* coeffs, const char* args) {
	double (*roundfn)(double) = round_function(args);
	struct scan_precomputed* p = calloc(1,sizeof(*p));

	size_t limit = roundfn(hypot(width-1,height-1))+1;

	for(size_t y = 0; y < height; y++)
		for(size_t x = 0; x < width; x++)
			if(!scan_precomputed_add_coord(p,limit-(size_t)roundfn(hypot(width-x-1,height-y-1))-1,x,y)) {
				scan_precomputed_destroy(p);
				p = NULL;
				goto end;
			}

end:
	return p;
}

static void* init_evalxy(size_t width, size_t height, size_t channels, coeff* coeffs, const char* args) {
	if(!args)
		return NULL;

	struct scan_precomputed* p = NULL;
	size_t* map = NULL;
	AVExpr* expr = NULL;
	const char* names[3] = {"x","y"};
	if(av_expr_parse(&expr,args,names,NULL,NULL,NULL,NULL,0,NULL) < 0)
		goto end;

	p = calloc(1,sizeof(*p));
	for(size_t y = 0; y < height; y++)
		for(size_t x = 0; x < width; x++) {
			double result = rint(av_expr_eval(expr,(double[3]){x,y},NULL));
			if(isnan(result) || isinf(result) || result < 0)
				continue;
			size_t i = (size_t)result;
			size_t j;
			for(j = 0; j < p->limit; j++)
				if(map[j] == i) {
					if(!scan_precomputed_add_coord(p,j,x,y))
						goto error;
					break;
				}
			if(j == p->limit) {
				if(!scan_precomputed_add_coord(p,j,x,y))
					goto error;
				size_t* m = realloc(map,sizeof(*map)*p->limit);
				if(!m)
					goto error;
				map = m;
				map[p->limit-1] = i;
			}
		}

end:
	free(map);
	av_expr_free(expr);
	return p;

error:
	scan_precomputed_destroy(p);
	p = NULL;
	goto end;
}

static void* init_evali(size_t width, size_t height, size_t channels, coeff* coeffs, const char* args) {
	if(!args)
		return NULL;

	AVExpr** expr = malloc(sizeof(*expr)*2);
	char* xexpr = strdup(args);
	char* yexpr = strchr(xexpr,';');
	if(!yexpr)
		goto error;
	*yexpr++ = '\0';

	const char* names[4] = {"i","width","height"};
	if(av_expr_parse(expr,xexpr,names,NULL,NULL,NULL,NULL,0,NULL) < 0)
		goto error;
	if(av_expr_parse(expr+1,yexpr,names,NULL,NULL,NULL,NULL,0,NULL) < 0) {
		av_expr_free(expr[0]);
		goto error;
	}
	return expr;

error:
	free(expr);
	free(xexpr);
	return NULL;
}

static void* init_file(size_t width, size_t height, size_t channels, coeff* coeffs, const char* args) {
	FILE* f;
	if(!(args && (f = fopen(args,"r"))))
		return NULL;
	struct scan_precomputed* p = scan_precomputed_unserialize(f);
	fclose(f);

	for(size_t i = 0; i < p->limit; i++)
		for(size_t j = 0; j < p->intervals[i]; j++)
			if(p->scans[i][j][1] >= width || p->scans[i][j][0] >= height) {
				scan_precomputed_destroy(p);
				return NULL;
			}

	return p;
}

static void* init_precomputed(size_t width, size_t height, size_t channels, coeff* coeffs, const char* args) {
	if(!args)
		return NULL;
	struct scan_precomputed* p = NULL;
	char* name = strdup(args),
	    * optstart = strchr(name,':');
	if(!optstart)
		optstart = strchr(name,'\0');
	*optstart++ = '\0';

	struct scan_method* m = scan_method_find(name);
	if(!m || m->init == init_precomputed)
		goto end;

	struct scan_context* ctx = scan_init(m,width,height,channels,coeffs,optstart);
	p = scan_precompute(ctx);
	scan_destroy(ctx);

end:
	free(name);
	return p;
}

void destroy_precomputed(void* ctx) {
	scan_precomputed_destroy(ctx);
}

void destroy_evali(void* ctx) {
	AVExpr** expr = ctx;
	av_expr_free(expr[0]);
	av_expr_free(expr[1]);
	free(expr);
}

// specification defaults to a single element scan, so:
// - limit is the product of the dimensions
// - max_interval is the product of the dimensions divided by the limit
// - interval is the max_interval for all i

static struct scan_method methods[] = {
	// single-element scans
	{
		"horizontal",
		scan_horiz,
	},{
		"vertical",
		scan_vert,
	},{
		"zigzag",
		scan_zigzag,
	},{
		"random",
		scan_ordered,
		.init = init_random,
		.init_args = "optional seed (int)",
	},
	// multiple-element scans
	{
		"row",
		scan_row,
		.limit = limit_height,
	},{
		"column",
		scan_col,
		.limit = limit_width,
	},{
		"diagonal",
		scan_diag,
		.interval = interval_diag,
		.limit = limit_sum,
		.max_interval = limit_min,
	},{
		"mirror",
		scan_mirror,
		.interval = interval_mirror,
		.limit = limit_max,
		.max_interval = limit_mirror,
	},{
		"box",
		scan_box,
		.interval = interval_box,
		.limit = limit_max,
		.max_interval = limit_sum,
	},{
		"ibox",
		scan_ibox,
		.interval = interval_ibox,
		.limit = limit_min,
		.max_interval = limit_sum,
	},{
		"radial",
		scan_precomputed,
		.interval = interval_precomputed,
		.limit = limit_precomputed,
		.max_interval = max_interval_precomputed,
		.init = init_radial,
		.destroy = destroy_precomputed,
		.init_args = "optional rounding mode (tonearest, upward, downward, system)",
	},{
		"iradial",
		scan_precomputed,
		.interval = interval_precomputed,
		.limit = limit_precomputed,
		.max_interval = max_interval_precomputed,
		.init = init_iradial,
		.destroy = destroy_precomputed,
		.init_args = "optional rounding mode (tonearest, upward, downward, system)",
	},{
		"magnitude",
		scan_precomputed,
		.interval = interval_precomputed,
		.limit = limit_precomputed,
		.max_interval = max_interval_precomputed,
		.init = init_magnitude,
		.destroy = destroy_precomputed,
		.init_args = "optional quantization factor (float)",
	},{
		"evalxy",
		scan_precomputed,
		.interval = interval_precomputed,
		.limit = limit_precomputed,
		.max_interval = max_interval_precomputed,
		.init = init_evalxy,
		.destroy = destroy_precomputed,
		.init_args = "expression satisfying index = f(x,y)",
	},{
		"evali",
		scan_evali,
		.init = init_evali,
		.destroy = destroy_evali,
		.init_args = "expressions satisfying x = f(i,width,height); y = f(i,width,height)",
	},
	// meta scans
	{
		"file",
		scan_precomputed,
		.interval = interval_precomputed,
		.limit = limit_precomputed,
		.max_interval = max_interval_precomputed,
		.init = init_file,
		.destroy = destroy_precomputed,
		.init_args = "filename",
	},{
		"precomputed",
		scan_precomputed,
		.interval = interval_precomputed,
		.limit = limit_precomputed,
		.max_interval = max_interval_precomputed,
		.init = init_precomputed,
		.destroy = destroy_precomputed,
		.init_args = "method:method options",
	},
	{0}
};

struct scan_method* scan_methods() {
	return methods;
}

struct scan_method* scan_method_find(const char* name) {
	for(struct scan_method* m = scan_methods(); m->name; m++)
		if(!strcmp(m->name,name))
			return m;
	return NULL;
}

// finds shortest name with given prefix
struct scan_method* scan_method_find_prefix(const char* prefix) {
	size_t namelen = strlen(prefix);
	size_t min = SIZE_MAX, new_min;
	struct scan_method* ret = NULL;
	for(struct scan_method* m = scan_methods(); m->name; m++)
		if(!strncmp(m->name,prefix,namelen) && (new_min = strlen(m->name)) < min) {
			ret = m;
			min = new_min;
		}
	return ret;
}

struct scan_method* scan_method_next(struct scan_method* m) {
	m++;
	if(!m->name)
		return NULL;
	return m;
}

const char* scan_method_name(struct scan_method* m) {
	return m->name;
}

const char* scan_method_options(struct scan_method* m) {
	return m->init_args;
}
