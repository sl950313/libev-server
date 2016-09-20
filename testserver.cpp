#include <stdio.h>
#include <ev.h> 
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <map>
#include <vector>
#include <set>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "muti_thread.h"
#include "_struct.h"
//#include "sql.h"
#include "macro.h"
#include "mainwindow.h"
#include "utils.h"

using namespace std;

/**
 * 00, 01-1F, 20-2F, 30-3F.FF
 */
struct user_fd_sign *users[MAX_ALLOWED_CLIENT] = {NULL};
int user_num = 0;

map<project_id_type, vector<set<fd_device_id_type > > > project_ids;

map<project_device_id_type, online_info> online_users;
set<project_device_id_type> forbidden_IDs;

//struct user_send_data *usd_buffer_head = NULL;
//struct user_send_data *usd_buffer_tail = NULL;
struct user_send_data usd_buffer[MAX_ALLOWED_CLIENT];
int usd_head = -1, usd_tail = 0;
int max_buffer_len = 1024;
int buffer_len = 0;
int is_update = 1;

pthread_mutex_t forbidden_IDs_lock;
pthread_mutex_t users_lock;
pthread_mutex_t online_users_lock;
pthread_mutex_t libevlist_lock;
pthread_mutex_t buffer_list_mutex;
pthread_cond_t buffer_list_cond;
//MYSQL *mysql = NULL;
MainWindow *w = NULL;
FILE *log_file_fp = NULL;
//user_fd_sign *project_ids[1024];

const char *server_log_file = "/home/server/server.log";
const int log_level = 3;

/**
 * total ev_io watcher.
 */
struct ev_io *libevlist[MAX_ALLOWED_CLIENT] = {NULL};
struct ev_loop *loop = EV_DEFAULT;

int freelibev(struct ev_loop *loop, int fd) {
   struct timeval end_time;
   gettimeofday(&end_time, NULL);
   pthread_mutex_lock(&libevlist_lock);
   if(libevlist[fd] == NULL)  {  
      printf("the fd already freed[%d]\n", fd);  
      return -1;  
   }  

   close(fd);  
   ev_io_stop(loop, libevlist[fd]);  
   free(libevlist[fd]);  
   libevlist[fd] = NULL;  
   pthread_mutex_unlock(&libevlist_lock);

   return 1; 
}

int updateToDatabase(int fd) {
   // update to database.TODO:
   return 0;
}

int freeOnlineUserMap(int fd) {
   project_device_id_type pd_id;
   pthread_mutex_lock(&users_lock);
   memcpy(&(pd_id.device_id), users[fd]->device_id, 8);
   memcpy(&(pd_id.project_id), users[fd]->project_id, 8);
   pthread_mutex_unlock(&users_lock);
   //pd_id.fd = fd;

   char tmp[128] = {0};
   pthread_mutex_lock(&online_users_lock);
   map<project_device_id_type, online_info>::iterator it = online_users.find(pd_id);
   if (it != online_users.end()) { 
      online_users.erase(it);
      pthread_mutex_unlock(&online_users_lock);
   } else {
       unsigned long long p_id, d_id;
       memcpy(&p_id, pd_id.project_id, 8);
       memcpy(&d_id, pd_id.device_id, 8);
      sprintf(tmp, "error in freeOnlineUserMap may happen. d_id = %llx,p_id = %llx", d_id, p_id);
      wlog(ERROR, tmp);
      pthread_mutex_unlock(&online_users_lock);
      return -1;
   }
   return 0;
}

int freeUserfdSign(int fd) {
   pthread_mutex_lock(&users_lock);
   free(users[fd]);
   users[fd] = NULL;
   pthread_mutex_unlock(&users_lock);
   user_num--;
   return 0;
}

