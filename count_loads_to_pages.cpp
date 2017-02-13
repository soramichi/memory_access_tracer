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

  cout << "# start aggregating the events" << endl;
  
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
  }

  cout << "# finished aggregating the events" << endl;
  cout << "# page,count" << endl;

  for(auto it = count.begin(); it != count.end(); it++){
    cout << it->first << "," << it->second << endl;
  }

  return 0;
}
