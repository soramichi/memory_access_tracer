#include <vector>

typedef unsigned long u64;

using namespace std;

extern "C" void* create_vector(){
  return new vector<u64>();
}

extern "C" void add_to_vector(void* _vec, u64 val){
  vector<u64>* vec = (vector<u64>*)_vec;
  vec->push_back(val);
}

extern "C" u64* get_data_from_vector(void* _vec){
  vector<u64>* vec = (vector<u64>*)_vec;
  return vec->data();
}