int freeProjectMap(int fd) {
   project_id_type project_id;
   unsigned long long device_id_t;
   memset(project_id.project_id, 0, 8);
   fd_device_id_type fd_device_id;
   fd_device_id.fd = fd;
   pthread_mutex_lock(&users_lock);
   memcpy(fd_device_id.device_id, users[fd]->device_id, 8);
   memcpy(project_id.project_id, users[fd]->project_id, 8);
   pthread_mutex_unlock(&users_lock);
   memcpy(&device_id_t, fd_device_id.device_id, 8);

   map<project_id_type, vector<set<fd_device_id_type> > >::iterator it = project_ids.find(project_id);
   if (it == project_ids.end()) {
      printf("some error may happen in freeProjectMap\n");
      return -1;
   }
   size_t i = 0;
   //printf("should be del fd = %d, device_id = %llx\n", fd, device_id_t);
   /*
    * debug
    */

   for (i = 0; i < it->second.size(); ++i) {
      set<fd_device_id_type>::iterator ite = it->second[i].find(fd_device_id);
      memcpy(&device_id_t, ite->device_id, 8);
      //printf("after sest.find() i = %ld : ite->fd = %d, ite->device_id = %llx\n", i, ite->fd, device_id_t);
      if (ite != it->second[i].end()) {
         it->second[i].erase(ite);
         break;
      }
   }
   //printf("in freeProjectMap after del device\n");
   /*
   for (i = 0; i < it->second.size(); ++i) {
      set<fd_device_id_type>::iterator ite;
      for (ite = it->second[i].begin(); ite != it->second[i].end(); ++ite) {
         memcpy(&device_id_t, ite->device_id, 8);
         //printf("in freeProjectMap %ld set fd = %d, device_id: %llx\n", i, (*ite).fd, device_id_t);
      }
   }
   */
   return 0;
}

int getSigByDeviceId(char device_id[8]) {
   if (((device_id[0] & 0xFF) >= (0x01 & 0xFF)) && ((device_id[0] & 0xFF) <= (0x1F & 0xFF))) {
      return 0;
   }
   if (((device_id[0] & 0xFF) >= (0x20 & 0xFF)) && ((device_id[0] & 0xFF) <= (0xFF & 0x2F))) {
      return 1;
   }
   if (((device_id[0] & 0xFF) >= (0x30 & 0xFF)) && ((device_id[0] & 0xFF) <= (0xFF & 0x3F))) {
      return 2;
   }
   if ((device_id[0] * 0xFF) == 0x00) {
      return 3;
   }
   if ((device_id[0] & 0xFF) == 0xFF) {
      return 4;
   }
   return -1;
}

int *getTransmitFdsByrules(int fd, int *length) {
   // TODO:
   //printf("in getTransmitFdsByrules fd = %d\n", fd);
   int *res = (int *)malloc(sizeof(int) * MAX_TRANSMIT_USER);

   project_id_type project_id_t;
   memset(project_id_t.project_id, 0, 8);
   pthread_mutex_lock(&users_lock);
   memcpy(project_id_t.project_id, users[fd]->project_id, 8);
   int sig = getSigByDeviceId(users[fd]->device_id);
   pthread_mutex_unlock(&users_lock);
   map<project_id_type, vector<set<fd_device_id_type> > >::iterator it = project_ids.find(project_id_t);
   if (it == project_ids.end()) {
      printf("here should not be excute\n");
      return res;
   }
   //printf("in getTransmitFdsByrules sig = %d\n", sig);
   int send = -1;
   send = sig;
   if (sig == 0) send = 1;
   if (sig == 1) send = 0;
   if (sig == 2) send = 2;
   int len = 0; 
   //printf("in getTransmitFdsByrules send = %d\n", send);
   if (send < 3) {
      for (set<fd_device_id_type>::iterator ite = it->second[send].begin(); ite != it->second[send].end(); ++ite) {
         if ((*ite).fd != fd) res[len++] = (*ite).fd;
      }
   } else {
      if (send == 3) res[len++] = fd; 
      if (send == 4) {
         for (size_t i = 0; i < it->second.size(); ++i) {
            for (set<fd_device_id_type>::iterator ite = it->second[i].begin(); ite != it->second[i].end(); ++ite) {
               if ((*ite).fd != fd) res[len++] = (*ite).fd;
            } 
         }
      }
   }
   (*length) = len;
   //printf("*length = %d\n", *length);
   return res;
}

