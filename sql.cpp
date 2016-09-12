#include "sql.h"
#include <stdio.h>

MYSQL *initSql(const char *ip_string, const char *user_name, const char *password, const char *database_name) {
   MYSQL *mysql = (MYSQL *)malloc(sizeof(MYSQL));
   mysql_init(mysql);

   if (mysql_real_connect(mysql, ip_string, user_name, password, database_name, 3306, NULL, 0) == NULL) {
      printf("error in connect database\n");
      return NULL;
   }
   return mysql;
}

void closeSql(MYSQL *mysql) {
   mysql_close(mysql);
}

void excuteSql(MYSQL *mysql, char *sql) {
   mysql_query(mysql, sql);
}
