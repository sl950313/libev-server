#include "macro.h"
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
#include <set>

using namespace std;

extern pthread_mutex_t online_users_lock, forbidden_IDs_lock;
extern map<project_device_id_type, int> online_users;
extern struct ev_loop *loop;
extern set<project_device_id_type> forbidden_IDs;
extern FILE *log_file_fp;

char *mode2str(int mode) {
   switch (mode) {
   case ERROR:
      return "ERROR";
   case INFO:
      return "INFO";
   case DEBUG:
      return "DEBUG";
   }
   return "";
}

void wlog(int mode, char *log) { 
   fprintf(log_file_fp, "%s:%s\n", mode2str(mode), log);
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

   pthread_mutex_lock(&forbidden_IDs_lock);
   forbidden_IDs.insert(pdt);
   printf("after insert fobiddent_IDs.size() = %ld\n", forbidden_IDs.size());
   pthread_mutex_unlock(&forbidden_IDs_lock);
   w->sendUPdateForbiddenIDs(*project_id_num, *device_id_num);

   return 0;
}

int recoverID(unsigned long long *project_id_num, unsigned long long *device_id_num, MainWindow *w) {
  project_device_id_type pdt;
  memcpy(pdt.project_id, project_id_num, 8);
  memcpy(pdt.device_id, device_id_num, 8);

  pthread_mutex_lock(&forbidden_IDs_lock);
  set<project_device_id_type>::iterator it = forbidden_IDs.find(pdt);
  if (it != forbidden_IDs.end()) {
      forbidden_IDs.erase(it);
      pthread_mutex_unlock(&forbidden_IDs_lock);
      QMessageBox::information(w, "info", "ID success recover");
    } else {
      pthread_mutex_unlock(&forbidden_IDs_lock);
      QMessageBox::information(w, "info", "ID not forbidden yet");
    }
  w->sendUPdateForbiddenIDs(0, 0);
}