void read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
   char buffer[BUFFER_SIZE];  
   memset(buffer, 0, BUFFER_SIZE);
   ssize_t read;  
   char tmp[256];

   if(EV_ERROR & revents)  {  
      //printf("error event in read\n");
      memset(tmp, 0, 256);
      sprintf(tmp, "error evnt in read.errno = %s", strerror(errno));
      wlog(ERROR, tmp);
      return ;  
   }  
   read = recv(watcher->fd, buffer, BUFFER_SIZE, 0);  
   if(read < 0)  {  
      perror("read_cb");
      //printf("read error\n");
      memset(tmp, 0, 256);
      sprintf(tmp, "read_cb:read error. errno = %s", strerror(errno));
      wlog(ERROR, tmp);
      return;  
   }  

   unsigned long long device_id_tmp, project_id_tmp;
   if(read == 0)  {  
      //printf("client disconnected.\n");  
      /**
       * free user login info. and project info.@sl
       */

      memset(tmp, 0, 256);
      memcpy(&device_id_tmp, users[watcher->fd]->device_id, 8);
      memcpy(&project_id_tmp, users[watcher->fd]->project_id, 8);
      sprintf(tmp, "device id:%llx, project id:%llx, ip:%s leave the server", device_id_tmp, project_id_tmp, users[watcher->fd]->ip);
      w->sendMsg((string)tmp);
      wlog(INFO, tmp);
      updateToDatabase(watcher->fd);
      freeProjectMap(watcher->fd);
      freeOnlineUserMap(watcher->fd);
      freeUserfdSign(watcher->fd);
      freelibev(loop, watcher->fd);
      return;  
   }
   if ((read == 1) && ((buffer[0] & 0xFF) == 0x1)) {
       /*
        * here is a heart package.
        */
       wlog(INFO, "heart package here.");
       return ;
     }
   char info[BUFFER_SIZE * 2] = {0};
   char s_tmp[256];
   memset(s_tmp, 0, 256);
   memcpy(&device_id_tmp, users[watcher->fd]->device_id, 8);
   memcpy(&project_id_tmp, users[watcher->fd]->project_id, 8);
   //struct timeval tm_t;
   //gettimeofday(&tm_t, NULL);
   time_t timep = time(NULL);
   struct tm *p_tm = localtime(&timep);
   strToHex(buffer, read, info);
   //printf("info = %s\n", info);
   sprintf(s_tmp, "receive message:0x%s from device_id:%llx, project_id:%llx, ip:%s, time:%d%d%d-%d:%d:%d",  info, device_id_tmp, project_id_tmp, users[watcher->fd]->ip, (p_tm->tm_year + 1900), (p_tm->tm_mon + 1), p_tm->tm_mday, p_tm->tm_hour, p_tm->tm_min, p_tm->tm_sec);
   w->sendMsg((string)s_tmp);
   wlog(INFO, s_tmp);

   // transmit data to other divice depend on some rules.
   int len = 0;
   int *transmit_fds = getTransmitFdsByrules(watcher->fd, &len);
   //printf("fd = %d\tlen = %d\n", watcher->fd, len);
   for (int i = 0; i < len; ++i) {
      //printf("transmit_fds[%d] = %d\n", i, transmit_fds[i]);
   }
   // TODO
   /*
    * do product data to usd_buffer.
    */
   pthread_mutex_lock(&buffer_list_mutex);
   if (usd_head < max_buffer_len) { 
      usd_buffer[++usd_head].fd = watcher->fd;
      memcpy(usd_buffer[usd_head].project_id, &project_id_tmp, 8);
      memcpy(usd_buffer[usd_head].device_id, &device_id_tmp, 8);
      memcpy(usd_buffer[usd_head].data, buffer, read);
      usd_buffer[usd_head].tm = timep;
   }
   if (usd_head - usd_tail + 1 >= ONCE_WRITE_LEN) {
      //printf("send a signal to read thread\n");
      pthread_cond_signal(&buffer_list_cond);
   }

   pthread_mutex_unlock(&buffer_list_mutex);
   int i = 0;
   for (i = 0; i < len; ++i) {
      send(transmit_fds[i], buffer, read, 0);
   }
   bzero(buffer, read);  
}

