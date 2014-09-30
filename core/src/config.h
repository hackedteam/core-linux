#ifndef _CONFIG_H
#define _CONFIG_H 1

#include <json.h>

int parseconfig(char *filename);
int get_int(json_object *json, char *key, int *val);
int get_double(json_object *json, char *key, int *val);
int get_string(json_object *json, char *key, char **val);
int get_boolean(json_object *json, char *key, int *val);

char user[128];
char host[128];
char instance[21];
char binaryname[256];
int jpeg_libver;
int arch;

#endif /* _CONFIG_H */
