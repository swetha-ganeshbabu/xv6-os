#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

// Defining the default value for tail in macro here
#define DEFAULT_N 10
#define READ_BUF 512


static int myatoi(const char *s) {
  int n = 0;
  int i = 0;
  if (!s) return 0;
  if (s[0] == '-') return -1;
  while (s[i]) {
    if (s[i] < '0' || s[i] > '9') return -1;
    n = n * 10 + (s[i] - '0');
    i++;
  }
  return n;
}

static char* xstrdup(const char *s) {
  int len = 0;
  while (s[len]) len++;
  char *p = (char*)malloc(len + 1);
  if (!p) return 0;
  for (int i = 0; i <= len; i++) p[i] = s[i];
  return p;
}

static char* append_byte(char *buf, int *len, int *cap, char b) {
  if (*len + 1 >= *cap) {
    int newcap = (*cap == 0) ? 128 : (*cap * 2);
    char *n = (char*)malloc(newcap);
    if (!n) {
      return buf;
    }
    if (buf) {
      for (int i = 0; i < *len; i++) n[i] = buf[i];
      free(buf);
    }
    buf = n;
    *cap = newcap;
  }
  buf[*len] = b;
  (*len)++;
  buf[*len] = 0;
  return buf;
}

static void print_last_n_from_fd(int fd, int N) {
  if (N <= 0) return;
  char *lines[N];
  int i;
  for (i = 0; i < N; i++) lines[i] = 0;
  int head = 0;
  int count = 0;
  char rbuf[READ_BUF];
  char *cur = 0;
  int cur_len = 0;
  int cur_cap = 0;
  int nread;
  while ((nread = read(fd, rbuf, READ_BUF)) > 0) {
    for (i = 0; i < nread; i++) {
      char c = rbuf[i];
      cur = append_byte(cur, &cur_len, &cur_cap, c);
      if (c == '\n') {
        char *dup = xstrdup(cur);
        if (lines[head]) free(lines[head]);
        lines[head] = dup;
        head = (head + 1) % N;
        if (count < N) count++;
        free(cur);
        cur = 0;
        cur_len = 0;
        cur_cap = 0;
      }
    }
  }
  if (cur_len > 0) {
    char *dup = xstrdup(cur);
    if (lines[head]) free(lines[head]);
    lines[head] = dup;
    head = (head + 1) % N;
    if (count < N) count++;
    free(cur);
    cur = 0;
  }
  int start = (head - count + N) % N;
  for (i = 0; i < count; i++) {
    int idx = (start + i) % N;
    if (lines[idx]) {
      write(1, lines[idx], strlen(lines[idx]));
      free(lines[idx]);
      lines[idx] = 0;
    }
  }
}

static int print_last_n_file(const char *fname, int N) {
  int fd = open(fname, O_RDONLY);
  if (fd < 0) {
    printf(2, "tail: cannot open %s\n", fname);
    return -1;
  }
  print_last_n_from_fd(fd, N);
  close(fd);
  return 0;
}

int main(int argc, char *argv[]) {
  int N = DEFAULT_N;
  int idx = 1;
  if (argc > 1) {
    if (argv[1][0] == '-') {
      if (argv[1][1] == 'n') {
        if (argv[1][2] != 0) {
          int val = myatoi(&argv[1][2]);
          if (val >= 0) N = val;
          else { printf(2, "tail: invalid number: %s\n", &argv[1][2]); exit(); }
          idx = 2;
        } else {
          if (argc >= 3) {
            int val = myatoi(argv[2]);
            if (val >= 0) N = val;
            else { printf(2, "tail: invalid number: %s\n", argv[2]); exit(); }
            idx = 3;
          } else { printf(2, "tail: missing number after -n\n"); exit(); }
        }
      } else {
        int val = myatoi(&argv[1][1]);
        if (val >= 0) { N = val; idx = 2; }
        else { idx = 1; }
      }
    }
  }
  if (idx >= argc) {
    print_last_n_from_fd(0, N);
    exit();
  }
  int files = argc - idx;
  int i;
  for (i = idx; i < argc; i++) {
    if (files > 1) {
      printf(1, "=> %s <=\n", argv[i]);
    }
    print_last_n_file(argv[i], N);
    if (i != argc - 1) {
      printf(1, "\n");
    }
  }
  exit();
}
