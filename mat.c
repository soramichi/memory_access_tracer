#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "util/session.h"
#include "util/evlist.h"
#include "util/values.h"
#include "util/parse-events.h"
#include "cache.h"

#if defined(__PERF_VERSION_4__)
#include "util/config.h"
#endif

#include "hash.h"
#include "memory_access.h"

static int __mat_parse_events(struct perf_evlist *evlist, const char *str){
#if defined(__PERF_VERSION_4__)
  struct parse_events_error err = { .idx = 0, };
  return parse_events(evlist, str, &err);
#else
  return parse_events(evlist, str);
#endif
}

static void __mat_perf_evlist__config(struct perf_evlist *evlist, struct record_opts *opts){
#if defined(__PERF_VERSION_4__)
  return perf_evlist__config(evlist, opts, NULL);
#else
  return perf_evlist__config(evlist, opts);
#endif
}

static int __mat_perf_session__process_events(struct perf_session *session,
#if defined(__PERF_VERSION_4__)
					      struct perf_tool *tool __attribute__((unused))){
  return perf_session__process_events(session);
#else
  					      struct perf_tool *tool){
  return perf_session__process_events(session, tool);
#endif
}

/** shared variable ************************************************************************/
volatile int written_so_far;
/*******************************************************************************************/

static char* make_uniq_path(void){
  char* ret = (char*)malloc(sizeof(char) * 64);
  int fd;
  
  sprintf(ret, "/dev/shm/perf_XXXXXX");
  fd = mkstemp(ret);

  close(fd); // close the file immediately, so that the following processes can open it

  return ret;
}

/** stuff related to do_record *************************************************************/
// default target, when -pid is not specified (, which this program assumes)
struct target unused_target = {NULL, NULL, NULL, NULL, -1, 0, 1, 1, 0};

// copy-pasted from builin-record.
static volatile int workload_exec_errno;
static volatile int done = 0;
static volatile int signr = -1;
static volatile int child_finished = 0;
static void workload_exec_failed_signal(int signo __maybe_unused,
					siginfo_t *info,
					void *ucontext __maybe_unused)
{
	workload_exec_errno = info->si_value.sival_int;
	done = 1;
	child_finished = 1;
}

/** struct record ********************************************************/
#if defined(__PERF_VERSION_4__)
struct record {
  struct perf_tool        tool;
  struct record_opts      opts;
  u64                     bytes_written;
  struct perf_data_file   file;
  struct auxtrace_record  *itr;
  struct perf_evlist      *evlist;
  struct perf_session     *session;
  const char              *progname;
  int                     realtime_prio;
  bool                    no_buildid;
  bool                    no_buildid_set;
  bool                    no_buildid_cache;
  bool                    no_buildid_cache_set;
  bool                    buildid_all;
  bool                    timestamp_filename;
  bool                    switch_output;
  unsigned long long      samples;
};
#else
struct record {
	struct perf_tool	tool;
	struct record_opts	opts;
	u64			bytes_written;
	struct perf_data_file	file;
	struct perf_evlist	*evlist;
	struct perf_session	*session;
	const char		*progname;
	int			realtime_prio;
	bool			no_buildid;
	bool			no_buildid_cache;
	long			samples;
};
#endif

// should be global, which is what builtin-record says :(
static struct record record = {
#if defined(__PERF_VER_4__)
  .opts = {
    .sample_time         = true,
    .mmap_pages          = UINT_MAX,
    .user_freq           = UINT_MAX,
    .user_interval       = ULLONG_MAX,
    .freq                = 4000,
    .target              = {
      .uses_mmap   = true,
      .default_per_cpu = true,
    },
    .proc_map_timeout     = 500,
  },
  .tool = {
    .sample         = process_sample_event,
    .fork           = perf_event__process_fork,
    .exit           = perf_event__process_exit,
    .comm           = perf_event__process_comm,
    .mmap           = perf_event__process_mmap,
    .mmap2          = perf_event__process_mmap2,
    .ordered_events = true,
  },
#else
  .opts = {
    .mmap_pages	     = UINT_MAX,
    .user_freq	     = UINT_MAX,
    .user_interval	     = ULLONG_MAX,
    .freq		     = 4000,
    .target		     = {
      .uses_mmap   = true,
      .default_per_cpu = true,
    },
  },
#endif
};
/*************************************************************************/

