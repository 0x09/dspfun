/*
 * spec - Generate invertible frequency spectrums for viewing and editing.
 * Copyright 2014-2016 0x09.net.
 */

#ifndef KEYED_ENUM_H
#define KEYED_ENUM_H

#define Xe(type,value) type##_##value,
#define Xk(type,value) "|" #value
#define Xt(type,value) #value,
#define Xc(type,value) 1+

#define enum_gen(type)\
	enum type {\
		Xe(type,none)\
		type(Xe,type)\
	};

#define enum_keys(type) type##_##keys
#define enum_keys_gen(type)\
	const static char* enum_keys(type) = type(Xk,type)+1;

#define enum_table(type) type##_##table
#define enum_table_gen(type)\
	const static char* enum_table(type)[type(Xc,type)+2] = {\
		Xt(type,)\
		type(Xt,type)\
	};

#define keyed_enum_gen(type)\
	enum_gen(type)\
	enum_keys_gen(type)\
	enum_table_gen(type)

#include <string.h>
static int enum_table_val(const char* table[], const char* key) {
	for(const char** i = table+1; *i; i++)
		if(!strcmp(key,*i))
			return i-table;
	return 0;
}

#define enum_val(type,key) enum_table_val(enum_table(type),key)
#define enum_key(type,value) enum_table(type)[value]

#endif
