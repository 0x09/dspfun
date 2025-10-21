/*
 * assoc - Define key/value tables for arbitrary static data
 */

#ifndef ASSOC_H
#define ASSOC_H

#include <string.h>

struct assoc {
	const char* const key;
	const void* const value;
};

#define assoc_gen_key_elem(key,value)   key,
#define assoc_gen_table_elem(key,value) {key,(value)},

#define assoc_keys(name) assoc_##name##_keys
#define assoc_keys_gen(name)\
	const static char* assoc_keys(name)[] = {\
		name(assoc_gen_key_elem)\
		NULL\
	};

#define assoc_table(name) assoc_##name##_table
#define assoc_table_gen(name)\
	const static struct assoc assoc_table(name)[] = {\
		name(assoc_gen_table_elem)\
		{0}\
	};

#define assoc_gen(name)\
	assoc_keys_gen(name)\
	assoc_table_gen(name)

const static inline void* assoc_table_val(const struct assoc table[],const char* key) {
	for(; table->key; table++)
		if(!strcmp(key,table->key))
			break;
	return table->value;
}

#define assoc_val(name,key) assoc_table_val(assoc_table(name),(key))

#endif
