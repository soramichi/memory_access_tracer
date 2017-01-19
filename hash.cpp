#include <map>

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
