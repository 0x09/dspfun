/*
 * spec - Generate invertible frequency spectrums for viewing and editing.
 * Copyright 2014-2016 0x09.net.
 */

#ifndef KEYED_ENUM_H
#define KEYED_ENUM_H

#define CAT_(x,y) x##y
#define CAT(x,y) CAT_(x,y)

#define Xe(type,value) type##_##value,
#define Xt(type,value) #value,
#define Xc(type,value) 1+
#define X(type,generator,value) CAT(X,generator)(type,value)

#define enum_table(type) type##_##table
#define keyed_enum_gen(type)\
	enum type {\
		Xe(type,none)\
		XENUM(type,e)\
	};\
	const static char* enum_table(type)[XENUM(type,c)+2] = {\
		Xt(type,)\
		XENUM(type,t)\
	};

#include <string.h>
static int enum_table_val(const char* table[], const char* key) {
	for(const char** i = table+1; *i; i++)
		if(!strcmp(key,*i))
			return i-table;
	return 0;
}
#define enum_val(type,key) enum_table_val(enum_table(type),key)
#define enum_key(type,value) enum_table(type)[value]

#include <stdlib.h>
static const char* enum_table_keys(const char* table[]) {
	size_t len = 0;
	for(const char** i = table+1; *i; i++)
		len += strlen(*i)+1;
	char* buf = malloc(len);
	for(const char** i = table+1; *i; i++, buf++)
		*(buf=strcpy(buf,*i)+strlen(*i))='|';
	buf[-1]='\0';
	return buf-len;
}
#define enum_keys(type) enum_table_keys(enum_table(type))
#endif
