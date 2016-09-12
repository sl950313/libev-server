#include <stdio.h>
#include <ev.h> //ev库头文件
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
#include "sql.h"
#include "macro.h"

using namespace std;

/**
 * 00, 01-1F, 20-2F, 30-3F.FF
 */
struct user_fd_sign *users[MAX_ALLOWED_CLIENT] = {NULL};
int user_num = 0;

map<project_id_type, vector<set<fd_device_id_type > > > project_ids;

//struct user_send_data *usd_buffer_head = NULL;
//struct user_send_data *usd_buffer_tail = NULL;
struct user_send_data usd_buffer[1024];
int usd_head = -1, usd_tail = 0;
int max_buffer_len = 1024;
int buffer_len = 0;

pthread_mutex_t buffer_list_mutex;
pthread_cond_t buffer_list_cond;
MYSQL *mysql = NULL;
//user_fd_sign *project_ids[1024];

/**
 * total ev_io watcher.
 */
struct ev_io *libevlist[MAX_ALLOWED_CLIENT] = {NULL};

static void timeout_cb(EV_P_ ev_timer *w,int revents) {
   puts("timeout");
   ev_break(EV_A_ EVBREAK_ONE);
}

int freelibev(struct ev_loop *loop, int fd) {
   struct timeval end_time;
   gettimeofday(&end_time, NULL);
   if(libevlist[fd] == NULL)  {  
      printf("the fd already freed[%d]\n", fd);  
      return -1;  
   }  

   close(fd);  
   ev_io_stop(loop, libevlist[fd]);  
   free(libevlist[fd]);  
   libevlist[fd] = NULL;  

   return 1; 
}

int updateToDatabase(int fd) {
   // update to database.TODO:
   return 0;
}

int freeUserfdSign(int fd) {
   free(users[fd]);
   users[fd] = NULL;
   user_num--;
   return 0;
}

int freeProjectMap(int fd) {
   project_id_type project_id;
   memset(project_id.project_id, 0, 8);
   memcpy(project_id.project_id, users[fd]->project_id, 8);
   map<project_id_type, vector<set<fd_device_id_type> > >::iterator it = project_ids.find(project_id);
   if (it == project_ids.end()) {
      printf("some error may happen in freeProjectMap\n");
      return -1;
   }
   size_t i = 0;
   fd_device_id_type fd_device_id;
   fd_device_id.fd = fd;
   memcpy(fd_device_id.device_id, users[fd]->device_id, 8);
   unsigned long long device_id_t;
   memcpy(&device_id_t, users[fd]->device_id, 8);
   printf("should be del fd = %d, device_id = %llx\n", fd, device_id_t);
   /*
    * debug
    */
   printf("in freeProjectMap fd = %d\n", fd);
   for (i = 0; i < it->second.size(); ++i) {
      set<fd_device_id_type>::iterator ite;
      for (ite = it->second[i].begin(); ite != it->second[i].end(); ++ite) {
         memcpy(&device_id_t, ite->device_id, 8);
         printf("in freeProjectMap %ld set fd = %d, device_id: %llx\n", i, (*ite).fd, device_id_t);
      }
   }
   for (i = 0; i < it->second.size(); ++i) {
      set<fd_device_id_type>::iterator ite = it->second[i].find(fd_device_id);
      memcpy(&device_id_t, ite->device_id, 8);
      printf("after sest.find() i = %ld : ite->fd = %d, ite->device_id = %llx\n", i, ite->fd, device_id_t);
      if (ite != it->second[i].end()) {
         it->second[i].erase(ite);
         break;
      }
   }
   printf("in freeProjectMap after del device\n");
   for (i = 0; i < it->second.size(); ++i) {
      set<fd_device_id_type>::iterator ite;
      for (ite = it->second[i].begin(); ite != it->second[i].end(); ++ite) {
         memcpy(&device_id_t, ite->device_id, 8);
         printf("in freeProjectMap %ld set fd = %d, device_id: %llx\n", i, (*ite).fd, device_id_t);
      }
   }
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
   printf("in getTransmitFdsByrules fd = %d\n", fd);
   int *res = (int *)malloc(sizeof(int) * 1024);
   int i = 0; 

   project_id_type project_id_t;
   memset(project_id_t.project_id, 0, 8);
   memcpy(project_id_t.project_id, users[fd]->project_id, 8);
   map<project_id_type, vector<set<fd_device_id_type> > >::iterator it = project_ids.find(project_id_t);
   if (it == project_ids.end()) {
      printf("here should not be excute\n");
      return res;
   }
   int sig = getSigByDeviceId(users[fd]->device_id);
   printf("in getTransmitFdsByrules sig = %d\n", sig);
   int send = -1;
   send = sig;
   if (sig == 0) send = 1;
   if (sig == 1) send = 0;
   if (sig == 2) send = 2;
   printf("in getTransmitFdsByrules send = %d\n", send);
   if (send < 3) {
      for (set<fd_device_id_type>::iterator ite = it->second[send].begin(); ite != it->second[send].end(); ++ite) {
         if ((*ite).fd != fd) res[i++] = (*ite).fd;
      }
   } else {
      if (send == 3) res[i++] = fd; 
      if (send == 4) {
         for (size_t i = 0; i < it->second.size(); ++i) {
            for (set<fd_device_id_type>::iterator ite = it->second[send].begin(); ite != it->second[send].end(); ++ite) {
               if ((*ite).fd != fd) res[i++] = (*ite).fd;
            } 
         }
      }
   }
   (*length) = i;
   printf("*length = %d\n", *length);
   return res;
}

