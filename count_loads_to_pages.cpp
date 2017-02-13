#include <iostream>
#include <map>
#include <cstdlib>
#include <cstring>

using namespace std;

typedef unsigned long long addr_t;

const addr_t page_size = 4096;

// Usage 1: perf script -F addr,ip -i perf.data | a.out
// Usage 2: perf script -F addr,data_src,ip -i perf.data | a.out
// Note that changing the orders inside -F does not affect.
// (-F addr,ip and -F ip,addr yield exactly the same output)
int main(void){
  string str;
  map<addr_t, int> count_mem, count_cache;

  cout << "# start aggregating the events" << endl;
  
  while(getline(cin, str)){
    addr_t addr, ip;
    unsigned int data_src_encoding;
    char* ptr = strdup(str.c_str());
    char* tail = ptr + strlen(ptr) - 1;
    int index = 0;

    // skip heading spaces
    while(*ptr == ' ')
      ptr++;
    addr = strtoll(ptr, &ptr, 16);

    // skip heading spaces
    // after this while, ptr points the second block of characeters,
    // it may be an ip or a data_src encoding depending on the usage
    while(*ptr == ' ')
      ptr++;

    // after this while, tail points the last block of chacaters
    while(*tail != ' ')
      tail--;

    // "second block" == "last block", which means the input is useage 1
    if(ptr == tail){
      ip = strtoll(ptr, NULL, 16);
    }
    // "second block" != "last block", which means the input is useage 2
    else{
      ip = strtoll(tail, NULL, 16);
      data_src_encoding = strtol(ptr, NULL, 16);
    }

    // inside the kernel, just skip this sample
    if(addr == 0ull)
      continue;

    // demand access
    addr_t page = addr / page_size;
    count_mem[page]++;
  }

  cout << "# finished aggregating the events" << endl;
  cout << "# page,count" << endl;

  for(auto it = count_mem.begin(); it != count_mem.end(); it++){
    cout << it->first << "," << it->second << endl;
  }

  return 0;
}
