
#include "message.h"
int parse_subject(message_t *,char *);
int pparse_subj(message_t *,char *);
int parse_from(message_t *,char *);

int parse_rcvd(message_t *,char *);
int parse_date(message_t *,char *);
int parse_xpriority(message_t *,char *);

int parse_ctype(message_t *,char *);
int parse_ctype_m(message_t *,char *);

int parse_ct_args(mime_t *,char *);

#define MAX_TOKENS 24

typedef struct sc {
        char *ptr;
        unsigned short fd;
} sc_t;

int parse_cte(message_t *, char *) ;
int parse_mime(message_t *, mime_t *, char *) ;
int parse_mime_cte(message_t *, mime_t *, char *) ;

int parse_cdisp(message_t *, char *) ;
int parse_mime_cdisp(message_t *, mime_t *, char *) ;
char *rfc2047(char *,char **);

int parse_ctid(message_t *, char *) ;
int parse_mime_ctid(message_t *, mime_t *, char *) ;


char *parse_email_from(char *);
char *parse_who_from(char *);

int parse_subject_x(char **,char *);
int parse_mime_from(message_t *, mime_t *, char *) ;
int parse_mime_subject(message_t *, mime_t *, char *) ;
int parse_cc(message_t *, char *) ;
int parse_to(message_t *, char *);
int parse_rpto(message_t *, char *);

char *post_cdisp(char *);

void add(char **,char *, int);

#define MAX_SUBST_LEN	7

char *show_filename(char *);

typedef struct subst {
	char c;
	char *to;
	int len;
} subst_t;
