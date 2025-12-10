/*
 * uthreads_io.h - Thread-Safe File I/O for User-Level Threading Library
 *
 * This module provides thread-safe wrappers around xv6 file I/O operations.
 *
 * IMPORTANT: In a user-level threading model (N:1), when ANY thread makes
 * a blocking system call (like read/write), ALL threads in the process block.
 * This is a fundamental limitation.
 *
 * To achieve true concurrent I/O, we use SEPARATE PROCESSES that communicate
 * via pipes. Each process has its own thread scheduler, so when one process
 * blocks on I/O, the kernel can schedule another process.
 *
 * Usage Pattern:
 *   - Use tio_open/tio_close for file operations
 *   - Use tio_read/tio_write for I/O (these yield before/after to be cooperative)
 *   - For true async behavior, use pipes between separate processes
 */

#ifndef UTHREADS_IO_H
#define UTHREADS_IO_H

#include "uthreads.h"

/*
 * Thread-safe file descriptor wrapper
 * Tracks which thread owns a file operation to prevent concurrent access
 */
typedef struct {
    int fd;             // Underlying file descriptor
    int owner_tid;      // TID of thread currently using this fd (-1 if none)
    mutex_t lock;       // Protects the file descriptor
} tio_file_t;

/*
 * tio_init - Initialize a thread-safe file wrapper
 *
 * @file: Pointer to tio_file_t structure
 * @fd: File descriptor to wrap (-1 for uninitialized)
 */
void tio_init(tio_file_t *file, int fd);

/*
 * tio_open - Thread-safe file open
 *
 * @path: Path to the file
 * @mode: Open mode (O_RDONLY, O_WRONLY, O_RDWR, O_CREATE)
 *
 * Returns: Initialized tio_file_t pointer, or 0 on failure
 * Note: Caller must free the returned structure when done
 */
tio_file_t* tio_open(char *path, int mode);

/*
 * tio_close - Thread-safe file close
 *
 * @file: Thread-safe file wrapper
 *
 * Returns: 0 on success, -1 on failure
 */
int tio_close(tio_file_t *file);

/*
 * tio_read - Thread-safe read with cooperative yielding
 *
 * @file: Thread-safe file wrapper
 * @buf: Buffer to read into
 * @n: Number of bytes to read
 *
 * Returns: Number of bytes read, or -1 on error
 *
 * This function:
 * 1. Acquires the file's mutex
 * 2. Yields to allow other threads to run (cooperative)
 * 3. Performs the read
 * 4. Yields again
 * 5. Releases the mutex
 */
int tio_read(tio_file_t *file, void *buf, int n);

/*
 * tio_write - Thread-safe write with cooperative yielding
 *
 * @file: Thread-safe file wrapper
 * @buf: Buffer to write from
 * @n: Number of bytes to write
 *
 * Returns: Number of bytes written, or -1 on error
 */
int tio_write(tio_file_t *file, void *buf, int n);

/*
 * tio_pipe - Create a thread-safe pipe
 *
 * @read_end: Pointer to store read end tio_file_t
 * @write_end: Pointer to store write end tio_file_t
 *
 * Returns: 0 on success, -1 on failure
 */
int tio_pipe(tio_file_t **read_end, tio_file_t **write_end);

/*
 * =============================================================================
 * Raw I/O functions (for use with raw file descriptors in forked processes)
 * =============================================================================
 * These are simpler wrappers that work with raw fds, useful when the
 * threading library state isn't shared across fork().
 */

/*
 * tio_read_fd - Cooperative read on raw file descriptor
 *
 * @fd: File descriptor
 * @buf: Buffer to read into
 * @n: Number of bytes to read
 *
 * Returns: Number of bytes read, or -1 on error
 */
int tio_read_fd(int fd, void *buf, int n);

/*
 * tio_write_fd - Cooperative write on raw file descriptor
 *
 * @fd: File descriptor
 * @buf: Buffer to write from
 * @n: Number of bytes to write
 *
 * Returns: Number of bytes written, or -1 on error
 */
int tio_write_fd(int fd, void *buf, int n);

#endif /* UTHREADS_IO_H */
