#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

struct ring_buffer {
    uint8_t data[2048];
    size_t head;
    size_t len;
};

/**
 * Open a serial device in non-blocking mode.
 *
 * @param path Path to the serial device.
 * @return File descriptor on success, -1 on failure.
 */
int serial_open_nonblocking(const char *path);

/**
 * Configure a serial file descriptor with the expected settings.
 *
 * @param fd File descriptor to configure.
 * @return 0 on success, -1 on failure.
 */
int serial_config(int fd);

/**
 * Read from a serial descriptor without blocking.
 *
 * @param fd File descriptor to read.
 * @param buf Destination buffer.
 * @param max Maximum bytes to read.
 * @return Bytes read, 0 if none available, or -1 on error.
 */
ssize_t serial_poll_read(int fd, uint8_t *buf, size_t max);

/**
 * Initialize a ring buffer for subsequent pushes/reads.
 *
 * @param rb Ring buffer to reset.
 * @return void.
 */
void rb_init(struct ring_buffer *rb);

/**
 * Push data into the ring buffer.
 *
 * @param rb Ring buffer to modify.
 * @param data Pointer to bytes to push.
 * @param len Number of bytes to push.
 * @return void.
 */
void rb_push(struct ring_buffer *rb, const uint8_t *data, size_t len);

/**
 * Find the first occurrence of a start byte in the ring buffer.
 *
 * @param rb Ring buffer to search.
 * @param start_byte Byte value to locate.
 * @return Offset of the byte or -1 if not found.
 */
ssize_t rb_find(struct ring_buffer *rb, uint8_t start_byte);

/**
 * Try to extract an 8-byte frame (variant A) from the ring buffer.
 *
 * @param rb Ring buffer source.
 * @param out8 Output buffer for the frame.
 * @return 1 when a frame is returned, 0 when incomplete, -1 on error.
 */
int rb_try_extract_frame_variantA(struct ring_buffer *rb, uint8_t *out8);

/**
 * Try to extract a 20-byte frame (variant B) from the ring buffer.
 *
 * @param rb Ring buffer source.
 * @param out20 Output buffer for the frame.
 * @return 1 when a frame is returned, 0 when incomplete, -1 on error.
 */
int rb_try_extract_frame_variantB(struct ring_buffer *rb, uint8_t *out20);
