#include <map>
#include <cstdlib>
#include <typeinfo>

#include "hash.h"

using namespace std;

typedef map<u64, u64> hash_t;

extern "C" c_hash create_hash(){
  hash_t* ret = new map<u64, u64>();
  return (c_hash)ret;
}

extern "C" int has_key(c_hash _hash, u64 key){
  hash_t* hash = (map<u64, u64>*)_hash;
  if(hash == NULL)
    fprintf(stderr, "hash is NULL (%s:%d)\n", __FILE__, __LINE__);
  return !!(hash->count(key));
}

extern "C" void add_to_hash(c_hash _hash, u64 key, u64 value){
  hash_t* hash = (map<u64, u64>*)_hash;
  if(hash == NULL)
    fprintf(stderr, "hash is NULL (%s:%d)\n", __FILE__, __LINE__);
  hash->operator[](key) = value;
}

extern "C" u64 get_from_hash(c_hash _hash, u64 key){
  hash_t* hash = (map<u64, u64>*)_hash;
  if(hash == NULL)
    fprintf(stderr, "hash is NULL (%s:%d)\n", __FILE__, __LINE__);
  return hash->operator[](key);
}

extern "C" int get_size_of_hash(c_hash _hash){
  hash_t* hash = (map<u64, u64>*)_hash;
  if(hash == NULL)
    fprintf(stderr, "hash is NULL (%s:%d)\n", __FILE__, __LINE__);
  return hash->size();
}

// Note: This returns a copy of the keys, which may change after get_keys is called.
extern "C" u64* get_keys(c_hash _hash){
  hash_t* hash = (map<u64, u64>*)_hash;
  int i = 0;
  u64* ret;

  if(hash == NULL)
    fprintf(stderr, "hash is NULL (%s:%d)\n", __FILE__, __LINE__);

  ret = (u64*)malloc(sizeof(u64) * hash->size());
  for(auto it=hash->begin(); it != hash->end(); it++){
    ret[i++] = { it->first };
  }

  return ret;
}

// Note: This returns a copy of the values, which may change after get_keys is called.
extern "C" u64* get_values(c_hash _hash){
  hash_t* hash = (map<u64, u64>*)_hash;
  int i = 0;
  u64* ret;

  if(hash == NULL)
    fprintf(stderr, "hash is NULL (%s:%d)\n", __FILE__, __LINE__);

  ret = (u64*)malloc(sizeof(u64) * hash->size());
  for(auto it=hash->begin(); it != hash->end(); it++){
    ret[i++] = { it->second };
  }

  return ret;  
}

extern "C" struct key_value* get_keys_and_values(c_hash _hash){
  hash_t* hash = (map<u64, u64>*)_hash;
  int i = 0;

  struct key_value* ret = (struct key_value*)malloc(sizeof(struct key_value) * hash->size());

  for(auto it=hash->begin(); it != hash->end(); it++){
    ret[i++] = { it->first, it->second };
  }

  return ret;
}
