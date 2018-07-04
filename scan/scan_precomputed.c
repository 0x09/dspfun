/*
 * scan - progressively reconstruct images using various frequency space scans.
 * Copyright 2018 0x09.net.
 */

#include "scan_precomputed.h"

#include <string.h>
#include <math.h>

void scan_precomputed_dimensions(struct scan_precomputed* p, size_t* restrict width, size_t* restrict height) {
	*width = *height = 0;
	for(size_t i = 0; i < p->limit; i++)
		for(size_t j = 0; j < p->intervals[i]; j++) {
			size_t (*coord)[2] = p->scans[i]+j;
			if((*coord)[0] > *height)
				*height = (*coord)[0];
			if((*coord)[1] > *width)
				*width = (*coord)[1];
		}
	(*width)++;
	(*height)++;
}

bool scan_precomputed_add_coord(struct scan_precomputed* p, size_t index, size_t x, size_t y) {
	if(index >= p->limit) {
		size_t limit = index+1;
		size_t* intervals = realloc(p->intervals,sizeof(*p->intervals)*limit);
		size_t (**scans)[2] = realloc(p->scans,sizeof(*p->scans)*limit);
		if(!(intervals && scans))
			return false;
		p->intervals = intervals;
		p->scans = scans;
		memset(p->intervals+p->limit,0,sizeof(*p->intervals)*(limit-p->limit));
		memset(p->scans+p->limit,0,sizeof(*p->scans)*(limit-p->limit));
		p->limit = limit;
	}
	size_t* interval = p->intervals+index;
	size_t (*scan)[2] = realloc(p->scans[index],sizeof(*p->scans[index])*(*interval+1));
	if(!scan)
		return false;
	p->scans[index] = scan;
	p->scans[index][*interval][0] = y;
	p->scans[index][*interval][1] = x;
	(*interval)++;
	return true;
}

static struct scan_precomputed* unserialize_coordinate(FILE* f, char** line) {
	struct scan_precomputed* p = calloc(1,sizeof(*p));
	size_t linecap = *line ? strlen(*line) : 0;
	ssize_t linelen;
	size_t i = 0;
	do {
		if(!*line || **line == '\n')
			continue;
		char *token,* string = *line;
		while((token = strsep(&string, " ")) && *token != '\n') {
			if(!*token)
				continue;
			size_t x, y;
			if(sscanf(token,"%zu,%zu",&x,&y) != 2 ||
			   !scan_precomputed_add_coord(p,i,x,y)) {
				scan_precomputed_destroy(p);
				return NULL;
			}
		}
		i++;
	} while((linelen = getline(line,&linecap,f)) > 0);
	return p;
}

static struct scan_precomputed* unserialize_index(FILE* f, char** line) {
	struct scan_precomputed* p = calloc(1,sizeof(*p));
	size_t linecap = *line ? strlen(*line) : 0;
	ssize_t linelen;
	size_t y = 0;
	do {
		if(!*line || **line == '\n')
			continue;
		size_t x = 0;
		char *token,* string = *line;
		while((token = strsep(&string, " ")) && *token != '\n') {
			if(!*token)
				continue;
			size_t index;
			if(sscanf(token,"%zu",&index) != 1 || !scan_precomputed_add_coord(p,index,x,y)) {
				scan_precomputed_destroy(p);
				return NULL;
			}
			x++;
		}
		y++;
	} while((linelen = getline(line,&linecap,f)) > 0);
	return p;
}

struct scan_precomputed* scan_precomputed_unserialize(FILE* f) {
	struct scan_precomputed* p = NULL;
	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;
	while((linelen = getline(&line,&linecap,f)) > 0 && strspn(line," \n") == linelen)
		;
	if(linelen > 0)
		p = strchr(line,',') ? unserialize_coordinate(f,&line) : unserialize_index(f,&line);
	free(line);
	return p;
}

void scan_precomputed_serialize_coordinate(struct scan_precomputed* p, FILE* f) {
	for(size_t i = 0; i < p->limit; i++) {
		for(size_t j = 0; j < p->intervals[i]; j++)
			fprintf(f,"%zu,%zu ", p->scans[i][j][1], p->scans[i][j][0]);
		fprintf(f,"\n");
	}
}

void scan_precomputed_serialize_index_noalloc(struct scan_precomputed* p, FILE* f) {
	int pad = log10f(p->limit)+1;
	size_t width, height;
	scan_precomputed_dimensions(p,&width,&height);
	for(size_t y = 0; y < height; y++) {
		for(size_t x = 0; x < width; x++)
			for(size_t i = 0; i < p->limit; i++)
				for(size_t j = 0; j < p->intervals[i]; j++) {
					if(p->scans[i][j][0] == y && p->scans[i][j][1] == x)
						fprintf(f,"%*zu ",pad,i);
				}
		fprintf(f,"\n");
	}
}

void scan_precomputed_serialize_index(struct scan_precomputed* p, FILE* f) {
	int pad = log10f(p->limit)+1;
	size_t width, height;
	scan_precomputed_dimensions(p,&width,&height);
	size_t* index = malloc(sizeof(*index)*width*height);
	for(size_t i = 0; i < p->limit; i++)
		for(size_t j = 0; j < p->intervals[i]; j++)
			index[p->scans[i][j][0]*width+p->scans[i][j][1]] = i;
	for(size_t y = 0; y < height; y++) {
		for(size_t x = 0; x < width; x++)
			fprintf(f,"%*zu ",pad,index[y*width+x]);
		fprintf(f,"\n");
	}
	free(index);
}

void scan_precomputed_destroy(struct scan_precomputed* p) {
	for(size_t i = 0; i < p->limit; i++)
		free(p->scans[i]);
	free(p->intervals);
	free(p);
}
