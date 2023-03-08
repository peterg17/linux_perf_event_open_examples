#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <sys/mman.h>
#include <linux/hw_breakpoint.h>
#include <asm/unistd.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

struct perf_sample_event {
  struct perf_event_header hdr;
  uint64_t    sample_id;                // if PERF_SAMPLE_IDENTIFIER
  uint64_t    ip;                       // if PERF_SAMPLE_IP
  uint32_t    pid, tid;                 // if PERF_SAMPLE_TID
  uint64_t    time;                     // if PERF_SAMPLE_TIME
  uint64_t    addr;                     // if PERF_SAMPLE_ADDR
  uint64_t    id;                       // if PERF_SAMPLE_ID
  uint64_t    stream_id;                // if PERF_SAMPLE_STREAM_ID
  uint32_t    cpu, res;                 // if PERF_SAMPLE_CPU
  uint64_t    period;                   // if PERF_SAMPLE_PERIOD
  struct      read_format *v;           // if PERF_SAMPLE_READ
  uint64_t    nr;                       // if PERF_SAMPLE_CALLCHAIN
  uint64_t    *ips;                     // if PERF_SAMPLE_CALLCHAIN
  uint32_t    size_raw;                 // if PERF_SAMPLE_RAW
  char        *data_raw;                // if PERF_SAMPLE_RAW
  uint64_t    bnr;                      // if PERF_SAMPLE_BRANCH_STACK
  struct      perf_branch_entry *lbr;   // if PERF_SAMPLE_BRANCH_STACK
  uint64_t    abi;                      // if PERF_SAMPLE_REGS_USER
  uint64_t    *regs;                    // if PERF_SAMPLE_REGS_USER
  uint64_t    size_stack;               // if PERF_SAMPLE_STACK_USER
  char        *data_stack;              // if PERF_SAMPLE_STACK_USER
  uint64_t    dyn_size_stack;           // if PERF_SAMPLE_STACK_USER
  uint64_t    weight;                   // if PERF_SAMPLE_WEIGHT
  uint64_t    data_src;                 // if PERF_SAMPLE_DATA_SRC
  uint64_t    transaction;              // if PERF_SAMPLE_TRANSACTION
  uint64_t    abi_intr;                 // if PERF_SAMPLE_REGS_INTR
  uint64_t    *regs_intr;               // if PERF_SAMPLE_REGS_INTR
};

int fib(int n) {
  if (n == 0) {
    return 0;
  } else if (n == 1 || n == 2) {
    return 1;
  } else {
    return fib(n-1) + fib(n-2);
  }
}

void do_something() {
  int i;
  char* ptr;

  ptr = malloc(100*1024*1024);
  for (i = 0; i < 100*1024*1024; i++) {
    ptr[i] = (char) (i & 0xff); // pagefault
  }
  free(ptr);
}

void insertion_sort(int *nums, size_t n) {
  int i = 1;
  while (i < n) {
    int j = i;
    while (j > 0 && nums[j-1] > nums[j]) {
      int tmp = nums[j];
      nums[j] = nums[j-1];
      nums[j-1] = tmp;
      j -= 1;
    }
    i += 1;
  }
}

static void process_ring_buffer_events(struct perf_event_mmap_page *data, int page_size) {
  struct perf_event_header *header = (uintptr_t) data + page_size + data->data_tail;
  void *end = (uintptr_t) data + page_size + data->data_head; 

  while (header != end) {
    if (header->type == PERF_RECORD_SAMPLE) {
      struct perf_sample_event *event = (uintptr_t) header;
      uint64_t ip = event->ip;
      printf("PERF_RECORD_SAMPLE found with ip: %lld\n", ip);
      uint64_t size_stack = event->size_stack;
      char *data_stack = (uintptr_t) event->data_stack;
      if (data_stack > 0) {
        printf("PERF_RECORD_SAMPLE has size stack: %lld at location: %lld\n", size_stack, data_stack);
      }
    } else {
      printf("other type %d found!", header->type);
    }
    header = (uintptr_t) header + header->size;
  }
}

int main(int argc, char* argv[]) {
  struct perf_event_attr pea;

  int fd1, fd2;
  uint64_t id1, id2;
  uint64_t val1, val2;
  char buf[4096];
  const int NUM_MMAP_PAGES = (1U << 4) + 1;
  // const int NUM_MMAP_PAGES = 17;
  int i;

  int some_nums[1000]; 
  for (int i=0; i < 1000; i++) {
    some_nums[i] = 1000-i;
  }

  memset(&pea, 0, sizeof(struct perf_event_attr));
  pea.type = PERF_TYPE_SOFTWARE;
  pea.size = sizeof(struct perf_event_attr);
  pea.config = PERF_COUNT_SW_CPU_CLOCK;
  pea.disabled = 1;
  pea.exclude_kernel = 1;
  pea.exclude_hv = 0;
  pea.sample_period = 1;
  pea.precise_ip = 3;
  pea.sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_STACK_USER;
  // pea.sample_type = PERF_SAMPLE_IP;
  pea.sample_stack_user = 10000;
  fd1 = syscall(__NR_perf_event_open, &pea, 0, -1, -1, 0);

  printf("size of perf_event_mmap_page struct is %d\n", sizeof(struct perf_event_mmap_page));
  int page_size = (int) sysconf(_SC_PAGESIZE);

  printf("page size in general is: %d\n", page_size);

 
   // Map the ring buffer into memory
  struct perf_event_mmap_page *pages = mmap(NULL, page_size * NUM_MMAP_PAGES, PROT_READ
    | PROT_WRITE, MAP_SHARED, fd1, 0);
  if (pages == MAP_FAILED) {
      perror("Error mapping ring buffer");
      return 1;
  }

  ioctl(fd1, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
  ioctl(fd1, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
  // do_something();
  // fib(40);
  size_t n = sizeof(some_nums)/sizeof(some_nums[0]);
  insertion_sort(&some_nums, n);
  ioctl(fd1, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);

  printf("head of perf ring buffer is at: %d", pages->data_head);
  process_ring_buffer_events(pages, page_size);
  munmap(pages, page_size * NUM_MMAP_PAGES);

  return 0;
}