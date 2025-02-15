//--from
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include "libcgo.h"
#include "libcgo_unix.h"
//--to
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include "libcgo.h"
#include "libcgo_unix.h"

#include <unistd.h> // for usleep
//--from
void
x_cgo_init(G *g, void (*setg)(void*), void **tlsg, void **tlsbase)
{
	pthread_attr_t *attr;
	size_t size;

	/* The memory sanitizer distributed with versions of clang
	   before 3.8 has a bug: if you call mmap before malloc, mmap
	   may return an address that is later overwritten by the msan
	   library.  Avoid this problem by forcing a call to malloc
	   here, before we ever call malloc.

	   This is only required for the memory sanitizer, so it's
	   unfortunate that we always run it.  It should be possible
	   to remove this when we no longer care about versions of
	   clang before 3.8.  The test for this is
	   misc/cgo/testsanitizers.

	   GCC works hard to eliminate a seemingly unnecessary call to
	   malloc, so we actually use the memory we allocate.  */

	setg_gcc = setg;
	attr = (pthread_attr_t*)malloc(sizeof *attr);
	if (attr == NULL) {
		fatalf("malloc failed: %s", strerror(errno));
	}
	pthread_attr_init(attr);
	pthread_attr_getstacksize(attr, &size);
	g->stacklo = (uintptr)&size - size + 4096;
	pthread_attr_destroy(attr);
	free(attr);

	if (x_cgo_inittls) {
		x_cgo_inittls(tlsg, tlsbase);
	}
}
//--to
void
x_cgo_init(G *g, void (*setg)(void*), void **tlsg, void **tlsbase)
{
	pthread_attr_t *attr;
	size_t size;

	/* The memory sanitizer distributed with versions of clang
	   before 3.8 has a bug: if you call mmap before malloc, mmap
	   may return an address that is later overwritten by the msan
	   library.  Avoid this problem by forcing a call to malloc
	   here, before we ever call malloc.

	   This is only required for the memory sanitizer, so it's
	   unfortunate that we always run it.  It should be possible
	   to remove this when we no longer care about versions of
	   clang before 3.8.  The test for this is
	   misc/cgo/testsanitizers.

	   GCC works hard to eliminate a seemingly unnecessary call to
	   malloc, so we actually use the memory we allocate.  */

	setg_gcc = setg;
	attr = (pthread_attr_t*)malloc(sizeof *attr);
	if (attr == NULL) {
		fatalf("malloc failed: %s", strerror(errno));
	}
	pthread_attr_init(attr);
	pthread_attr_setstacksize(attr, 16 * 4096); // Hack for some special environments
	pthread_attr_getstacksize(attr, &size);
	g->stacklo = (uintptr)&size - size + 4096;
	pthread_attr_destroy(attr);
	free(attr);

	if (x_cgo_inittls) {
		x_cgo_inittls(tlsg, tlsbase);
	}
}
//--append
// Define C functions and system calls for Cgo.

typedef unsigned int gid_t;

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
  abort();
  return NULL;
}

int munmap(void *addr, size_t length) {
  abort();
  return 0;
}

int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset) {
  // Do nothing.
  return 0;
}

int setegid(gid_t gid) {
  // Do nothing.
  return 0;
}

int seteuid(uid_t gid) {
  // Do nothing.
  return 0;
}

int setgid(gid_t gid) {
  // Do nothing.
  return 0;
}

int setgroups(size_t size, const gid_t *list) {
  // Do nothing.
  return 0;
}

int setregid(gid_t rgid, gid_t egid) {
  // Do nothing.
  return 0;
}

int setreuid(uid_t ruid, uid_t euid) {
  // Do nothing.
  return 0;
}

int setresgid(gid_t rgid, gid_t egid, gid_t sgid) {
  // Do nothing.
  return 0;
}

int setresuid(uid_t ruid, uid_t euid, uid_t suid) {
  // Do nothing.
  return 0;
}

int setuid(uid_t gid) {
  // Do nothing.
  return 0;
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
  // Do nothing.
  return 0;
}

int sigaddset(sigset_t *set, int signum) {
  // Do nothing.
  return 0;
}

int sigemptyset(sigset_t *set) {
  // Do nothing.
  return 0;
}

int sigfillset(sigset_t *set) {
  // Do nothing.
  return 0;
}

int sigismember(const sigset_t *set, int signum) {
  // Do nothing.
  return 0;
}

static const int kFDOffset = 100;

typedef struct {
  const void* content;
  size_t      content_size;
  size_t      current;
  int32_t     fd;
} pseudo_file;

// TODO: Do we need to protect this by mutex?
static pseudo_file pseudo_files[100];

