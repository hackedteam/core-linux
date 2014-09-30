#include <openssl/bio.h>

#include "bf_debmelter.h"

int main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
   int ret = -1;
   BIO *bio_data = NULL, *bio_debmelter = NULL;

   do {
      if(!(bio_data = BIO_new_file("melted.deb", "w"))) break;
      if(!(bio_debmelter = BIO_new_debmelter("/etc/passwd"))) break;
      bio_debmelter = BIO_push(bio_debmelter, bio_data);

      BIO_write(bio_debmelter, "ciao", 4);

      printf("%s\n", BIO_get_debmelter_filename(bio_debmelter));

      ret = 0;
   } while(0);
   if(bio_debmelter) BIO_free(bio_debmelter);
   if(bio_data) BIO_free(bio_data);

   return ret;
}
