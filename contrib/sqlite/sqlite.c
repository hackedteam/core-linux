#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sqlite3.h>
#include <glob.h>
#define SO

static void firefoxmain(void);

int begin = 1367938668;
int end = 1367938675;

int main(void)
{
   firefoxmain();

   return 0;
}

static void firefoxmain(void)
{
   int i;
   char *query;
   glob_t g = {0};
   sqlite3 *db = NULL;
   sqlite3_stmt *stmt = NULL;

   do {
      /*if(initlib(INIT_LIBSQLITE3)) break;*/
      if(glob(SO"~/.mozilla/firefox/*/places.sqlite", GLOB_NOSORT|GLOB_TILDE, NULL, &g)) break;

      for(i = 0; i < g.gl_pathc; i++) {
         do {
            query = SO"SELECT IFNULL(B.url, ''), IFNULL(B.title, ''), A.visit_date/1000000 AS timestamp FROM moz_historyvisits AS A JOIN moz_places AS B ON A.place_id = B.id WHERE timestamp BETWEEN ? AND ?";

            if(sqlite3_open(g.gl_pathv[i], &db) != SQLITE_OK) break;
            if(sqlite3_prepare_v2(db, query, strlen(query) + 1, &stmt, NULL) != SQLITE_OK) break;
            if(sqlite3_bind_int(stmt, 1, begin) != SQLITE_OK) break;
            if(sqlite3_bind_int(stmt, 2, end) != SQLITE_OK) break;
            while(sqlite3_step(stmt) == SQLITE_ROW) {
               /* TODO gestire con una lista */
               if(!list) {
                  if(!(list = malloc(sizeof(struct entry)))) break;
                  listp = list;
               } else {
                  if(!(listp->next = malloc(sizeof(struct entry)))) break;
                  listp = listp->next;
               }

               listp->next = NULL;
               listp->browser = BROWSER_FIREFOX;
               listp->url = strdup(sqlite3_column_text(stmt, 0));
               listp->title = sqlite3_column_text(stmt, 1));
               listp->time = (time_t)sqlite3_column_int(stmt, 2);
               /* TODO */
            }
         } while(0);
         if(stmt) { sqlite3_finalize(stmt); stmt = NULL; }
         if(db) { sqlite3_close(db); db = NULL; }
      }
   } while(0);
   globfree(&g);

   return;
}

