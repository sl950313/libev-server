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

extern map<project_id_type, vector<set<fd_device_id_type > > > project_ids;
extern pthread_mutex_t online_users_lock, forbidden_IDs_lock, project_ids_lock;
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
   fflush(log_file_fp);
   //fprintf(fp, )
}

int delID(unsigned long long *project_id_num, unsigned long long *device_id_num, MainWindow *w) {
   char project_id[8], device_id[8];
   memcpy(project_id, project_id_num, 8);
   memcpy(device_id, device_id_num, 8);

   char info[32];
   //printf("project_id = %s, device_id = %s\n", strToHex(project_id, 8, info), strToHex(device_id, 8, info));
   reverse_char_8(project_id);
   reverse_char_8(device_id);
   //printf("project_id = %s, device_id = %s\n", strToHex(project_id, 8, info), strToHex(device_id, 8, info));

   int fd;
   project_device_id_type pdt;
   memcpy(pdt.project_id, project_id, 8);
   memcpy(pdt.device_id, device_id, 8);

   project_id_type pit;
   memcpy(pit.project_id, project_id, 8);
   pthread_mutex_lock(&project_ids_lock);
   map<project_id_type, vector<set<fd_device_id_type > > >::iterator it = project_ids.find(pit);
   int found = 0;
   for (size_t i = 0; i < it->second.size(); ++i) {
       for (set<fd_device_id_type>::iterator ite = it->second[i].begin(); ite != it->second[i].end(); ++ite) {
           if (memcmp(ite->device_id, &device_id, 8) == 0) {
               fd = ite->fd;
               found = 1;
               it->second[i].erase(ite);
               break;
             }
         }
       if (found) {
           break;
         }
     }

   pthread_mutex_unlock(&project_ids_lock);

   if (!found) {
       QMessageBox::information(w, "error", "ID not online");
       return -1;
     }

   //freeProjectMap(fd);
   freeUserfdSign(fd);
   freelibev(loop, fd);
   QMessageBox::information(w, "info", "ID success forbidden");
   w->sendMsg("ID success forbidden");
   char tmp[256] = {0};
   memset(info, 0, 32);
   char info2[32] = {0};
   sprintf(tmp, "ID:%s %s success forbidden", strToHex(project_id, 8, info), strToHex(device_id, 8, info2));
   wlog(INFO, tmp);

   pthread_mutex_lock(&forbidden_IDs_lock);
   forbidden_IDs.insert(pdt);
   //printf("after insert fobiddent_IDs.size() = %ld\n", forbidden_IDs.size());
   pthread_mutex_unlock(&forbidden_IDs_lock);
   w->sendUPdateForbiddenIDs(*project_id_num, *device_id_num);

   return 0;
}

int recoverID(unsigned long long *project_id_num, unsigned long long *device_id_num, MainWindow *w) {
  project_device_id_type pdt;
  memcpy(pdt.project_id, project_id_num, 8);
  memcpy(pdt.device_id, device_id_num, 8);

  reverse_char_8(pdt.project_id);
  reverse_char_8(pdt.device_id);
  char tmp[256] = {0}, info[32] = {0}, info2[32] = {0};

  pthread_mutex_lock(&forbidden_IDs_lock);
  set<project_device_id_type>::iterator it = forbidden_IDs.find(pdt);
  if (it != forbidden_IDs.end()) {
      forbidden_IDs.erase(it);
      pthread_mutex_unlock(&forbidden_IDs_lock);
      QMessageBox::information(w, "info", "ID success recover");
      sprintf(tmp, "ID:%s %s success recover", strToHex(pdt.project_id, 8, info), strToHex(pdt.device_id, 8, info2));
      wlog(INFO, tmp);
    } else {
      pthread_mutex_unlock(&forbidden_IDs_lock);
      QMessageBox::information(w, "info", "ID not forbidden yet");
      sprintf(tmp, "ID:%s %s not forbidden yet", strToHex(pdt.project_id, 8, info), strToHex(pdt.device_id, 8, info2));
      wlog(ERROR, tmp);
    }
  w->sendUPdateForbiddenIDs(0, 0);
  return 0;
}

char *strToHex(char *str, int len, char *hex) {
  //int begin = 0;
    for (int i = 0; i < len; ++i) {
        sprintf(hex + 2 * i, "%02x", (str[i] & 0xFF));
      }
    return hex;
}

void reverse_char_8(char *id) {
  for (int i = 0; i < 4; ++i) {
      char tmp = id[i];
      id[i] = id[7 - i];
      id[7 - i] = tmp;
    }
}
