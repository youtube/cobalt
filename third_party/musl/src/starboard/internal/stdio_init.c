#include "stdio_impl.h"
#include "pthread_impl.h"

static pthread_once_t __std_files_once = PTHREAD_ONCE_INIT;

void __init_std_files() {
  __stdin_FILE.lock = __init_file_lock(&__stdin_FILE);
  __stdout_FILE.lock = __init_file_lock(&__stdout_FILE);
  __stderr_FILE.lock = __init_file_lock(&__stderr_FILE);
}

size_t __stdio_read_stub(FILE *f, unsigned char *buf, size_t len) {
  return 0;
}

size_t __stdio_write_stub(FILE *f, const unsigned char *buf, size_t len) {
  return 0;
}

size_t __stdio_read_init(FILE *f, unsigned char *buf, size_t len) {
  pthread_once(&__std_files_once, __init_std_files);
  f->read = __stdio_read;
  return __stdio_read(f, buf, len);
}

size_t __stdio_write_init(FILE *f, const unsigned char *buf, size_t len) {
  pthread_once(&__std_files_once, __init_std_files);
  f->write = __stdout_write;
  return __stdout_write(f, buf, len);
}

off_t __stdio_seek_init(FILE *f, off_t off, int whence) {
  pthread_once(&__std_files_once, __init_std_files);
  f->seek = __stdio_seek;
  return __stdio_seek(f, off, whence);
}
