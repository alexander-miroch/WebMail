#define SBY_DATE	'd'
#define SBY_SUBJ 	's'
#define SBY_FROM	'f'
#define SBY_TO		't'
#define SBY_SIZE	'z'

#define SBY_EMAIL	'e'
#define SBY_ANAME	'n'

typedef struct {
	char *name;
	void (*do_work)(char *value);
} params_t;

#define DOCHECK(expr,rv)        if ((expr) < 0) longjmp(rv,1)
//#define DOCHECK(expr,rv)        if ((expr) < 0) { print_hello(); printf("l=%d,f=%s<br>",__LINE__,__FUNCTION__); }
//#define DOCHECK(expr,rv)	
void inline setpage(int);


#define SBY_ORDER_ASC	'a'
#define SBY_ORDER_DESC	'd'


#define CF_CFOLDER	0x1
#define CF_DFOLDER	0x2
#define	CF_COMPOSE	0x3
#define CF_RESEND	0x4
#define CF_EFOLDER	0x5
#define CF_PREFS	0x6
#define CF_DROPSESS	0x7
#define CF_ABOOK	0x8
#define CF_ABOOK_ADD	0x9
#define CF_ABOOK_VIEW	0xa
#define CF_ABOOK_GET	0xb
#define CF_SOURCE	0xc
#define CF_DRAFT	0xd
#define CF_PRINT	0xe
#define CF_FILTER	0xf
#define CF_FILTER_ADD	0x10
#define CF_FILTER_EDIT	0x11

#define CF_MAX		CF_FILTER_EDIT

#define CPRIO_HIGH	0x1
#define CPRIO_MED	0x3
#define CPRIO_LOW	0x5

#define UDEC(v)	{ int l;\
		   l = url_decode(v,strlen(v));\
		   v[l] = 0; }

#define ABTR(v,z)  if (!AB) AB = alloc_abook(); \
		   len = strlen(v); \
		   if (len > (z))  { len = (z) - 1; \
		   v[len] = 0; } ++len;


#define MARK_SEEN	0x1
#define MARK_UNSEEN	0x2
#define MARK_PRIO	0x3
#define MARK_UNPRIO	0x4

#define MAX_MARK	MARK_UNPRIO
