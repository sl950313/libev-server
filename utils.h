#ifndef UTILS_H
#define UTILS_H
#include <stdio.h>
#include <string>
#include "mainwindow.h"

void wlog(int mode, char *log, FILE *fp);
int delID(unsigned long long *project_id_num, unsigned long long *device_id_num, MainWindow *w);
int recoverID(unsigned long long *project_id_num, unsigned long long *device_id_num, MainWindow *w);

#endif /** */
