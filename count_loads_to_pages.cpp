#include <iostream>
#include <map>

using namespace std;

typedef unsigned long long addr_t;

const addr_t page_size = 4096;

int main(void){
  string str;
  addr_t addr;
  map<addr_t, int> count;

  cout << "start aggregating the events" << endl;
  
  while(getline(cin, str)){
    addr_t addr = strtoll(str.c_str(), NULL, 16);

    if(addr == 0ull)
      continue;

    addr_t page = addr / page_size;
    count[page]++;
  }

  cout << "finished aggregating the events" << endl;
  cout << "page,count" << endl;
  
  for(auto it = count.begin(); it != count.end(); it++){
    cout << it->first << "," << it->second << endl;
  }

  return 0;
}