void read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
   char buffer[BUFFER_SIZE];  
   ssize_t read;  

   if(EV_ERROR & revents)  {  
      printf("error event in read\n");  
      return ;  
   }  
   read = recv(watcher->fd, buffer, BUFFER_SIZE, 0);  
   if(read < 0)  {  
      printf("read error\n");  
      return;  
   }  
   if(read == 0)  {  
      printf("client disconnected.\n");  
      /**
       * free user login info. and project info.@sl
       */
      updateToDatabase(watcher->fd);
      freeProjectMap(watcher->fd);
      freeUserfdSign(watcher->fd);
      freelibev(loop, watcher->fd);  
      return;  
   }
   printf("in read_cb receive message:%s\n", buffer); 

   /*
   char project_id[8], device_id[8];
   char *data = NULL;
   //analysis(buffer, project_id, device_id, data);
   memcpy(project_id, buffer, 8);
   memcpy(device_id, buffer + 8, 8);
   */
   char *data = buffer;
   /*
   printf("project_id = ");
   for (int i = 0; i < 8; ++i) {
      printf("%x ", project_id[i]);
   }
   printf("\n");
   printf("device_id = ");
   for (int i = 0; i < 8; ++i) {
      printf("%x ", device_id[i]);
   }
   printf("\n");
   */
   printf("data = %s\n", data);
   //printf("project_id = %s, device_id = %s, data = %s\n", project_id, device_id, data);
   // write data to database.
   // transmit data to other divice depend on some rules.
   int len = 0;
   int *transmit_fds = getTransmitFdsByrules(watcher->fd, &len);
   printf("fd = %d\tlen = %d\n", watcher->fd, len);
   for (int i = 0; i < len; ++i) {
      printf("transmit_fds[%d] = %d\n", i, transmit_fds[i]);
   }
   // TODO
   /*
    * do product data to usd_buffer.
    */
   pthread_mutex_lock(&buffer_list_mutex);
   if (usd_head < max_buffer_len) { 
      usd_buffer[++usd_head].fd = watcher->fd;
      memcpy(usd_buffer[usd_head].data, data, strlen(data));
   }
   if (usd_head - usd_tail + 1 >= ONCE_WRITE_LEN) {
      pthread_cond_signal(&buffer_list_cond);
   }
   printf("send a signal to read thread\n");

   pthread_mutex_unlock(&buffer_list_mutex);
   int i = 0;
   for (i = 0; i < len; ++i) {
      send(transmit_fds[i], data, strlen(data), 0);
   }
   bzero(buffer, read);  
}

