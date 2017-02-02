#if !defined(_EVENT_LOG_H) // double include guard
#define _EVENT_LOG_H

struct event_log{
  int n_samples;
  void* address_to_count; // hash{memory_address -> number of samples}
};

#endif
