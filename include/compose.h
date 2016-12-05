
#ifndef _H_COMPOSE
#define _H_COMPOSE

#include "message.h"
#define MAX_UPLOADED_FILES 5
#define SENDMAIL "/usr/sbin/sendmail -i"

void flush_attach_dir(void);

char *filelisting(int);
void create_attach_dir(void);

FILE *open_file(void);

void rm_file(char *);
void compose_msg(int);

char *compose_rebody(message_t *);
char *compose_resubj(char *);
char *compose_fwdsubj(char *);

char *compose_draft_body(message_t *);

void move_attaches(message_t *);
void fill_cc(message_t *);

unsigned int set_sign(char *,int);
void sendmail(char *,char *,char *);


#define ARROW(ptr)	*ptr++ = '>', *ptr++ = 0x20

/* MB */
#define FILE_QUOTA	10
#define MAX_QUOTA	20

#endif