#if defined(__PERF_VERSION_4__)
static void record__init_features(struct record *rec)
{
  struct perf_session *session = rec->session;
  int feat;

  for (feat = HEADER_FIRST_FEATURE; feat < HEADER_LAST_FEATURE; feat++)
    perf_header__set_feat(&session->header, feat);

  if (rec->no_buildid)
    perf_header__clear_feat(&session->header, HEADER_BUILD_ID);

  if (!have_tracepoints(&rec->evlist->entries))
    perf_header__clear_feat(&session->header, HEADER_TRACING_DATA);

  if (!rec->opts.branch_stack)
    perf_header__clear_feat(&session->header, HEADER_BRANCH_STACK);

  if (!rec->opts.full_auxtrace)
    perf_header__clear_feat(&session->header, HEADER_AUXTRACE);

  perf_header__clear_feat(&session->header, HEADER_STAT);
}
#endif

static void sig_handler(int sig)
{
	if (sig == SIGCHLD)
		child_finished = 1;
	else
		signr = sig;

	done = 1;
}

static void record__sig_exit(void)
{
	if (signr == -1)
		return;

	signal(signr, SIG_DFL);
	raise(signr);
}

// copy-pasted from builtin-record.c
static int record__write(struct record *rec, void *bf, size_t size){
  if (perf_data_file__write(rec->session->file, bf, size) < 0) {
    pr_err("failed to write perf data, error: %m\n");
    return -1;
  }

  rec->bytes_written += size;
  return 0;
}

// copy-pasted from builtin-record.c
static int record__mmap_read(struct record* rec, struct perf_mmap *md)
{
	unsigned int head = perf_mmap__read_head(md);
	unsigned int old = md->prev;
	unsigned char *data = md->base + page_size;
	unsigned long size;
	void *buf;
	int rc = 0;
	
	if (old == head){
	  return 0;
	}

	rec->samples++;

	size = head - old;

	if ((old & md->mask) + size != (head & md->mask)) {
		buf = &data[old & md->mask];
		size = md->mask + 1 - (old & md->mask);
		old += size;

		if (record__write(rec, buf, size) < 0) {
			rc = -1;
			goto out;
		}
	}

	buf = &data[old & md->mask];
	size = head - old;
	old += size;

	if (record__write(rec, buf, size) < 0) {
		rc = -1;
		goto out;
	}

	md->prev = old;
	perf_mmap__write_tail(md, old);

out:
	return rc;
}

// copy-pasted from builtin-record.c
static struct perf_event_header finished_round_event = {
	.size = sizeof(struct perf_event_header),
	.type = PERF_RECORD_FINISHED_ROUND,
};

// (almost) copy-pasted from builtin-record.c
static int record__mmap_read_all(struct record* rec){
  int ret = 0;
  int i;
  
  for(i=0; i<rec->evlist->nr_mmaps; i++){
    if (rec->evlist->mmap[i].base){ // when .base becames 0? isn't it an error??
      ret = record__mmap_read(rec, &rec->evlist->mmap[i]);
      if(ret != 0)
	return -1;
    }
  }
  
  if (perf_header__has_feat(&rec->session->header, HEADER_TRACING_DATA))
    ret = record__write(rec, &finished_round_event, sizeof(finished_round_event));

  return ret;
}

static int perf_record_config(const char *var, const char *value, void *cb)
{
	return perf_default_config(var, value, cb);
}

