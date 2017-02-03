#if !defined(_HASH_H) // double include guard
#define _HASH_H

typedef unsigned long u64;

typedef struct {}* c_hash; // a dummy type to let C side compiler do type checking

struct key_value { // the name pair is used in the STL so we avoid it
  u64 key;
  u64 value;
};

#ifdef __cplusplus // gcc extension
extern "C" {
#endif
  c_hash create_hash(void);
  int has_key(c_hash _hash, u64 key);
  void add_to_hash(c_hash _hash, u64 key, u64 value);
  u64 get_from_hash(c_hash _hash, u64 key);
  int get_size_of_hash(c_hash _hash);
  u64* get_keys(c_hash _hash);
  u64* get_values(c_hash _hash);
  struct key_value* get_keys_and_values(c_hash _hash);
#ifdef __cplusplus
} // extern "C" {
#endif

#endif
