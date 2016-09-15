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
#include "_struct.h"

#define SIG0 10
#define SIG1 20
#define SIG2 20
#define SIG3 20
#define SIG4 10

int argc;
char **argv;

int sig0 = 10;
int sig1 = 20;
int sig2 = 20; 

void *_read(void *arg) {
   printf("in thread _read\n");
   int *sock_fd = (int *)arg;
   char buffer[256];
   while (1) {
      printf("in recv loop\n");
      int len = recv(*sock_fd, buffer, 256, 0);
      buffer[len] = 0x00;
      printf("recv len = %d\n", len);
      printf("recv data = %s\n", buffer);
      sleep(1);
   }
   return (void *)0;
}

void *test(void *arg) {
   project_device_id_type *pdt = (project_device_id_type *)arg;
   char project_id[8], device_id[8];
   memcpy(project_id, pdt->project_id, 8);
   memcpy(device_id, pdt->device_id, 8);
   if(argc != 3) {
      printf("usage:socket_client ipaddress port\n eg:./socket_client 192.168.1.158 5555");
      return (void *)-1;
   }
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
   int destport = atoi(argv[2]);
   dest_addr.sin_family = AF_INET;
   dest_addr.sin_port = htons(destport);
   dest_addr.sin_addr.s_addr = inet_addr(argv[1]);

   memset(&dest_addr.sin_zero,0,8);
   if(-1 == connect(client_socket,(struct sockaddr*)&dest_addr,sizeof(struct sockaddr))) {
      perror("connect error\n");
      exit(0);
   }


   /*
   int bReuseaddr=1;  
   if(setsockopt(client_socket, SOL_SOCKET, SO_REUSEADDR,(const char*)&bReuseaddr,sizeof(bReuseaddr)) != 0)  {  
      printf("setsockopt error in reuseaddr[%d]\n", client_socket);  
      return (void *)-1;  
   }  
   printf("Reuse success\n");
   */

   char buffer[16];
   //int len = 0;
   int len = recv(client_socket, buffer, 16, 0);
   printf("recv msg = %s, len = %d\n", buffer, len);
   /*
   char project_id[8] = {0x3C, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
   char device_id[8] = {0x01, 0x01, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x9D};
   char device_id_t[8] = {0x00, 0x01, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x9C};
   */
   char msg[16];
   memcpy(msg, project_id, 8);
   memcpy(msg + 8, device_id, 8);
   if (strcmp(buffer, "comfirm") == 0) { 
      int send_len = send(client_socket, msg, 16, 0);
      printf("send len = %d \n", send_len);

      memset(buffer, 0, 16);
      int result = recv(client_socket, buffer, 1, 0);
      printf("len in result = %d\n", result);
      char tmp = 0x01;
      if (buffer[0] == tmp) {
         printf("fail by server. Error : wrong ID\n");
         close(client_socket);
         return 0;
      }
      if (!strcmp(buffer, "ID is forbidden")) {
         printf("ID is forbidden");
         close(client_socket);
         return 0;
      }
      pthread_t id1;
      pthread_create(&id1, NULL, _read, (void *)&client_socket);

      char data[256];
      char true_data[8] = "hello";
      while (1) {
         //printf("in send data loop\n");
         memset(data, 0, 256);
         /*
         memcpy(data, project_id, 8);
         memcpy(data + 8, device_id, 8);
         */
         memcpy(data, true_data, strlen(true_data));
         int nsend = send(client_socket, data, 16 + strlen(true_data), 0);
         printf("nsend = %d\n", nsend);
         sleep(4);
      }
   }

   close(client_socket);

   return 0;
}


char sum(char p_id[8], char d_id[8]) {
   char _sum = 0;
   for (int i = 0; i < 8; ++i) {
      _sum += p_id[i];
   }
   for (int i = 0; i < 7; ++i) {
      _sum += d_id[i];
   }
   return _sum;
}

int main(int _argc, char **_argv) {
   argc = _argc;
   argv = _argv;
   if(argc != 3) {
      printf("useage:socket_client ipaddress port\n eg:socket_client par             192.168.1.158 5555");
      return -1;
   } 

   pthread_t sig0[SIG0];
   pthread_t sig1[SIG1];
   pthread_t sig2[SIG2];
   pthread_t sig3[SIG3];
   pthread_t sig4[SIG4];

   int i = 0;
   unsigned long long t = 1;
   char project_id[8] = {0x3C, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
   project_device_id_type pdt;
   for (i = 0; i < SIG0; ++i) {
      memcpy(pdt.project_id, project_id, 8);
      memcpy(pdt.device_id, &t, 8);
      pdt.device_id[0] = 0x00;
      pdt.device_id[7] = sum(pdt.project_id, pdt.device_id);
      t++;
      pthread_create(&(sig0[i]), NULL, test, (void *)&pdt);
   }

   for (i = 0; i < SIG1; ++i) {
      memcpy(pdt.project_id, project_id, 8);
      memcpy(pdt.device_id, &t, 8);
      pdt.device_id[0] = 0x01;
      pdt.device_id[7] = sum(pdt.project_id, pdt.device_id);
      t++;
      pthread_create(&(sig1[i]), NULL, test, (void *)&pdt);
   }

   for (i = 0; i < SIG2; ++i) {
      memcpy(pdt.project_id, project_id, 8);
      memcpy(pdt.device_id, &t, 8);
      pdt.device_id[0] = 0x20;
      pdt.device_id[7] = sum(pdt.project_id, pdt.device_id);
      t++;
      pthread_create(&(sig2[i]), NULL, test, (void *)&pdt);
   }

   for (i = 0; i < SIG3; ++i) {
      memcpy(pdt.project_id, project_id, 8);
      memcpy(pdt.device_id, &t, 8);
      pdt.device_id[0] = 0x30;
      pdt.device_id[7] = sum(pdt.project_id, pdt.device_id);
      t++;
      pthread_create(&(sig3[i]), NULL, test, (void *)&pdt);
   }

   for (i = 0; i < SIG4; ++i) {
      memcpy(pdt.project_id, project_id, 8);
      memcpy(pdt.device_id, &t, 8);
      pdt.device_id[0] = 0xFF;
      pdt.device_id[7] = sum(pdt.project_id, pdt.device_id);
      t++;
      pthread_create(&(sig4[i]), NULL, test, (void *)&pdt);
   }

   return 0;
}