int checkRegMsg(char *msg) {
   //   if (strlen(msg) != REG_MSG_LEN) {
   //      return 1;
   //   }
   if (msg[0] != 0x3C) {
      return 1;
   }
   int i = 0;
   char sum = 0;
   for (i = 0; i < REG_MSG_LEN - 1; ++i) {
      sum += msg[i];
   }
   return ((sum & 0xFF) == (0xFF & msg[REG_MSG_LEN - 1])) ? 0 : 1;
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


void comfirm_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
   char buffer[ID_SIZE];
   ssize_t read;
   int _register = 0;

   if(EV_ERROR & revents)  {  
      printf("error event in read\n");  
      freelibev(loop, watcher->fd);  
      return ;  
   }  
   read = recv(watcher->fd, buffer, BUFFER_SIZE, 0);  
   if(read < 0)  {  
      printf("read error\n");  
      freelibev(loop, watcher->fd);  
      return;  
   }  
   if(read == 0)  {  
      printf("client disconnected.\n");  
      //close(watcher->fd);
      //ev_io_stop(EV_A_ watcher);
      freelibev(loop, watcher->fd);  
      return;  
   } 
   //printf("receive message:%s\n", buffer); 
   printf("receive msg\n");
   ssize_t i = 0;
   for (i = 0; i < read; i++) {
      printf("buffer[%ld] = %x\n", i, buffer[i]);
   }
   char result[1] = {0x00};
   if (checkRegMsg(buffer)) {
      printf("check msg ID error\n");
      result[0] = 0x01;
      send(watcher->fd, result, 1, 0);
      freelibev(loop, watcher->fd);
      return ;
   }
   printf("ID is right\n");

   char project_id[8], device_id[8];
   char *data = NULL;
   //analysis(buffer, project_id, device_id, data);
   memcpy(project_id, buffer, 8);
   memcpy(device_id, buffer + 8, 8);
   data = buffer + 16;
   int sig = getSigByDeviceId(device_id);
   if (sig < 0) {
      printf("ID error\n");
      result[0] = 0x01;
      send(watcher->fd, result, 1, 0);
      freelibev(loop, watcher->fd);
      return ;
   }
   //printf("project_id = %s, device_id = %s\n", project_id, device_id);
   printf("sig = %d\n", sig);

   /** TODO check the ID can be used or not.*/
   send(watcher->fd, result, 1, 0);
   // TODO
   // check ID reduplicate or not.
   unsigned long long device_id_tmp;
   printf("sizeof(unsigned long long) = %ld\n", sizeof(unsigned long long));
   memcpy(&device_id_tmp, device_id, 8);
   if (checkReduplicateID(project_id, device_id)) {
      printf("error. reduplicate ID. ID = %llx\n", device_id_tmp);
      for (int i = 0; i < 8; i++) {
         printf("device_id[%d] = %x\n", i, device_id[i]);
      }
      result[0] = 0x01;
      send(watcher->fd, result, 1, 0);
      freelibev(loop, watcher->fd);
      return ; 
   }
   printf("not reduplicateID. ID = %llx\n", device_id_tmp);
   for (int i = 0; i < 8; i++) {
      printf("device_id[%d] = %x\n", i, device_id[i]);
   }

   //ev_break(EV_A_ EVBREAK_ONE);
   int fd = watcher->fd;
   ev_io_stop(EV_A_ watcher);
   free(libevlist[fd]);
   libevlist[fd] = NULL;
   //freelibev(loop, watcher->fd);
   struct ev_io *data_transform = (struct ev_io *)malloc(sizeof(struct ev_io));
   if (!data_transform) {
      printf("malloc error\n");
      freelibev(loop, watcher->fd);
      return ;
   } 
   if(EV_ERROR & revents) {  
      freelibev(loop, watcher->fd);
      printf("error event in accept\n");  
      return ;  
   }  
   struct sockaddr_in sa;
   socklen_t len;
   len = sizeof(sa);
   if (getpeername(watcher->fd, (struct sockaddr *)&sa, &len)) {
      printf("error happen\n");
      freelibev(loop, watcher->fd);
      return ;
   }
   users[fd] = (struct user_fd_sign *)malloc(sizeof(user_fd_sign));
   char *ip = inet_ntoa(sa.sin_addr);
   memcpy(users[fd]->ip, ip, strlen(ip));
   users[fd]->port = ntohs(sa.sin_port);
   memcpy(users[fd]->project_id ,project_id, 8);
   memcpy(users[fd]->device_id, device_id, 8);
   gettimeofday(&(users[fd]->begin_time), NULL);
   user_num++;
   printf("user_num = %d\n", user_num);
   printf("users[%d].ip = %s\n", fd, users[fd]->ip);

   project_id_type project_id_tmp;
   memcpy(project_id_tmp.project_id, project_id, 8);
   fd_device_id_type fd_device_id;
   fd_device_id.fd = fd;
   memcpy(fd_device_id.device_id, device_id, 8);
   map<project_id_type, vector<set<fd_device_id_type> > >::iterator it = project_ids.find(project_id_tmp);
   if (it == project_ids.end()) {
      vector<set<fd_device_id_type> > same_p_id(3);
      same_p_id[sig].insert(fd_device_id);
      project_ids.insert(pair<project_id_type, vector<set<fd_device_id_type> > >(project_id_tmp, same_p_id));
      printf("create a new project, ID = \n");
      for (int i = 0; i < 8; ++i) {
         printf("%x ", project_id_tmp.project_id[i]);
      }
      printf("\n");
   } else {
      it->second[sig].insert(fd_device_id);
   }
   printf("insert ok!\n");

   libevlist[fd] = data_transform;
   ev_io_init(data_transform, read_cb, fd, EV_READ);
   ev_io_start(loop, data_transform);
}

void accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
   struct sockaddr_in client_addr;  
   socklen_t client_len = sizeof(client_addr);  
   int client_sd;  
   struct ev_io *w_client = (struct ev_io*) malloc(sizeof(struct ev_io));

   if(w_client == NULL) {  
      printf("malloc error in accept_cb\n");  
      return ;  
   }  

   if(EV_ERROR & revents) {  
      printf("error event in accept\n");  
      return ;  
   }  
   client_sd = accept(watcher->fd, (struct sockaddr*)&client_addr, &client_len);  
   if(client_sd < 0)  {  
      printf("accept error\n");  
      free(w_client);
      return;  
   } 
   if( client_sd > MAX_ALLOWED_CLIENT)  {  
      printf("fd too large[%d]\n", client_sd);  
      close(client_sd);  
      free(w_client);
      return ;  
   }  
   printf("accept request success\n");

   if(libevlist[client_sd] != NULL)  {  
      printf("client_sd not NULL fd is [%d]\n", client_sd);  
      //free(w_client);
      close(client_sd);
      return ;  
   }  
   printf("client connected\n");
   // send comfirm msg to clinet.

   char msg[8] = "comfirm";
   send(client_sd, msg, 8, 0);
   //ev_io_stop(EV_A_ watcher);
   //libevlist[client_sd] = (ev_io *)malloc(sizeof(ev_io));
   ev_io_init(w_client, comfirm_cb, client_sd, EV_READ);
   //ev_io_init(w_client, read_cb, client_sd, EV_READ);  
   ev_io_start(loop, w_client);  

   libevlist[client_sd] = w_client;
}

int main(int argc,char **args) {
   int sd;  
   struct sockaddr_in addr;  
   //int addr_len = sizeof(addr);
   sd = socket(PF_INET, SOCK_STREAM, 0);
   if(sd < 0)  {  
      printf("socket error\n");  
      return -1;  
   } 
   printf("socket success\n");
   bzero(&addr, sizeof(addr));  
   addr.sin_family = AF_INET;  
   addr.sin_port = htons(PORT);  
   addr.sin_addr.s_addr = INADDR_ANY;  
   if(bind(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0)  {  
      printf("bind error\n");  
      return -1;  
   }  
   printf("bind success\n");
   printf("SOMAXCONN = %d\n", SOMAXCONN);
   //printf("bind success\n");
   if(listen(sd, SOMAXCONN) < 0)  {  
      printf("listen error\n");  
      return -1;  
   } 

   // some problem TODO:
   int bReuseaddr=1;  
   if(setsockopt(sd,SOL_SOCKET ,SO_REUSEADDR,(const char*)&bReuseaddr,sizeof(bReuseaddr)) != 0)  {  
      printf("setsockopt error in reuseaddr[%d]\n", sd);  
      return -1;  
   }  
   printf("Reuse success\n");

   /**
    * some initial work.
    */
   const char *ip_string = "114.214.169.173";
   const char *user_name = "root";
   const char *password = "openstack210";
   const char *database_name = "test";
   mysql = initSql(ip_string, user_name, password, database_name);
   if (mysql == NULL) {
      printf("error may happen in mysql\n");
      return -1;
   }
   pthread_mutex_init(&buffer_list_mutex, NULL);
   pthread_cond_init(&buffer_list_cond, NULL);
   pthread_t comsume_id;
   pthread_create(&comsume_id, NULL, comsume, (void *)mysql);

   libevlist[sd] = (ev_io *)malloc(sizeof(ev_io));
   if (libevlist[sd] == NULL) {
      printf("malloc error\n");
      return -1;
   }

   //ev_timer timeout_watcher;
   struct ev_loop *loop = EV_DEFAULT;
   ev_io_init(libevlist[sd], accept_cb, sd, EV_READ);
   ev_io_start(loop, libevlist[sd]);


   /*
      ev_timer_init(&timeout_watcher,timeout_cb,5.5,0);
      ev_timer_start(loop,&timeout_watcher);
    */

   printf("loop running\n");
   ev_run(loop,0);
   return 0;
}
