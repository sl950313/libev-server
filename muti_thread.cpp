#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "_struct.h"
#include "sql.h"
#include "macro.h"
#include "mainwindow.h"
#include "utils.h"


extern struct user_send_data usd_buffer[1024]; 
extern user_fd_sign *users[MAX_ALLOWED_CLIENT];
extern int usd_head, usd_tail;
extern pthread_mutex_t buffer_list_mutex;
extern pthread_mutex_t users_lock;
extern pthread_cond_t buffer_list_cond;
extern int max_buffer_len;
extern int buffer_len;
extern int is_update;

#define min(a, b) ((a) < (b)) ? (a) : (b)
/*
void *comsume(void *arg) { 
   MYSQL *mysql = (MYSQL *)arg;
   user_send_data *buffer_data = NULL;
   pthread_mutex_lock(&buffer_list_mutex);
   char tmp[128] = {0};

   ////printf("usd_head = %d\tusd_tail = %d\n", usd_head, usd_tail);
   int len = min(ONCE_WRITE_LEN, usd_head - usd_tail) + 1;
   while (len < ONCE_WRITE_LEN) {
      len = min(ONCE_WRITE_LEN, usd_head - usd_tail) + 1;
      sprintf(tmp, "len = %d", len);
      wlog(DEBUG, tmp);
      //printf("data len < ONCE_WRITE_LEN is less so we should wait\n");
      pthread_cond_wait(&buffer_list_cond, &buffer_list_mutex);
      //printf("here we may check again .\n");
   }

   // then we read the data.
   //printf("in comsume len = %d\n", len);
   if (len > 0) {
      buffer_data = (user_send_data *)malloc(sizeof(user_send_data) * len);
      int i = 0;
      for (i = 0; i < len; ++i) {
         memcpy(&(buffer_data[i]), &(usd_buffer[usd_tail + i]), sizeof(user_send_data));
      }
      memset(tmp, 0, 128);
      for (i = 0; i < len; ++i) {
         sprintf(tmp, "buffer_data[%d].fd = %d\n", i, buffer_data[i].fd);
         wlog(INFO, tmp);
         sprintf(tmp, "buffer_data[%d].data = %s\n", i, buffer_data[i].data);
         wlog(INFO, tmp);
         //printf("buffer_data[%d].send_time.tv = %ld\n", i, buffer_data[i].send_time.tv_sec);
      } 
      usd_tail += len;
      if (usd_tail > usd_head) {
         usd_tail = 0;
         usd_head = -1;
      }
   } 
   pthread_mutex_unlock(&buffer_list_mutex);

   wlog(DEBUG, "here?");
   if (buffer_data) {
      //printf("in comsume write data\n");
      //excuteSql(mysql, "START TRANSACTION");
      memset(tmp, 0, 128);
      sprintf(tmp, "now we should write data to database");
      wlog(INFO, tmp);
      mysql_query(mysql, "START TRANSACTION");
      //printf("here bug?\n");
      int i = 0;
      char sql[1024] ;
      //pthread_mutex_lock(&users_lock);
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
      //pthread_mutex_unlock(&users_lock);
      //excuteSql(mysql, "COMMIT");
      mysql_query(mysql, "COMMIT");
      memset(tmp, 0, 128);
      sprintf(tmp, "data write success");
      wlog(INFO, tmp);
      // write to database.
   } else {
      printf("buffer data = NULL\n");
      wlog(ERROR, "buffer_data = NULL");
   }

   return (void *)0;
}
*/
void product() {
}

void *updateOnlineUsers(void *arg) {
   MainWindow *w = (MainWindow *)arg;
   while (1) {
       //printf("in updateOnlineUsers while\n");
      if (1) {
          ////printf("is_update = 1. here\n");
         w->sendUpdateMsg();
         //printf("sendUpdateMsg\n");
      }
      sleep(2);
   }
   return (void *)0;
}

void *updateOnlineNums(void *arg) {
  MainWindow *w = (MainWindow *)arg;
  while (1) {
      w->sendUpdateOnlineNumsMsg();
      sleep(2);
    }
}
