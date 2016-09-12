#ifndef _STRUCT_H
#define _STRUCT_H
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>

struct project_id_type {
   char project_id[8];
   // self definition. no why. but needed.
   bool operator<(const project_id_type &pit) const {
      return (project_id[0] & 0xFF) < (pit.project_id[0] & 0xFF);
   }
};

struct fd_device_id_type {
   char device_id[8];
   int fd;
   bool operator<(const fd_device_id_type &fd2) const {
      return fd < fd2.fd;
   }
};

struct user_data {
   char project_id[8];
   char device_id[8];
};

struct user_send_data {
   int fd;
   char data[256];
   struct timeval send_time;
   user_send_data() {
      fd = 0;
      memset(data, 0, 256);
   }
};

struct user_fd_sign {
   //int fd;
   char ip[32];
   int port;
   char project_id[8];
   char device_id[8];
   struct timeval begin_time;
   struct timeval end_time;
   user_fd_sign() {
      memset(ip, 0, 32);
      port = -1;
   }
};
#endif /** _STRUCT_H */
