#include <netinet/in.h>    // for sockaddr_in
#include <sys/types.h>    // for socket
#include <sys/socket.h>    // for socket
#include <stdio.h>        // for printf
#include <stdlib.h>        // for exit
#include <string.h>        // for bzero
#include<netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

int client_num[5] = {1000, 1000, 1000, 1000, 1000}; // ref to 0,1,2,3,4.
pthread_t *cl_p[5];
//cha//r project_id[8] = {};
char project_id[8] = {0x3C, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
char sig[5] = {0x00, 0x01, 0x20, 0x30, 0xFF};
char ip[32] = "205.209.163.153";

void *_read(void *arg) {
   sleep(1);
   //printf("in thread _read. thread_id = %ld\n", pthread_self());
   int sock_fd = (int )arg;
   char buffer[1024];
   while (1) {
      //printf("in recv loop\n");
      int len = recv(sock_fd, buffer, 1024, 0);
      if (len == 0) {
         printf("sock_fd closed\n");
         close(sock_fd);
      }
      buffer[len] = 0x00;
      //printf("recv len = %d\n", len);
      //printf("recv data = %s\n", buffer);
      //sleep(1);
      usleep(10000);
   }
   return (void *)0;
}

void *connect_rw(void *arg) {
   char *ID = (char *)arg;
   //printf("in connect_thread. thread_id = %ld\n", pthread_self());
   int i = 0 ;
   for (i = 0; i < 16; ++i){
      //printf("%x\t", ID[i]);
   }
   //printf("\n");
   char project_id[8], device_id[8];
   memcpy(project_id, ID, 8);
   memcpy(device_id, ID + 8, 8);

   struct sockaddr_in client_addr;
   bzero(&client_addr,sizeof(client_addr)); //把一段内存区的内容全部设置为0
   client_addr.sin_family = AF_INET;    //internet协议族
   client_addr.sin_addr.s_addr = htons(INADDR_ANY);//INADDR_ANY表示自动获取本机地址
   client_addr.sin_port = htons(0);

   int client_socket = socket(AF_INET,SOCK_STREAM,0);
   if( client_socket < 0) {
      printf("Create Socket Failed!\n");
      exit(1);
   }

   struct sockaddr_in dest_addr;
   //int destport = atoi(argv[2]);
   int destport = 8081;
   dest_addr.sin_family = AF_INET;
   dest_addr.sin_port = htons(destport);
   dest_addr.sin_addr.s_addr = inet_addr(ip);

   memset(&dest_addr.sin_zero,0,8);
   if(-1 == connect(client_socket,(struct sockaddr*)&dest_addr,sizeof(struct sockaddr))) {
      perror("connect error\n");
      exit(0);
   }
   int bReuseaddr=1;  
   if(setsockopt(client_socket, SOL_SOCKET, SO_REUSEADDR,(const char*)&bReuseaddr,sizeof(bReuseaddr)) != 0)  {  
      printf("setsockopt error in reuseaddr[%d]\n", client_socket);  
      return (void *)-1;  
   }  
   //printf("Reuse success\n");


   char buffer[16];
   //int len = 0;
   int len = recv(client_socket, buffer, 16, 0);
   //printf("recv msg = %s, len = %d\n", buffer, len);

   if (strcmp(buffer, "comfirm") == 0) { 
      int send_len = send(client_socket, ID, 16, 0);
      //printf("send len = %d \n", send_len);
      memset(buffer, 0, 16);
      int result = recv(client_socket, buffer, 16, 0);
      //printf("len in result = %d\n", result);
      char tmp = 0x01;
      if (buffer[0] == tmp) {
         printf("fail by server. Error : wrong ID\n");
         return 0;
      }
      pthread_t id1;
      pthread_create(&id1, NULL, _read, (void *)client_socket);

      char data[32] = "hello_world";
      while (1) {
         //printf("in send data loop\n");
         //int nsend = send(client_socket, data, strlen(data), 0);
         //printf("nsend = %d\n", nsend);
         sleep(5);
      }
   }
   close(client_socket);

   return (void *)0;
}

void check(char *ID) {
   char sum = 0;
   int i = 0;
   for (i = 0; i < 15; ++i) {
      sum += ID[i];
   }
   ID[15] = sum;
}

int main() {
   int i = 0, j = 0;
   for (i = 0; i < 5; ++i) {
      cl_p[i] = (pthread_t *)malloc(sizeof(pthread_t) * client_num[i]); 
   }

   sigset_t signal_mask;
   sigemptyset (&signal_mask);
   sigaddset (&signal_mask, SIGPIPE);
   int rc = pthread_sigmask (SIG_BLOCK, &signal_mask, NULL);
   if (rc != 0) {
      printf("block sigpipe error\n");
   }

   char ID[16];
   unsigned long long device_id = 1000000009649;
   memcpy(ID, project_id, 8);
   for (i = 0; i < 5; ++i) {
      for (j = 0; j < client_num[i]; ++j) {
         memcpy(ID + 8, &device_id, 8);
         ID[8] = sig[i];
         check(ID);
         pthread_create(&(cl_p[i][j]), NULL, connect_rw, (void *)ID);
         device_id += 1000000009651;
         usleep(10000);
      }
   }

   void *ret;
   for (i = 0; i < 5; ++i) {
      for (j = 0; j < client_num[i]; ++j) {
         pthread_join(cl_p[i][j], &ret);
      }
   }
   return 0;
}
