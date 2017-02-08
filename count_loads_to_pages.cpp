#include <iostream>
#include <map>
#include <cstdlib>
#include <cstring>

using namespace std;

typedef unsigned long long addr_t;

const addr_t page_size = 4096;

struct ip_based_stride{
  addr_t last;
  int stride;
};

// Usage: perf script -F addr,ip -i perf.data
// Note that addr and ip are displayed in this order no matter what the order after -F is.
// (-F addr,ip and -F ip,addr yield exactly the same output)
int main(void){
  string str;
  map<addr_t, int> count;
  map<addr_t, ip_based_stride> prefetcher;

  cout << "start aggregating the events" << endl;
  
  while(getline(cin, str)){
    addr_t addr, ip;
    char* ptr = strdup(str.c_str());
    int index = 0;

    // skip heading spaces
    while(ptr[index] == ' ')
      index++;
    addr = strtoll(ptr+index, &ptr, 16);

    // skip heading spaces
    while(ptr[index] == ' ')
      index++;
    ip = strtoll(ptr+index, NULL, 16);

    // inside the kernel, just skip this sample
    if(addr == 0ull)
      continue;

    // demand access
    addr_t page = addr / page_size;
    count[page]++;

    // prefetch access
    auto it = prefetcher.find(ip);

    if(it == prefetcher.end()){
      ip_based_stride stride = { .last = addr, .stride = 0 };
      prefetcher[ip] = stride;
      cerr << "initialize a stride" << endl;
    }
    else {
      ip_based_stride* stride = &it->second;
      int current_stride = addr - stride->last;
      
      // current stride matches the last stride, pretetch
      // (if current_stride is 0, do nothinf as accessing the same address is nonsense)
      if(stride->stride == current_stride && current_stride != 0){
	/*
	// debug print
	cerr << "find a stride" << endl;
	cerr << "ip:" << ip << ", addr: " << addr << ", last: " << stride->last << endl;
	cerr << "current_stride: " << current_stride << ", last_stride: " << stride->stride << endl;
	*/
	
	// addr + current_stride is prefetched,
	// meaning that this page is accessed again
	count[page]++;
      }

      stride->stride = current_stride;
      stride->last = addr;
    }
  }

  cout << "finished aggregating the events" << endl;
  cout << "page,count" << endl;
  
  for(auto it = count.begin(); it != count.end(); it++){
    cout << it->first << "," << it->second << endl;
  }

  return 0;
}
