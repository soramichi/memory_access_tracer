#include <map>
#include <cstdlib>

#include "memory_access.h"

typedef unsigned long u64;

using namespace std;

extern "C" void* create_hash(){
  map<u64, int>* ret = new map<u64, int>();
  return (void*)ret;
}

extern "C" void add_to_hash(void* _hash, u64 key, int value){
  map<u64, int>* hash = (map<u64, int>*)_hash;
  hash->operator[](key) = value;
}

extern "C" int get_from_hash(void* _hash, u64 key){
  map<u64, int>* hash = (map<u64, int>*)_hash;
  return hash->operator[](key);
}

extern "C" int get_size_of_hash(void* _hash){
  map<u64, int>* hash = (map<u64, int>*)_hash;
  return hash->size();
}

extern "C" struct memory_access* get_memory_access(void* _hash){
  map<u64, int>* hash = (map<u64, int>*)_hash;
  map<u64, int>::iterator it;
  int i = 0;
  
  struct memory_access* ret = (struct memory_access*)malloc(sizeof(struct memory_access) * hash->size());
  
  for(it=hash->begin(); it != hash->end(); it++){
    ret[i++] = { it->first, it->second };
  }

  return ret;
}
