#if !defined(_HASH_H) // double include guard
#define _HASH_H

#include "memory_access.h"

void* create_hash(void);
void add_to_hash(void* _hash, u64 key, u64 value);
u64 get_from_hash(void* _hash, u64 key);
int get_size_of_hash(void* _hash);
struct memory_access* get_memory_access(void* _hash);


#endif
