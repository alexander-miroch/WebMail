#ifndef _ABOOK_H
#define _ABOOK_H

#include "wmail.h"

void do_abook(void);
void do_abook_add(void);
void do_abook_view(void);
void do_abook_get(void);


#define AB_NAME_LEN	128
#define AB_TEL_LEN	64
#define AB_ADDR_LEN	256
#define AB_COMM_LEN	256


#define MAX_RECORDS	1024

typedef struct abook_s {
	unsigned char name[AB_NAME_LEN],nick[AB_NAME_LEN];
	unsigned char raddr[AB_ADDR_LEN],daddr[AB_ADDR_LEN];
	unsigned char comm[AB_COMM_LEN];
	char rtel[AB_TEL_LEN],dtel[AB_TEL_LEN],mtel[AB_TEL_LEN];
	char email[EMAIL_LEN];
} abook_t;

abook_t *alloc_abook(void);

void do_delrec(void);
int inline hm(abook_t **);
int (*get_abook_sf(void))(const void *,const void *);

int check_ab_space(char *);
void ab_add2hash(char *,char *,int);

#define AB	current.data.abook
#define ABERR	gdbm_strerror(gdbm_errno)

#define MAX_AB_SPACE	1024 // KB

#endif