int checkRegMsg(char *msg,int read) {
   //   if (strlen(msg) != REG_MSG_LEN) {
   //      return 1;
   //   }
   if (msg[0] != 0x3C) {
      return 1;
   }
   int i = 0;
   char sum = 0;
   for (i = 0; i < read - 1; ++i) {
      sum += msg[i];
   }
   return ((sum & 0xFF) == (0xFF & msg[read - 1])) ? 0 : 1;
}

int checkReduplicateID(char project_id[8], char device_id[8]) {
   project_id_type project_id_t;
   memcpy(&project_id_t, project_id, 8);
   map<project_id_type, vector<set<fd_device_id_type> > >::iterator it = project_ids.find(project_id_t);
   size_t i = 0;
   for (i = 0; i < it->second.size(); ++i) { 
      for (set<fd_device_id_type>::iterator ite = it->second[i].begin(); ite != it->second[i].end(); ++ite) {
         if (memcmp(ite->device_id, device_id, 8) == 0) {
            return 1;
         }
      }
   }
   return 0;
}

int isForbiddenID(char project_id[8], char device_id[8]) {
   //printf("in isForbiddenID\n");
   project_device_id_type pdt;
   memcpy(pdt.project_id, project_id, 8);
   memcpy(pdt.device_id, device_id, 8);
   //printf("in waiting for forbidden_IDs_lock\n");
   pthread_mutex_lock(&forbidden_IDs_lock);
   //printf("get forbidden_IDs_lock\n");
   if (forbidden_IDs.find(pdt) != forbidden_IDs.end()) {
      pthread_mutex_unlock(&forbidden_IDs_lock);
      return 1;
   }
   pthread_mutex_unlock(&forbidden_IDs_lock);
   return 0;
}