static int do_record(const char* path, const char* argv[]){
  struct perf_evsel* pos;
  int exit_status, ret;
  char msg[512];
  struct perf_session *session;
  struct record *rec = &record;

  // setup filepath for the output
  rec->file.path = path;

  /** cmd_record() starts here **/
  // create boxes for the counters
  rec->evlist = perf_evlist__new();

  // load and apply default config
  perf_config(perf_record_config, rec);

  // add additinal options
  rec->opts.sample_address = true; // sample linear address from PEBS

  ret = perf_evlist__create_maps(rec->evlist, &unused_target);
  if(ret < 0){
    fprintf(stderr, "perf_evlist__create_maps returned %d\n", ret);
  }

  record_opts__config(&rec->opts);
  
  /** __cmd_record() starts here **/
 
  // create a new session
#if defined(__PERF_VERSION_4__) 
  session = perf_session__new(&rec->file, false, &rec->tool);
#else
  session = perf_session__new(&rec->file, false, NULL);
#endif
  if(session == NULL){
    fprintf(stderr, "perf_session__new returned NULL\n");
    exit(-1);
  }
  rec->session = session;

#if defined(__PERF_VERSION_4__)
  record__init_features(rec);
#endif

  /** record__open() starts here **/
  // set counters
  __mat_parse_events(rec->evlist, "r20D1:pp"); // == parse_options in builtin-record.
  /** record__open() ends here **/

  // prepare workload
  perf_evlist__prepare_workload(rec->evlist, &unused_target, argv, false, workload_exec_failed_signal);

  /** record__open (builtin-record.c) ***************************************/
  __mat_perf_evlist__config(rec->evlist, &rec->opts);
#if defined(__PERF_VERSION_4__)
  evlist__for_each_entry(rec->evlist, pos) {
  try_again:
    ret = perf_evsel__open(pos, pos->cpus, pos->threads);
    if(ret < 0){
      ret = perf_evsel__fallback(pos, errno, msg, sizeof(msg));
      if(ret)
	goto try_again;
    
      perf_evsel__open_strerror(pos, NULL, errno, msg, sizeof(msg));
      fprintf(stderr, "%s\n", msg);
      return -1;
    }
  }
#else
  evlist__for_each(rec->evlist, pos) {
    ret = perf_evsel__open(pos, rec->evlist->cpus, rec->evlist->threads);
    if(ret < 0){
      perf_evsel__open_strerror(pos, NULL, errno, msg, sizeof(msg));
      fprintf(stderr, "%s\n", msg);
      return -1;
    }
  }
#endif
  session->evlist = rec->evlist;
  perf_session__set_id_hdr_size(session);
  /**************************************************************************/

  // mmap actual counters into some memory region.
  // always specify UINT_MAX for the second 
  ret = perf_evlist__mmap(rec->evlist, UINT_MAX, false);
  if(ret < 0)
    return -1;

  // run the workload
  perf_evlist__start_workload(rec->evlist);

  // enable the counters (?)
  perf_evlist__enable(rec->evlist);
  
  // write the magic number to the output file
  perf_session__write_header(session, rec->evlist, rec->file.fd, false);
  
  // read the counters
  for(;;){
#if defined(__PERF_VERSION_4__)
    unsigned long long hits = rec->samples;
#else
    long hits = rec->samples;
#endif

    record__mmap_read_all(rec);
    written_so_far = rec->bytes_written;

    if (hits == rec->samples){ // means `reacord__mmap_read_all' didn't read anything, so we poll
      if(done)
	break;

#if defined(__PERF_VERSION_4__)
      ret = perf_evlist__poll(rec->evlist, -1);
#else
      ret = poll(rec->evlist->pollfd, rec->evlist->nr_fds, -1);
#endif
      if(ret < 0)
	break;
    }
  }

  // read once again after the workload finishes
  record__mmap_read_all(rec);
  
  // wait for the workload to finish
  wait(&exit_status);

  // disable the events
  perf_evlist__disable(rec->evlist);
  
  // overwrite the header with the actually written size
  printf("rec->bytes_written: %lu\n", rec->bytes_written);
  rec->session->header.data_size += rec->bytes_written;
  perf_session__write_header(rec->session, rec->evlist, rec->file.fd, true);

  return 0;
}

/** stuff related to do_report *************************************************************/
// should be placed before "container_of", as it uses this type inside
struct report {
	struct perf_tool	tool;
	struct perf_session	*session;
	bool			force, use_tui, use_gtk, use_stdio;
	bool			hide_unresolved;
	bool			dont_use_callchains;
	bool			show_full_info;
	bool			show_threads;
	bool			inverted_callchain;
	bool			mem_mode;
	bool			header;
	bool			header_only;
	int			max_stack;
	struct perf_read_values	show_threads_values;
	const char		*pretty_printing_style;
	const char		*cpu_list;
	const char		*symbol_filter_str;
	float			min_percent;
	u64			nr_entries;
	DECLARE_BITMAP(cpu_bitmap, MAX_NR_CPUS);
        // members blow here are added by soramichi
        int                     n_samples;
        void                    *address_to_count;
};

static int compare_memory_access(const void* _a, const void* _b){
  const struct memory_access* a = (const struct memory_access*)_a;
  const struct memory_access* b = (const struct memory_access*)_b;

  // sort in the order of the access count
  return b->count - a->count;
}

static int process_sample_event(struct perf_tool *tool  __attribute__((unused)),
				union perf_event *event  __attribute__((unused)),
				struct perf_sample *sample,
				struct perf_evsel *evsel  __attribute__((unused)),
				//				struct perf_evsel *evsel,
				struct machine *machine  __attribute__((unused))){

  struct report* rec = container_of(tool, struct report, tool);
  u64 count;

  rec->n_samples++;

  count = get_from_hash(rec->address_to_count, sample->addr);
  add_to_hash(rec->address_to_count, sample->addr, count + 1);
  
  return 0;
}

