#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include<netinet/in.h>
#include <stdlib.h>
#include "utils.h"

// writen n bytes to socket and return the number of bytes written or -1 on error
ssize_t writen(int socket_fd, const void *buffer, size_t n)
{
   ssize_t written;
   ssize_t left = n;
   char *cur = (char *)buffer;

   while(left > 0)
   {
      written = write(socket_fd, cur, left);

      if(written < 0)
      {
         if(errno == EINTR)
         {
            continue;
         }
         else
         {
            return -1;
         }
      }

      if(written == 0)
      {
         return n - left;
      }

      cur += written;
      left -= written;
   }

   return n;
}

// read n bytes from socket
ssize_t read_n_bytes(int socket_fd, void *buffer, size_t count)
{
   ssize_t len = 0;
   ssize_t left = count;
   char *cur = (char *)buffer;

   while(left > 0)
   {
      len = read(socket_fd, cur, left);

      if(len == 0)
      {
         return count - left;
      }

      if(len < 0)
      {
         if(errno == EINTR)
         {
            continue;
         }
         else
         {
            return -1;
         }
      }

      cur += len;
      left = count - len;
   }

   return count;
}

// read a line, the length of a "line" is set to 10 in this project
ssize_t readline(int socket_fd, void *buffer, size_t len)
{
   ssize_t n = 0;
   ssize_t left = len;
   ssize_t total;
   char *cur = buffer;

   while(1)
   {
      while(1)
      {
         total = recv(socket_fd, (void *)cur, len, MSG_PEEK);

         if(total == -1 && errno == EINTR)
         {
            continue;
         }
         else
         {
            break;
         }
      }

      if((n = total) <= 0)
      {
         return total;
      }

      int i = 0;

      for(i = 0; i < n; i++)
      {
         if(cur[i] == '\n')
         {
            return read_n_bytes(socket_fd, cur, i + 1);
         }
      }

      left -= n;
      total = read_n_bytes(socket_fd, cur, n);

      cur += n;

      if(left <= 0)
      {
         return n;
      }
   }

   return -1;
}
