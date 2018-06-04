/*
 * keyed_enum - Define enums with static string lookup
 * Copyright 2014-2018 0x09.net.
 */

#ifndef KEYED_ENUM_H
#define KEYED_ENUM_H

#include <string.h>

#define enum_gen_enum_elem(type,value) type##_##value,
#define enum_gen_key_elem(type,value) "|" #value
#define enum_gen_table_elem(type,value) #value,
#define enum_gen_count_elem(type,value) 1+

#define enum_gen_enum(type)\
	enum type {\
		enum_gen_enum_elem(type,none)\
		type(enum_gen_enum_elem,type)\
	};

#define enum_keys(type) (enum##_##type##_##keys+1)
#define enum_gen_keys(type)\
	const static char enum##_##type##_##keys[] = type(enum_gen_key_elem,type);

#define enum_table(type) enum##_##type##_##table
#define enum_gen_table(type)\
	const static char* enum_table(type)[type(enum_gen_count_elem,type)+2] = {\
		enum_gen_table_elem(type,)\
		type(enum_gen_table_elem,type)\
	};

#define enum_gen_accessor_defs(type)\
	enum type type##_##val(const char*);\
	const char** type##_##keys(void);

#define enum_gen_accessors(type)\
	enum type type##_##val(const char* key) { return enum_val(type,key); };\
	const char** type##_##keys() { return enum_table(type)+1; };

#define enum_public_gen(type)\
	enum_gen_enum(type)\
	enum_gen_accessor_defs(type)

#define enum_private_gen(type)\
	enum_gen_keys(type)\
	enum_gen_table(type)\
	enum_gen_accessors(type)

#define enum_gen(type)\
	enum_gen_enum(type)\
	enum_gen_keys(type)\
	enum_gen_table(type)

static inline int enum_table_val(const char* table[], const char* key) {
	for(const char** i = table+1; *i; i++)
		if(!strcmp(key,*i))
			return i-table;
	return 0;
}

#define enum_val(type,key) enum_table_val(enum_table(type),key)
#define enum_key(type,value) enum_table(type)[value]

#endif
