#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gnome-keyring.h>

int main(void)
{
   GList *keyrings = NULL, *k, *entries = NULL, *e;
   GnomeKeyringInfo *keyringinfo;
   GnomeKeyringItemInfo *iteminfo;
   GnomeKeyringAttributeList *attributes;
   GnomeKeyringAttribute *a;
   char *pass, *user, *service, *resource;
   time_t mtime;
   int i;

   do {
      if(!gnome_keyring_is_available()) break;
      if(gnome_keyring_list_keyring_names_sync(&keyrings) != GNOME_KEYRING_RESULT_OK) break;
      for(k = keyrings; k; k = k->next) {
         do {
            keyringinfo = NULL;
            if((gnome_keyring_get_info_sync((char *)k->data, &keyringinfo) != GNOME_KEYRING_RESULT_OK) || gnome_keyring_info_get_is_locked(keyringinfo)) break;

            if(gnome_keyring_list_item_ids_sync((char *)k->data, &entries) != GNOME_KEYRING_RESULT_OK) break;
            for(e = entries; e; e = e->next) {
               do {
                  iteminfo = NULL;
                  attributes = NULL;
                  pass = user = service = resource = NULL;
                  if(gnome_keyring_item_get_info_sync((char *)k->data, (unsigned int)e->data, &iteminfo) != GNOME_KEYRING_RESULT_OK) break;
                  if(!(pass = gnome_keyring_item_info_get_secret(iteminfo)) || !pass[0]) break;
                  mtime = gnome_keyring_item_info_get_mtime(iteminfo);
                  /* TODO mettere il controllo sulla data */
                  /* if((mtime < begin) || (mtime > end)) break; */
                  if(gnome_keyring_item_get_attributes_sync((char *)k->data, (unsigned int)e->data, &attributes) != GNOME_KEYRING_RESULT_OK) break;
                  do {
                     for(i = 0; i < attributes->len; i++) {
                        a = &gnome_keyring_attribute_list_index(attributes, i);
                        if(!strcmp(a->name, "application") && (a->type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING) && !strncmp(a->value.string, "chrome-", strlen("chrome-"))) {
                           resource = "Chrome";
                        } else if(!strcmp(a->name, "username_value") && (a->type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING)) {
                           user = a->value.string;
                        } else if(!strcmp(a->name, "origin_url") && (a->type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING)) {
                           service = a->value.string;
                        }
                     }
                     if(resource && service && user) {
                        /* TODO inserire l'elemento nella lista */
                        printf("%s %s %s %s\n", resource, service, user, pass);
                        break;
                     }
                  } while(0);
               } while(0);
               if(iteminfo) gnome_keyring_item_info_free(iteminfo);
               if(attributes) gnome_keyring_attribute_list_free(attributes);
               if(pass) free(pass);
            }
         } while(0);
         if(keyringinfo) gnome_keyring_info_free(keyringinfo);
         if(entries) g_list_free(entries);
      }
   } while(0);
   if(keyrings) gnome_keyring_string_list_free(keyrings);

   return 0;
}