void comfirm_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
   char buffer[ID_SIZE];
   ssize_t read;
   char tmp[256] = {0};
   //int _register = 0;

   if(EV_ERROR & revents)  {
      memset(tmp, 0, 256);
      sprintf(tmp, "error event in read. errno = %s", strerror(errno));
      wlog(ERROR, tmp);
      freelibev(loop, watcher->fd);
      return ;  
   }  
   read = recv(watcher->fd, buffer, ID_SIZE, 0);  
   //printf("in comfirm _cb\n");
   if(read < 0)  {  
       memset(tmp, 0, 256);
      perror("comfirm_cb");
      sprintf(tmp, "read error. errno = %s", strerror(errno));
      wlog(ERROR, tmp);
      freelibev(loop, watcher->fd);
      return;  
   }  
   if(read == 0)  {
       memset(tmp, 0, 256);

      sprintf(tmp, "in comfirm_cb client disconnected. errno = %s", strerror(errno));
      wlog(ERROR, tmp);
      perror("comfirm_cb:read == 0");
      //close(watcher->fd);
      //ev_io_stop(EV_A_ watcher);
      freelibev(loop, watcher->fd);  
      return;  
   } 
   //printf("receive message:%s\n", buffer); 
   //printf("receive msg\n");
   //w->sendMsg("receive msg");
   ssize_t i = 0;
   for (i = 0; i < read; i++) {
      //printf("buffer[%ld] = %x\n", i, buffer[i]);
      char s_tmp[64];
      memset(s_tmp, 0, 64);
      sprintf(s_tmp, "buffer[%ld] = %x", i, buffer[i]);
      string s_t = s_tmp;
      //w->sendMsg(s_t);
   }
   unsigned long p_id = 0, d_id = 0;
   memcpy(&p_id, buffer, 8);
   memcpy(&d_id, buffer + 8, 8);
   char result[1] = {0x00};
   if (checkRegMsg(buffer, read)) {
      memset(tmp, 0, 256);
      sprintf(tmp, "check msg ID error.p_id = %llx, d_id = %llx. %s:%d", p_id, d_id, __FILE__, __LINE__);
      wlog(ERROR, tmp);
      w->sendMsg("check msg ID error");
      result[0] = 0x01;
      //send(watcher->fd, result, 1, 0);
      freelibev(loop, watcher->fd);
      return ;
   }
   
   //printf("ID is right\n");
   //w->sendMsg("ID is right");

   char project_id[8], device_id[8];
   char *data = NULL;
   //analysis(buffer, project_id, device_id, data);
   memcpy(project_id, buffer, 8);
   memcpy(device_id, buffer + 8, 8);
   data = buffer + 16;
   if (isForbiddenID(project_id, device_id)) {
      memset(tmp, 0, 128);
      sprintf(tmp, "id is forbidden by server, ID = %llx, %llx", p_id, d_id);
      w->sendMsg("id is forbidden by server");
      wlog(ERROR, "id is forbidden by server");
      char result[] = "ID is forbidden";
      //send(watcher->fd, result, strlen(result), 0);
      freelibev(loop, watcher->fd);
      return ;
   }
   int sig = getSigByDeviceId(device_id);
   if (sig < 0) {
      printf("ID error\n");
      printf("device_id = :");
      for (int k = 0; k < 8; ++k) {
         printf("%x ", device_id[k]);
      }
      printf("\n");
      result[0] = 0x01;
      //send(watcher->fd, result, 1, 0);
      freelibev(loop, watcher->fd);
      return ;
   }
   //printf("project_id = %s, device_id = %s\n", project_id, device_id);
   //printf("sig = %d\n", sig);

   /** TODO check the ID can be used or not.*/
   //send(watcher->fd, result, 1, 0);
   // TODO
   // check ID reduplicate or not.
   unsigned long long device_id_tmp;
   //printf("sizeof(unsigned long long) = %ld\n", sizeof(unsigned long long));
   memcpy(&device_id_tmp, device_id, 8);
   //char tmp[64] = {0};
   if (checkReduplicateID(project_id, device_id)) {
      memset(tmp, 0, 256);
      sprintf(tmp, "error. reduplicate ID. device_ID = %llx. project_id = %llx. %s:%d", device_id_tmp, p_id, __FILE__, __LINE__);
      wlog(ERROR, tmp);
      for (int i = 0; i < 8; i++) {
         //printf("device_id[%d] = %x\n", i, device_id[i]);
      }
      result[0] = 0x01;
      //send(watcher->fd, result, 1, 0);
      freelibev(loop, watcher->fd);
      return ; 
   }
   memset(tmp, 0, 256);
   sprintf(tmp, "not reduplicateID. ID = %llx. %s:%d", device_id_tmp, __FILE__, __LINE__);
   wlog(INFO, tmp);
   /*
   for (int i = 0; i < 8; i++) {
      printf("device_id[%d] = %x\n", i, device_id[i]);
   }
   */


   //ev_break(EV_A_ EVBREAK_ONE);
   int fd = watcher->fd;
   ev_io_stop(EV_A_ watcher);
   free(libevlist[fd]);
   libevlist[fd] = NULL;
   //freelibev(loop, watcher->fd);
   struct ev_io *data_transform = (struct ev_io *)malloc(sizeof(struct ev_io));
   if (!data_transform) {
      //printf("malloc error\n");
      wlog(ERROR, "malloc error");
      freelibev(loop, watcher->fd);
      return ;
   } 
   if(EV_ERROR & revents) {  
      freelibev(loop, watcher->fd);
      //printf("error event in accept\n");  
      wlog(ERROR, "error event in accept");
      return ;  
   }  
   struct sockaddr_in sa;
   socklen_t len;
   len = sizeof(sa);
   if (getpeername(watcher->fd, (struct sockaddr *)&sa, &len)) {
      //printf("error happen\n");
      memset(tmp, 0, 64);
      sprintf(tmp, "error in getpeername. %s:%d", __FILE__, __LINE__);
      wlog(ERROR, tmp);
      freelibev(loop, watcher->fd);
      return ;
   }
   pthread_mutex_lock(&users_lock);
   users[fd] = (struct user_fd_sign *)malloc(sizeof(user_fd_sign));
   char *ip = inet_ntoa(sa.sin_addr);
   //printf("ip = %s\nip.length = %ld\n", ip, strlen(ip));
   memset(users[fd]->ip, 0, 32);
   memcpy(users[fd]->ip, ip, strlen(ip));
   //printf("after memcpy, users[fd]->ip.length = %ld\n", strlen(users[fd]->ip));
   users[fd]->port = ntohs(sa.sin_port);
   memcpy(users[fd]->project_id ,project_id, 8);
   memcpy(users[fd]->device_id, device_id, 8);
   //gettimeofday(&(users[fd]->begin_time), NULL);
   pthread_mutex_unlock(&users_lock);

   user_num++;
   //printf("user_num = %d\n", user_num);
   //printf("users[%d].ip = %s\n", fd, users[fd]->ip);
   //printf("users[%d].ip.len = %ld\n", fd, strlen(users[fd]->ip));

   project_id_type project_id_tmp;
   memset(project_id_tmp.project_id, 0, 8);
   memcpy(project_id_tmp.project_id, project_id, 8);
   fd_device_id_type fd_device_id;
   memset(fd_device_id.device_id, 0, 8);
   fd_device_id.fd = fd;
   memcpy(fd_device_id.device_id, device_id, 8);
   //sleep(3);

   //printf("project_id:");

   //printf("project_id_tmp = %llx\n", project_id_tmp);
   map<project_id_type, vector<set<fd_device_id_type> > >::iterator it = project_ids.find(project_id_tmp);
   if (it == project_ids.end()) {
      vector<set<fd_device_id_type> > same_p_id(5);
      same_p_id[sig].insert(fd_device_id);
      project_ids.insert(pair<project_id_type, vector<set<fd_device_id_type> > >(project_id_tmp, same_p_id));
      //printf("create a new project, ID = \n");
      memset(tmp, 0, 64);
      unsigned long long project_id_num;
      memcpy(&project_id_num, project_id, 8);
      sprintf(tmp, "create a new project. ID = %llx", project_id_num);
      wlog(INFO, tmp);
      //printf("\n");
   } else {
      it->second[sig].insert(fd_device_id);
   }
   //printf("insert ok!\n");

   /**
    * insert into online_users.
    */
   project_device_id_type project_device_t;
   memcpy(&(project_device_t.device_id), device_id, 8);
   memcpy(&(project_device_t.project_id), project_id, 8);
   //project_device_t.fd = fd;

   //printf("waiting for online_users_lock\n");

   pthread_mutex_lock(&online_users_lock);
   //printf("get lock\n");
   map<project_device_id_type, online_info>::iterator ite = online_users.find(project_device_t);
   //printf("t1 = %llx, t2 = %llx\n", t1, t2);

   if (ite != online_users.end()) {
      unsigned long long t1, t2;
      memcpy(&t1, ite->first.project_id, 8);
      memcpy(&t2, ite->first.device_id, 8);
      //printf("ite->first.project_id = %llx, ite->first.device_id = %llx\n", t1, t2);
      //printf("duplicate ID!\n");
      memset(tmp, 0, 64); 
      sprintf(tmp, "and here should not be excute. %s:%d", __FILE__, __LINE__);
      wlog(ERROR, tmp);
      return ;
   }
   online_info oi;
   oi.tm = time(NULL);
   struct tm *timep = localtime(&(oi.tm));
   sprintf(oi.login_time, "%d%d%d-%d:%d:%d", (timep->tm_year + 1900), (timep->tm_mon + 1), timep->tm_mday, timep->tm_hour, timep->tm_min, timep->tm_sec);
   memcpy(oi.ip, ip, 32);
   online_users.insert(pair<project_device_id_type, online_info>(project_device_t, oi));
   pthread_mutex_unlock(&online_users_lock);

   char str_tmp[128];
   memset(str_tmp, 0, 128);
   unsigned long long project_id_t;
   memcpy(&device_id_tmp, fd_device_id.device_id, 8);
   memcpy(&project_id_t, project_id_tmp.project_id, 8);
   sprintf(str_tmp, "userID:%llx, projectID:%llx from %s time:%s enter service", device_id_tmp, project_id_t, oi.ip, oi.login_time);
   //printf("str_tmp = %s\n", str_tmp);

   w->sendMsg((string)str_tmp);
   wlog(INFO, str_tmp);

   pthread_mutex_lock(&libevlist_lock);
   libevlist[fd] = data_transform;
   ev_io_init(data_transform, read_cb, fd, EV_READ);
   ev_io_start(loop, data_transform);
   pthread_mutex_unlock(&libevlist_lock);
}

void accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
   struct sockaddr_in client_addr;  
   socklen_t client_len = sizeof(client_addr);  
   int client_sd;  
   struct ev_io *w_client = (struct ev_io*) malloc(sizeof(struct ev_io));
   char tmp[64] = {0};

   if(w_client == NULL) {  
      printf("malloc error in accept_cb\n");  
      w->sendMsg("malloc error in accept_cb");
      wlog(ERROR, "malloc error in accept_cb");
      return ;  
   }  

   if(EV_ERROR & revents) {  
      printf("error event in accept\n");  
      w->sendMsg("error event in accept");
      wlog(ERROR, "error event in accept");
      return ;  
   }  
   client_sd = accept(watcher->fd, (struct sockaddr*)&client_addr, &client_len);  
   if(client_sd < 0)  {  
       memset(tmp, 0, 256);
      sprintf(tmp, "accept error. errno = %s", strerror(errno));
      perror("accept_error:");
      w->sendMsg("accept error");
      wlog(ERROR, tmp);
      free(w_client);
      return;  
   } 
   if( client_sd > MAX_ALLOWED_CLIENT)  {  
      memset(tmp, 0, 64);
      sprintf(tmp, "fd too large[%d]. %s:%d", client_sd, __FILE__, __LINE__);  
      w->sendMsg(tmp);
      wlog(ERROR, tmp);
      close(client_sd);  
      free(w_client);
      return ;  
   }  
   //printf("accept request success\n");
   //w->sendMsg("accept request success");

   if(libevlist[client_sd] != NULL)  {  
      memset(tmp, 0, 64);
      sprintf(tmp, "client_sd not NULL fd is [%d]. %s:%d", client_sd, __FILE__, __LINE__);  
      w->sendMsg(tmp);
      wlog(ERROR, tmp);
      //printf("here should not be excute\n");
      //free(w_client);
      close(client_sd);
      return ;  
   }  
   //printf("client connected\n");
   //w->sendMsg("client connected");
   // send comfirm msg to clinet.

   char msg[6] = {0x7B, 0x70, 0x69, 0x6E, 0x67, 0x7D}; // {ping}.
   send(client_sd, msg, 6, 0);
   ev_io_init(w_client, comfirm_cb, client_sd, EV_READ);
   ev_io_start(loop, w_client);  

   libevlist[client_sd] = w_client;
}

