#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "_struct.h"
#include "sql.h"
#include "macro.h"


extern struct user_send_data usd_buffer[1024]; 
extern user_fd_sign *users[MAX_ALLOWED_CLIENT];
extern int usd_head, usd_tail;
extern pthread_mutex_t buffer_list_mutex;
extern pthread_cond_t buffer_list_cond;
extern int max_buffer_len;
extern int buffer_len;

#define min(a, b) ((a) < (b)) ? (a) : (b)

void *comsume(void *arg) { 
   MYSQL *mysql = (MYSQL *)arg;
   user_send_data *buffer_data = NULL;
   pthread_mutex_lock(&buffer_list_mutex);

   printf("usd_head = %d\tusd_tail = %d\n", usd_head, usd_tail);
   int len = min(ONCE_WRITE_LEN, usd_head - usd_tail) + 1;
   while (len < ONCE_WRITE_LEN) {
      len = min(10, usd_head - usd_tail) + 1;
      printf("data len < ONCE_WRITE_LEN is less so we should wait\n");
      pthread_cond_wait(&buffer_list_cond, &buffer_list_mutex);
      printf("here we may check again .\n");
   }

   // then we read the data.
   printf("in comsume len = %d\n", len);
   if (len > 0) {
      buffer_data = (user_send_data *)malloc(sizeof(user_send_data) * len);
      int i = 0;
      for (i = 0; i < len; ++i) {
         memcpy(&(buffer_data[i]), &(usd_buffer[usd_tail + i]), sizeof(user_send_data));
      }
      for (i = 0; i < len; ++i) {
         printf("buffer_data[%d].fd = %d\n", i, buffer_data[i].fd);
         printf("buffer_data[%d].data = %s\n", i, buffer_data[i].data);
         printf("buffer_data[%d].send_time.tv = %ld\n", i, buffer_data[i].send_time.tv_sec);
      } 
      usd_tail += len;
      if (usd_tail > usd_head) {
         usd_tail = 0;
         usd_head = -1;
      }
   } 
   pthread_mutex_unlock(&buffer_list_mutex);
   if (buffer_data) {
      printf("in comsume write data\n");
      //excuteSql(mysql, "START TRANSACTION");
      mysql_query(mysql, "START TRANSACTION");
      printf("here bug?\n");
      int i = 0;
      char sql[1024] ;
      for (i = 0; i < len; ++i) { 
         unsigned long long project_id_t, device_id_t;
         memcpy(&project_id_t, (users[buffer_data[i].fd]->project_id), 8);
         memcpy(&device_id_t, (users[buffer_data[i].fd]->device_id), 8);
         memset(sql, 0, 1024);
         sprintf(sql, "insert into TRANSMIT values('%s', %d, 0x%llx, 0x%llx, '%s')", users[buffer_data[i].fd]->ip, users[buffer_data[i].fd]->port, project_id_t, device_id_t, buffer_data[i].data);
         mysql_query(mysql, sql);
         printf("insert success\n");
         //TODO:
      }
      //excuteSql(mysql, "COMMIT");
      mysql_query(mysql, "COMMIT");
      // write to database.
   } else {
      printf("buffer data = NULL\n");
   }

   return (void *)0;
}

void product() {
}