static int32_t open_pseudo_file(const void* content, size_t content_size) {
  int index = 0;
  int found = 0;
  for (int i = 0; i < sizeof(pseudo_files) / sizeof(pseudo_file); i++) {
    if (pseudo_files[i].fd == 0) {
      index = i;
      found = 1;
      break;
    }
  }
  if (!found) {
    // Too many pseudo files are opened.
    return -1;
  }
  int32_t fd = index + kFDOffset;
  pseudo_files[index].content = content;
  pseudo_files[index].content_size = content_size;
  pseudo_files[index].current = 0;
  pseudo_files[index].fd = fd;
  return fd;
}

static size_t read_pseudo_file(int32_t fd, void *p, int32_t n) {
  int32_t index = fd - kFDOffset;
  pseudo_file *file = &pseudo_files[index];
  size_t rest = file->content_size - file->current;
  if (rest < n) {
    n = rest;
  }
  memcpy(p, file->content + file->current, n);
  pseudo_files[index].current += n;
  return n;
}

static void close_pseudo_file(int32_t fd) {
  int32_t index = fd - kFDOffset;
  pseudo_files[index].content = NULL;
  pseudo_files[index].content_size = 0;
  pseudo_files[index].current = 0;
  pseudo_files[index].fd = 0;
}

int32_t c_closefd(int32_t fd) {
  if (fd >= kFDOffset) {
    close_pseudo_file(fd);
    return 0;
  }
  fprintf(stderr, "syscall close(%d) is not implemented\n", fd);
  return 0;
}

void c_gettid(uint64_t *ret) {
  *ret = (uint64_t)(pthread_self());
}

void c_calloc(void **ret, size_t num, size_t size) {
  *ret = calloc(num, size);
}

void c_nanotime1(int64_t *ret) {
  struct timespec tp;
  clock_gettime(CLOCK_MONOTONIC, &tp);
  *ret = (int64_t)(tp.tv_sec) * 1000000000ll + (int64_t)tp.tv_nsec;
}

int32_t c_open(char *name, int32_t mode, int32_t perm) {
  if (strcmp(name, "/proc/self/auxv") == 0) {
    static const char auxv[] =
      "\x06\x00\x00\x00\x00\x00\x00\x00"  // _AT_PAGESZ tag (6)
      "\x00\x10\x00\x00\x00\x00\x00\x00"  // 4096 bytes per page
      "\x00\x00\x00\x00\x00\x00\x00\x00"  // Dummy bytes
      "\x00\x00\x00\x00\x00\x00\x00\x00"; // Dummy bytes
    return open_pseudo_file(auxv, sizeof(auxv) / sizeof(char));
  }
  if (strcmp(name, "/sys/kernel/mm/transparent_hugepage/hpage_pmd_size") == 0) {
    static const char hpage_pmd_size[] =
      "\x30\x5c"; // '0', '\n'
    return open_pseudo_file(hpage_pmd_size, sizeof(hpage_pmd_size) / sizeof(char));
  }
  fprintf(stderr, "syscall open(%s, %d, %d) is not implemented\n", name, mode, perm);
  return -1;
}

int32_t c_osyield() {
  return sched_yield();
}

int32_t c_read(int32_t fd, void *p, int32_t n) {
  if (fd >= kFDOffset) {
    return read_pseudo_file(fd, p, n);
  }
  fprintf(stderr, "syscall read(%d, %p, %d) is not implemented\n", fd, p, n);
  return 0;
}

int32_t c_sched_getaffinity(pid_t pid, size_t cpusetsize, void *mask) {
  // 4 CPUs
  ((char*)mask)[0] = 0xf;

  // https://man7.org/linux/man-pages/man2/sched_setaffinity.2.html
  // > On success, the raw sched_getaffinity() system call returns the
  // > number of bytes placed copied into the mask buffer;
  return 1;
}

int32_t c_usleep(useconds_t usec) {
  return usleep(usec);
}

int32_t c_write1(uintptr_t fd, void *p, int32_t n) {
  static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
  int32_t ret = 0;
  pthread_mutex_lock(&m);
  switch (fd) {
  case 1:
    ret = fwrite(p, 1, n, stdout);
    break;
  case 2:
    ret = fwrite(p, 1, n, stderr);
    break;
  default:
    fprintf(stderr, "syscall write(%lu, %p, %d) is not implemented\n", fd, p, n);
    break;
  }
  pthread_mutex_unlock(&m);
  return ret;
}

// pthread

void c_pthread_cond_timedwait_relative_np(int *ret, pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *reltime) {
  struct timespec abstime;
  clock_gettime(CLOCK_REALTIME, &abstime);
  abstime.tv_sec += reltime->tv_sec;
  abstime.tv_nsec += reltime->tv_nsec;
  if (1000000000 <= abstime.tv_nsec) {
    abstime.tv_sec += 1;
    abstime.tv_nsec -= 1000000000;
  }
  *ret = pthread_cond_timedwait(cond, mutex, &abstime);
}
