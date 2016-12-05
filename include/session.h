
#ifndef _SESSION_H
#include <fcgi_stdio.h>
#include "abook.h"
#include "filter.h"
#define _SESSION_H


typedef struct udata {
	char **marr;
	char *copyfolder,*cfolder;
	int marr_len;
	int mark;
	long int s,e;
	abook_t *abook;
} udata_t;

typedef struct cdata {
	unsigned char *subject;
	char *to;
	char *copy;
	unsigned char *body;
	char *todel;
	int prio;
	int isdraft;
	int ra;
} cdata_t;

typedef struct adata {
	char *msg, *edit;
	char *edit_what;
	char **abarr;
	int abarr_len;
} adata_t;

typedef struct filter_part {
	int field;
	unsigned short yno;
	unsigned char value[MAX_FACT_LEN];
} fpart_t;


typedef struct fidata {
	unsigned short andor;
	char key[KEY_LEN];
	fpart_t fp[NUM_FIELDS];
	unsigned short order;
	unsigned short status;
	unsigned short act[NUM_FIELD_ACTS];
	unsigned char act_data[NUM_FIELD_ACTS][EMAIL_LEN];
} fidata_t;

typedef struct fidata_whole {
	char **fdarr;
	int fdarr_len;
	fidata_t fidata;
} fidata_whole_t;


typedef struct prefs_s {
	unsigned char *name;
	unsigned char *sign;
	int pp;
	short otd;
	char sby, sby_order;
	char *uploaddir;
} prefs_t;

typedef struct {
	char *sid;
	char *email;
	char *password;
	char *path;
	int page;
	int authenticated;
	int todo;
	char *folder,*msg;
	unsigned short ff;
	char *rmsg,*fmsg;
	int sf;
	prefs_t prefs;
	char *ll;
	union {
		udata_t data;
		cdata_t cdata;
		adata_t adata;
		fidata_whole_t fdata;
	};
} session_t;

#define FF_ISFOLDER	0x0
#define FF_ISSENT	0x1
#define FF_ISTRASH	0x2	

#define DEFAULT_PERPAGE	20
#define DEFAULT_FOLDER	"."

#define SESSION_FORMAT "%s|%s|%d"
#define SID_SIZE	128

#define SESSION_EXPIRES		86400   // 24hours
#define FREE_IT(val)	if (val) { free(val); val = NULL; }

//#define CLEAN_PROB	0.01
#define CLEAN_PROB	1
#define SPECIAL_FILE ".htaccess"

char *serialize_session(session_t *);
int unserialize_session(char *,session_t *);

int init_session(void);
char *get_sid(void);

void set_prefs(prefs_t *);
void zero_prefs(prefs_t *);
void free_prefs(prefs_t *);

void to_ascii(unsigned char *,char *);
void save_session(void);
void new_session(void);

void flush_session(void);

FILE *get_prefs_file(char *);
void save_prefs(void);
void read_prefs(void);

void drop_session(void);
void do_page_prefs(void);
void check_prefs_dir(void);

void session_cleaner(void);
void remember(char *);
void set_ll(void);
#endif
