#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "util/session.h"
#include "util/evlist.h"
#include "util/values.h"
#include "util/parse-events.h"
#include "cache.h"

// dirt hack. those variables are never used in mat.c but
// required by libperf.a (should be removed from perf.c)
int use_browser = -1;
const char perf_version_string[] = "";

/** shared variable ************************************************************************/
volatile int written_so_far;
volatile int read_so_far;
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

// should be global, which is what builtin-record says :(
static struct record record = {
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
};
/*************************************************************************/

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

  printf("samples:%ld\n", rec->samples);
  
  if (perf_header__has_feat(&rec->session->header, HEADER_TRACING_DATA))
    ret = record__write(rec, &finished_round_event, sizeof(finished_round_event));

  return ret;
}

static int perf_record_config(const char *var, const char *value, void *cb)
{
	return perf_default_config(var, value, cb);
}

static void* observer(void* arg __attribute__((unused))){
  int written_prev = 0;
  struct record *rec = &record;
  
  for(;;){
    if(written_so_far > written_prev){
      int fd_out, ret;
      char* filename;
      void* mem_in;
      volatile int header_data_size_old;

      // the file to which the recorder is writing the recorded data
      // cannot be moved outside of for(;;) because rec might not be initizlied then
      volatile int fd_in = rec->file.fd;
 
      printf("rec->file.fd: %d\n", rec->file.fd);
      printf("mmap(NULL, %d, PROT_READ, MAP_PRIVATE, %d, 0)\n", written_so_far, fd_in);
      mem_in = mmap(NULL, written_so_far, PROT_READ, MAP_PRIVATE, fd_in, 0);
      if(mem_in ==  MAP_FAILED){
	perror("mmap(NULL, written_so_far, PROT_READ, MAP_PRIVATE, fd_in, 0)");
      }

      printf("bytes written so far: %d\n", written_so_far);

      filename = make_uniq_path();
      fd_out = open(filename, O_RDWR);

      // write the header
      header_data_size_old = rec->session->header.data_size;
      rec->session->header.data_size = written_so_far - written_prev;
      perf_session__write_header(rec->session, rec->evlist, fd_out, false);
      rec->session->header.data_size = header_data_size_old;
      
      // write the data
      ret = write(fd_out, mem_in + written_prev, written_so_far - written_prev);
      if(ret < written_so_far - written_prev){
	perror("write(fd_out, mem_in, written_so_far - written_prev)");
      }

      // done
      printf("Saved the data into %s\n", filename);
      close(fd_out);
      munmap(mem_in, written_so_far);
    }

    written_prev = written_so_far;

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

static int do_record(const char* path){
  struct perf_evsel* pos;
  const char* argv[] = {"./cache_miss", NULL}; 
  int exit_status, ret;
  char msg[512];
  struct perf_session *session;
  struct record *rec = &record;

  // setup filepath for the output
  rec->file.path = path;
  
  // create boxes for the counters
  rec->evlist = perf_evlist__new();

  // load and apply default config
  perf_config(perf_record_config, rec);
  record_opts__config(&rec->opts);

  // add additinal options
  rec->opts.sample_address = true; // sample linear address from PEBS
  
  // create a new session
  session = perf_session__new(&rec->file, false, NULL);
  rec->session = session;
  
  // set counters
  //parse_events(rec->evlist, "cache-misses:pp");
  parse_events(rec->evlist, "r81D0:pp");
  perf_evlist__create_maps(rec->evlist, &unused_target);

  // prepare workload
  perf_evlist__prepare_workload(rec->evlist, &unused_target, argv, false, workload_exec_failed_signal);

  /** record__open (builtin-record.c) ***************************************/
  perf_evlist__config(rec->evlist, &rec->opts);
  evlist__for_each(rec->evlist, pos) {
    ret = perf_evsel__open(pos, rec->evlist->cpus, rec->evlist->threads);
    if(ret < 0){
      perf_evsel__open_strerror(pos, NULL, errno, msg, sizeof(msg));
      fprintf(stderr, "%s\n", msg);
      return -1;
    }
  }
  session->evlist = rec->evlist;
  perf_session__set_id_hdr_size(session);
  /**************************************************************************/

  // mmap actual counters into some memory region.
  // always specify UINT_MAX for the second 
  ret = perf_evlist__mmap(rec->evlist, UINT_MAX, false);
  if(ret < 0)
    return -1;

  // now that evlist is ready for use
  printf("evlist->nr_fds: %d\n", rec->evlist->nr_fds);
  printf("evlist->nr_mmaps: %d\n", rec->evlist->nr_mmaps);
  printf("rec->file.path: %s\n", rec->file.path);
  printf("rec->file.fd: %d\n", rec->file.fd);

  // run the workload
  perf_evlist__start_workload(rec->evlist);

  // enable the counters (?)
  perf_evlist__enable(rec->evlist);
  
  // write the magic number to the output file
  perf_session__write_header(session, rec->evlist, rec->file.fd, false);
  
  // read the counters
  for(;;){
    int hits = rec->samples;

    record__mmap_read_all(rec);
    written_so_far = rec->bytes_written;

    if (hits == rec->samples){ // means `reacord__mmap_read_all' didn't read anything, so we poll
      ret = poll(rec->evlist->pollfd, rec->evlist->nr_fds, -1);
      printf("poll(2) returned with %d\n", ret);
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
};

static int process_sample_event(struct perf_tool *tool  __attribute__((unused)),
				union perf_event *event  __attribute__((unused)),
				struct perf_sample *sample,
				struct perf_evsel *evsel  __attribute__((unused)),
				struct machine *machine  __attribute__((unused))){
  printf("ip: 0x%lx, addr: 0x%lx\n", sample->ip, sample->addr);

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
      .ordered_samples = true,
      .ordering_requires_timestamps = true,
    },
    .max_stack		 = PERF_MAX_STACK_DEPTH,
    .pretty_printing_style	 = "normal",
  };
  struct perf_data_file file = {
    .mode  = PERF_DATA_MODE_READ,
  };

  printf("do_report: %s\n", filename);
  
  perf_config(report__config, &report);
  
  file.path  = filename;
  file.force = report.force;

  session = perf_session__new(&file, false, &report.tool);
  report.session = session;

  /** Example output:
     # ========
     # captured on: Tue Jan 17 12:18:22 2017
     # ========
     #
   **/
  perf_session__fprintf_info(session, stdout, report.show_full_info);

  perf_session__process_events(session, &report.tool);

  return 0;
}

int main(void){
  char* path = make_uniq_path();

  // should be done before calling any function in libperf.a
  init();
  
  do_record(path);
  //do_report(path);
}
