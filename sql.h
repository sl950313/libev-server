#ifndef SQL_H
#define SQL_H

#include "mysql.h"

MYSQL *initSql(const char *ip_string, const char *user_name, const char *password, const char *database_name);

void closeSql(MYSQL *mysql);

void excuteSql(MYSQL *mysql, char *sql);

#endif /** SQL_H */
