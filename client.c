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

void *_read(void *arg) {
   printf("in thread _read\n");
   int sock_fd = (int )arg;
   char buffer[256];
   while (1) {
      printf("in recv loop\n");
      int len = recv(sock_fd, buffer, 256, 0);
      buffer[len] = 0x00;
      printf("recv len = %d\n", len);
      printf("recv data = %s\n", buffer);
      sleep(1);
   }
   return (void *)0;
}

int main(int argc, char **argv) {
   if(argc != 3) {
      printf("useage:socket_client ipaddress port\n eg:socket_client par             192.168.1.158 5555");
      return -1;
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


   int bReuseaddr=1;  
   if(setsockopt(client_socket, SOL_SOCKET, SO_REUSEADDR,(const char*)&bReuseaddr,sizeof(bReuseaddr)) != 0)  {  
      printf("setsockopt error in reuseaddr[%d]\n", client_socket);  
      return -1;  
   }  
   printf("Reuse success\n");

   char buffer[16];
   //int len = 0;
   int len = recv(client_socket, buffer, 16, 0);
   printf("recv msg = %s, len = %d\n", buffer, len);
   char project_id[8] = {0x3C, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
   char device_id[8] = {0x01, 0x01, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x9D};
   char device_id_t[8] = {0x00, 0x01, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x9C};
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
         return 0;
      }
      pthread_t id1;
      pthread_create(&id1, NULL, _read, (void *)client_socket);

      char data[256];
      char true_data[12] = "hello";
      while (1) {
         //printf("in send data loop\n");
         memset(data, 0, 256);
         memcpy(data, project_id, 8);
         memcpy(data + 8, device_id_t, 8);
         memcpy(data + 16, true_data, strlen(true_data));
         //int nsend = send(client_socket, data, 16 + strlen(true_data), 0);
         //printf("nsend = %d\n", nsend);
         sleep(4);
      }
   }

   close(client_socket);

   return 0;
}
