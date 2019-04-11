#ifndef UTILS_H__
#define UTILS_H__

#include <stdio.h>
#include <unistd.h>
#include <errno.h>

ssize_t writen(int target_fd, const void *buffer, size_t n);

ssize_t readn(int fd, void *buf, size_t count);

ssize_t recv_peek(int sockfd, void *buf, size_t len);

ssize_t readline(int sockfd, void *buf, size_t len);

#endif
