#if !defined(_HASH_H) // double include guard
#define _HASH_H

typedef unsigned long u64;

struct key_value { // the name pair is used in the STL so we avoid it
  u64 key;
  u64 value;
};

#ifdef __cplusplus // gcc extension
extern "C" {
#endif
  void* create_hash(void);
  int has_key(void* _hash, u64 key);
  void add_to_hash(void* _hash, u64 key, u64 value);
  u64 get_from_hash(void* _hash, u64 key);
  int get_size_of_hash(void* _hash);
  u64* get_keys(void* _hash);
  u64* get_values(void* _hash);
  struct key_value* get_keys_and_values(void* _hash);
#ifdef __cplusplus
} // extern "C" {
#endif

#endif
