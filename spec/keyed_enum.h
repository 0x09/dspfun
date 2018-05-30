/*
 * spec - Generate invertible frequency spectrums for viewing and editing.
 * Copyright 2014-2016 0x09.net.
 */

#ifndef KEYED_ENUM_H
#define KEYED_ENUM_H

#define enum_gen_enum_elem(type,value) type##_##value,
#define enum_gen_key_elem(type,value) "|" #value
#define enum_gen_table_elem(type,value) #value,
#define enum_gen_count_elem(type,value) 1+

#define enum_gen_enum(type)\
	enum type {\
		enum_gen_enum_elem(type,none)\
		type(enum_gen_enum_elem,type)\
	};

#define enum_keys(type) (type##_##keys+1)
#define enum_gen_keys(type)\
	const static char type##_##keys[] = type(enum_gen_key_elem,type);

#define enum_table(type) type##_##table
#define enum_gen_table(type)\
	const static char* enum_table(type)[type(enum_gen_count_elem,type)+2] = {\
		enum_gen_table_elem(type,)\
		type(enum_gen_table_elem,type)\
	};

#define enum_gen(type)\
	enum_gen_enum(type)\
	enum_gen_keys(type)\
	enum_gen_table(type)

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
