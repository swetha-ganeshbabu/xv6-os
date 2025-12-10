/*
 * uthreads_io.c - Thread-Safe File I/O Implementation
 *
 * Provides thread-safe wrappers for xv6 file operations with cooperative
 * yielding to work better with user-level threading.
 */

#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "uthreads.h"
#include "uthreads_io.h"

/*
 * tio_init - Initialize a thread-safe file wrapper
 */
void tio_init(tio_file_t *file, int fd) {
    if (file == 0) return;

    file->fd = fd;
    file->owner_tid = -1;
    mutex_init(&file->lock);
}

/*
 * tio_open - Thread-safe file open
 */
tio_file_t* tio_open(char *path, int mode) {
    // Allocate the wrapper structure
    tio_file_t *file = (tio_file_t*)malloc(sizeof(tio_file_t));
    if (file == 0) {
        return 0;
    }

    // Open the file
    int fd = open(path, mode);
    if (fd < 0) {
        free(file);
        return 0;
    }

    // Initialize the wrapper
    tio_init(file, fd);

    return file;
}

/*
 * tio_close - Thread-safe file close
 */
int tio_close(tio_file_t *file) {
    if (file == 0) {
        return -1;
    }

    // Acquire lock to ensure no one is using the file
    mutex_lock(&file->lock);

    int result = -1;
    if (file->fd >= 0) {
        result = close(file->fd);
        file->fd = -1;
    }

    mutex_unlock(&file->lock);

    // Free the wrapper
    free(file);

    return result;
}

/*
 * tio_read - Thread-safe read with cooperative yielding
 */
int tio_read(tio_file_t *file, void *buf, int n) {
    if (file == 0 || buf == 0 || n <= 0) {
        return -1;
    }

    // Acquire the file's mutex
    mutex_lock(&file->lock);

    file->owner_tid = thread_self();

    // Yield before blocking I/O to let other threads run
    // Note: This is cooperative - the actual read() syscall will still block
    // all threads, but we yield to be a good citizen
    thread_yield();

    // Perform the read
    int bytes_read = read(file->fd, buf, n);

    // Yield after I/O completes
    thread_yield();

    file->owner_tid = -1;

    // Release the mutex
    mutex_unlock(&file->lock);

    return bytes_read;
}

/*
 * tio_write - Thread-safe write with cooperative yielding
 */
int tio_write(tio_file_t *file, void *buf, int n) {
    if (file == 0 || buf == 0 || n <= 0) {
        return -1;
    }

    // Acquire the file's mutex
    mutex_lock(&file->lock);

    file->owner_tid = thread_self();

    // Yield before blocking I/O
    thread_yield();

    // Perform the write
    int bytes_written = write(file->fd, buf, n);

    // Yield after I/O completes
    thread_yield();

    file->owner_tid = -1;

    // Release the mutex
    mutex_unlock(&file->lock);

    return bytes_written;
}

/*
 * tio_pipe - Create a thread-safe pipe
 */
int tio_pipe(tio_file_t **read_end, tio_file_t **write_end) {
    if (read_end == 0 || write_end == 0) {
        return -1;
    }

    // Create the pipe
    int fds[2];
    if (pipe(fds) < 0) {
        return -1;
    }

    // Allocate and initialize read end
    *read_end = (tio_file_t*)malloc(sizeof(tio_file_t));
    if (*read_end == 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    tio_init(*read_end, fds[0]);

    // Allocate and initialize write end
    *write_end = (tio_file_t*)malloc(sizeof(tio_file_t));
    if (*write_end == 0) {
        close(fds[0]);
        close(fds[1]);
        free(*read_end);
        *read_end = 0;
        return -1;
    }
    tio_init(*write_end, fds[1]);

    return 0;
}

/*
 * tio_read_fd - Cooperative read on raw file descriptor
 *
 * Simpler version for use in forked processes where the threading
 * library is re-initialized.
 */
int tio_read_fd(int fd, void *buf, int n) {
    if (fd < 0 || buf == 0 || n <= 0) {
        return -1;
    }

    // Yield before blocking I/O
    thread_yield();

    // Perform the read
    int bytes_read = read(fd, buf, n);

    // Yield after I/O
    thread_yield();

    return bytes_read;
}

/*
 * tio_write_fd - Cooperative write on raw file descriptor
 */
int tio_write_fd(int fd, void *buf, int n) {
    if (fd < 0 || buf == 0 || n <= 0) {
        return -1;
    }

    // Yield before blocking I/O
    thread_yield();

    // Perform the write
    int bytes_written = write(fd, buf, n);

    // Yield after I/O
    thread_yield();

    return bytes_written;
}
