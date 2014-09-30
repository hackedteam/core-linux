#ifndef _BF_DEBMELTER_H
#define _BF_DEBMELTER_H

#define BIO_TYPE_DEBMELTER (50|0x0200)

#define BIO_F_DEBMELTER_STEP_INIT         0x00
#define BIO_F_DEBMELTER_STEP_ENTRY_HEADER 0x01
#define BIO_F_DEBMELTER_STEP_ENTRY_MELT   0x02
#define BIO_F_DEBMELTER_STEP_ENTRY_DATA   0x02
#define BIO_F_DEBMELTER_STEP_PASSTHRU     0xff

#define BIO_CTRL_GET_DEBMELTER_FILENAME 500
#define BIO_CTRL_SET_DEBMELTER_FILENAME 501

#define BIO_get_debmelter_filename(b) BIO_ptr_ctrl(b, BIO_CTRL_GET_DEBMELTER_FILENAME, 0)
#define BIO_set_debmelter_filename(b, filename) BIO_ctrl(b, BIO_CTRL_SET_DEBMELTER_FILENAME, 0, (char *)filename)

BIO_METHOD *BIO_f_debmelter(void);
BIO *BIO_new_debmelter(const char *filename);

#endif /* _BF_DEBMELTER_H */
