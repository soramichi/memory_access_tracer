#if !defined(_HASH_H) // double include guard
#define _HASH_H

void* create_hash(void);
void add_to_hash(void* _hash, u64 key, int value);
int get_from_hash(void* _hash, u64 key);

#endif
