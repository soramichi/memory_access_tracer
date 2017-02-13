#include <iostream>
#include <map>
#include <cstdlib>
#include <cstring>

using namespace std;

typedef unsigned long long addr_t;

const addr_t page_size = 4096;

bool local_dram_hit(unsigned int data_src){
  // Note: these numbers are perf's output, and nothing related to data_src_encoding in the intel manuals.
  if(data_src == 50501042 || data_src == 68501042)
    return true;
  else
    return false;
}

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
    unsigned int data_src;
    char* ptr = strdup(str.c_str());
    char* tail = ptr + strlen(ptr) - 1;
    int index = 0;

    // skip heading spaces
    while(*ptr == ' ')
      ptr++;
    addr = strtoll(ptr, &ptr, 16);

    // inside the kernel, just skip this sample
    if(addr == 0ull)
      continue;
    
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
      addr_t page = addr / page_size;
      ip = strtoll(ptr, NULL, 16);
      count_mem[page]++;
    }
    // "second block" != "last block", which means the input is useage 2
    else{
      addr_t page = addr / page_size;
      ip = strtoll(tail, NULL, 16); // last block
      data_src = strtol(ptr, NULL, 10); // second block

      if(local_dram_hit(data_src)){
	count_mem[page]++;
      }
      else{
	count_cache[page]++;
      }
    }
  }

  if(count_cache.size() == 0){ // no cache hit, must be usage 1{
    cout << "# finished aggregating the events" << endl;
    cout << "# page,count" << endl;

    for(auto it = count_mem.begin(); it != count_mem.end(); it++){
      cout << it->first << "," << it->second << endl;
    }
  }
  else{ // usage 2
    auto it_mem = count_mem.begin();
    auto it_cache = count_cache.begin();

    cout << "# finished aggregating the events" << endl;
    cout << "# page,mem_hit,cache_hit" << endl;

    while(it_mem != count_mem.end() && it_cache != count_cache.end()){
      if(it_mem->first == it_cache->first){
	cout << it_mem->first << "," << it_mem->second << "," << it_cache->second << endl;
	it_mem++;
	it_cache++;
      }
      else if(it_mem->first < it_cache->first){
	cout << it_mem->first << "," << it_mem->second << "," << "0" << endl;
	it_mem++;
      }
      else /* if(it_mem->first > it_cache->first) */{
	cout << it_cache->first << "," << "0" << "," << it_cache->second << endl;
	it_cache++;
      }
    }
  }

  return 0;
}
