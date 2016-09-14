#include "utils.h"
#include "_struct.h"
#include "testserver.h"
#include "mainwindow.h"
#include <qdialog.h>
#include <QMessageBox>
#include <string>
#include <string.h>
#include <map>
#include <ev.h>

using namespace std;

extern pthread_mutex_t online_users_lock;
extern map<project_device_id_type, int> online_users;
extern struct ev_loop *loop;

void wlog(int mode, char *log, FILE *fp) { 
   //fprintf(fp, )
}

int delID(unsigned long long *project_id_num, unsigned long long *device_id_num, MainWindow *w) {
   char project_id[8], device_id[8];
   memcpy(project_id, project_id_num, 8);
   memcpy(device_id, device_id_num, 8);

   int fd;
   project_device_id_type pdt;
   memcpy(pdt.project_id, project_id_num, 8);
   memcpy(pdt.device_id, device_id_num, 8);
   pthread_mutex_lock(&online_users_lock);
   map<project_device_id_type, int>::iterator it = online_users.find(pdt);
   if (it != online_users.end()) {
      fd = it->second;
      online_users.erase(it);
   } else {
      printf("ID not online\n");
      pthread_mutex_unlock(&online_users_lock);
      QMessageBox::information(w, "error", "ID not online");
      return -1;
   }
   pthread_mutex_unlock(&online_users_lock);

   freeProjectMap(fd);
   freeUserfdSign(fd);
   freelibev(loop, fd);
   QMessageBox::information(w, "info", "ID success forbidden");

   return 0;
}