int initServer(int &sd) {
   struct sockaddr_in addr;  
   char tmp[64] = {0};
   //int addr_len = sizeof(addr);
   sd = socket(PF_INET, SOCK_STREAM, 0);
   if(sd < 0)  {  
      memset(tmp, 0, 64); 
      sprintf(tmp, "socket error. %s:%d", __FILE__, __LINE__);  
      w->sendMsg("socket error");
      wlog(ERROR, tmp);
      return -1;  
   } 
   //printf("socket success\n");
   bzero(&addr, sizeof(addr));  
   addr.sin_family = AF_INET;  
   addr.sin_port = htons(PORT);  
   addr.sin_addr.s_addr = INADDR_ANY;  
   if(bind(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0)  {  
      printf("bind error\n");  
      return -1;  
   }  
   memset(tmp, 0, 64);
   sprintf(tmp, "bind success. %s:%d", __FILE__, __LINE__);
   wlog(INFO, tmp);
   
   //printf("SOMAXCONN = %d\n", SOMAXCONN);
   //printf("bind success\n");
   if(listen(sd, SOMAXCONN) < 0)  {  
      memset(tmp, 0, 64);
      sprintf(tmp, "listen error. %s:%d", __FILE__, __LINE__);  
      wlog(ERROR, tmp);
      return -1;  
   } 

   // some problem TODO:
   int bReuseaddr=1;  
   if(setsockopt(sd,SOL_SOCKET ,SO_REUSEADDR,(const char*)&bReuseaddr,sizeof(bReuseaddr)) != 0)  {  
      printf("setsockopt error in reuseaddr[%d]\n", sd);  
      return -1;  
   }  
   //printf("Reuse success\n");
   return 0;
}

void *init_server(void *arg) {
   w = (MainWindow *)arg;
   int ret = 0;
   int sd = 0;
   if ((log_file_fp = fopen(server_log_file, "a+")) == NULL) {
      w->sendMsg("error open log file.");
      printf("log file open error\n");
      return (void *)-1;
   }
   if ((ret = initServer(sd)) == -1) {
      //printf("error happen in initServer()\n");
      wlog(ERROR, "error happen in initServer()");
      w->sendMsg("error happen in initServer()");
      fclose(log_file_fp);
      return (void *)-1;
   }
   /**
    * some initial work.
    */
   const char *ip_string = "114.214.169.173";
   const char *user_name = "root";
   const char *password = "openstack210";
   const char *database_name = "test";
/*
   mysql = initSql(ip_string, user_name, password, database_name);
   if (mysql == NULL) {
      wlog(ERROR, "error may happen in mysql");
      w->sendMsg("error may happen in mysql");
      fclose(log_file_fp);
      return (void *)-1;
   }
*/

   pthread_mutex_init(&forbidden_IDs_lock, NULL);
   pthread_mutex_init(&users_lock, NULL);
   pthread_mutex_init(&online_users_lock, NULL);
   pthread_mutex_init(&buffer_list_mutex, NULL);
   pthread_mutex_init(&libevlist_lock, NULL);
   pthread_cond_init(&buffer_list_cond, NULL);
   pthread_t comsume_id, update_id;
   //pthread_create(&comsume_id, NULL, comsume, (void *)mysql);
   //pthread_create(&update_id, NULL, updateOnlineUsers, (void *)0);

   pthread_mutex_lock(&libevlist_lock);
   libevlist[sd] = (ev_io *)malloc(sizeof(ev_io));
   if (libevlist[sd] == NULL) {
      wlog(ERROR, "malloc error");
      w->sendMsg("malloc error");
      fclose(log_file_fp);
      return (void *) -1;
   }

   //ev_timer timeout_watcher;
   ev_io_init(libevlist[sd], accept_cb, sd, EV_READ);
   ev_io_start(loop, libevlist[sd]);
   pthread_mutex_unlock(&libevlist_lock);

   wlog(INFO, "loop running");
   w->sendMsg("loop running");
   ev_run(loop,0);
   return (void *)0;
}

/*
   int main() {
   int ret = init();
   if (ret == -1) {
   printf("error in init()\n");
   return -1;
   }
   }
 */