static int report__config(const char *var, const char *value, void *cb){
  return perf_default_config(var, value, cb);
}

static int do_report(const char* filename){
  struct perf_session *session;
  struct report report = {
    .tool = {
      .sample		 = process_sample_event,
#if !defined(__PERF_VERSION_4__)
      .ordered_samples = true,
#endif
      .ordering_requires_timestamps = true,
    },
    .max_stack		 = PERF_MAX_STACK_DEPTH,
    .pretty_printing_style	 = "normal",
    .n_samples = 0,
  };
  struct perf_data_file file = {
    .mode  = PERF_DATA_MODE_READ,
  };

  printf("do_report --------------------------------\n");
  
  perf_config(report__config, &report);
  
  file.path  = filename;
  file.force = report.force;

  session = perf_session__new(&file, false, &report.tool);
  report.session = session;
  report.address_to_count = create_hash();
  
  __mat_perf_session__process_events(session, &report.tool);

  printf("%d samples contained\n", report.n_samples);
  printf("Top 5 mostly accessed addresses:\n");
  {
    struct memory_access* memory_access = get_memory_access(report.address_to_count);
    int i;

    qsort(memory_access, get_size_of_hash(report.address_to_count), sizeof(struct memory_access), compare_memory_access);

    for(i=0; i<5; i++){
      printf("0x%lx: %lu\n", memory_access[i].addr, memory_access[i].count);
    }
  }
  
  return 0;
}

/** things not copy-pasted from perf source codes (that means my own logics) *****/
static void* observer(void* arg __attribute__((unused))){
  int written_prev = 0;
  struct record *rec = &record;
  
  for(;;){
    volatile int written_so_far_local = written_so_far;

    if(written_so_far_local > written_prev){
      int fd_out, ret;
      char* filename;
      void* mem_in;
      volatile int header_data_size_old;

      // the file to which the recorder is writing the recorded data
      // cannot be moved outside of for(;;) because rec might not be initizlied then
      volatile int fd_in = rec->file.fd;
 
      mem_in = mmap(NULL, written_so_far_local, PROT_READ, MAP_PRIVATE, fd_in, 0);
      if(mem_in ==  MAP_FAILED){
	perror("mmap(NULL, written_so_far_local, PROT_READ, MAP_PRIVATE, fd_in, 0)");
      }

      filename = make_uniq_path();
      fd_out = open(filename, O_RDWR);

      // write the header, header.data_size is not yet written?
      perf_session__write_header(rec->session, rec->evlist, fd_out, false);
      
      // write the data
      ret = write(fd_out,
		  mem_in +  rec->session->header.data_offset + written_prev,
		  written_so_far_local - written_prev);
      if(ret < written_so_far_local - written_prev){
	perror("write(fd_out, mem_in, written_so_far_local - written_prev)");
      }

      // write the header *again*: needed as perf_session__write_header
      // does something different when the 4th argument is true
      // (this mode is supposed to be used after all data is written)
      // !!!!!! assiging to rec->session->* is a very bad idea,
      // !!!!!!  which 100000000% surely will cause timing issues
      header_data_size_old = rec->session->header.data_size;
      rec->session->header.data_size = written_so_far_local - written_prev;
      perf_session__write_header(rec->session, rec->evlist, fd_out, true);
      rec->session->header.data_size = header_data_size_old;

      // done
      close(fd_out);
      munmap(mem_in, written_so_far_local);
      
      written_prev = written_so_far_local;

      do_report(filename);

      remove(filename);
    }

    usleep(100 * 1000); // wait 500 msec
  }

  return NULL;
}

static void init(void){
  pthread_t tid_observer;
  
  // resides in util.o
  page_size = sysconf(_SC_PAGE_SIZE);

  // singal handling to a child process to work as intended
  atexit(record__sig_exit);
  signal(SIGCHLD, sig_handler);
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  // create observer
  pthread_create(&tid_observer, NULL, observer, NULL);
}

int main(int argc, char* argv[]){
  int i;
  char* path = make_uniq_path();

  // should be done before calling any function in libperf.a
  init();
  
  if(argc == 1){
    fprintf(stderr, "usage: %s program [parameters]\n", "run.sh");
    return -1;
  }
  for(i=0; i<argc-1; i++){
    argv[i] = argv[i+1];
  }
  argv[argc-1] = NULL;

  do_record(path, (const char**)argv);
}
