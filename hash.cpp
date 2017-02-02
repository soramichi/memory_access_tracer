#include <map>
#include <cstdlib>
#include <typeinfo>

#include "memory_access.h"

using namespace std;

typedef unsigned long u64;
typedef map<u64, u64> hash_t

extern "C" void* create_hash(){
  hash_t* ret = new map<u64, u64>();
  return (void*)ret;
}

extern "C" void add_to_hash(void* _hash, u64 key, u64 value){
  hash_t* hash = (map<u64, u64>*)_hash;
  hash->operator[](key) = value;
}

extern "C" u64 get_from_hash(void* _hash, u64 key){
  hash_t* hash = (map<u64, u64>*)_hash;
  return hash->operator[](key);
}

extern "C" int get_size_of_hash(void* _hash){
  hash_t* hash = (map<u64, u64>*)_hash;
  return hash->size();
}

extern "C" struct memory_access* get_memory_access(void* _hash){
  hash_t* hash = (map<u64, u64>*)_hash;
  int i = 0;
  
  struct memory_access* ret = (struct memory_access*)malloc(sizeof(struct memory_access) * hash->size());

  for(auto it=hash->begin(); it != hash->end(); it++){
    ret[i++] = { it->first, it->second };
  }

  return ret;
}
