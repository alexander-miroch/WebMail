#ifndef _MESSAGE_H
#define _MESSAGE_H

#include <time.h>
#include "fcgi_stdio.h"

#define FL_PASSED	0x1
#define FL_REPLIED	0x2
#define FL_SEEN		0x4
#define FL_TRASHED	0x8
#define FL_DRAFT	0x10
#define FL_FLAGGED	0x20

#define MAX_FLAGS	10

#define FL_ATTACH	0x40
#define FL_SIGNED	0x80
#define FL_RELATED	0x100


#define MFL_LAST	0x1
#define MFL_ISDISPLAY	0x2
#define MFL_ISALTER	0x4
#define MFL_ISPARALLEL	0x8
#define MFL_ALTERFOUND	0x10
#define MFL_ALTERREAD	0x20
#define MFL_ISOUR	0x40
#define MFL_CATT	0x80
#define MFL_MESSAGE	0x100
#define MFL_RELATED	0x200

#define DUMMY_FILE      "file.data"

typedef struct mime {
	char **headers;
	char *type;
	char *name,*filename;
	char *subject, *email_from, *from;
	long int size;
	int enc;
	int flags;
	char *boundary;
	char *charset;
	char *ctid;
	long start,rstart,end;
	struct mime **childs;
	struct mime *parent;
	int total;
	struct mime *sm;
} mime_t;


#define LAST_POS	-1

mime_t *alloc_mime(void);
void free_mime(mime_t *);

enum {
	ZERO,
	PFL_BSUBJ
};

typedef struct message {
	char **headers;
	char *file;
	mime_t *p0;
	int flags;
	int pflags;
	time_t rcvd;
	char *date,*senddate;
	char *from,*to,*replyto;
	char *email_from,*email_to,*email_rpto;
	char *subject;
	char *cc, *bcc;
	char *cc_email,*bcc_email;
	unsigned char *body;
	int bodylen;
	int size;
	int plen,pi;
	int prio;
} message_t;


int (*get_sort_func(void))(const void *,const void *);

typedef struct header {
	char *name;
	int (*parse)(message_t *,char *);
	int (*pparse)(message_t *,char *);
	int (*mparse)(message_t *,mime_t *,char *);
} header_t;


#define HEADER(x)		headers[x].name
#define PARSE(msg,x,y)		headers[x].parse(msg,y)
#define PPARSE(msg,x,y)		headers[x].pparse(msg,y)
#define MPARSE(msg,mime,x,y)	headers[x].mparse(msg,mime,y)



enum headers_n {
	H_RECEIVED,
	H_FROM, 
	H_SUBJECT,
	H_XPRIORITY,
	H_CONTENT_TYPE,
	H_CONTENT_TRANSFER_ENCODING,
	H_CONTENT_DISPOSITION,
	H_CONTENT_ID,
	H_TO,
	H_REPLY_TO,
	H_SPAM_STATUS,	// 10
	H_CC,
	H_DATE,
};

#define MAX_HDR H_DATE

#define CTE_UNDEF	0x0
#define CTE_7BIT	0x1
#define CTE_QP		0x2
#define CTE_BASE64	0x3

#define SET_CT	headers[H_CONTENT_TYPE].parse = parse_ctype_m
#define RET_CT	headers[H_CONTENT_TYPE].parse = parse_ctype

// Subject, Received, From, Priority
#define HDRS_COUNT(ph)		(sizeof(ph)/sizeof(int))

#define NUM_MIME_PARTS		32
#define NUM_MIME_HEADERS	10	   
#define NUM_HEADERS		32



// RFC2822 + 2  '\n' + '\0'
#define MSG_LINE_SIZE		1000

message_t *alloc_msg(void);
void free_msg(message_t *);

void read_mimes(void *, mime_t *, message_t *);
void inline clear_disp(mime_t *);
void read_headers(void *,char **,int *,int);
void read_body(void *, message_t *);

char *get_header_bar(message_t *);
void show_mime(message_t *,mime_t *,char *);
char *get_attach(char *,FILE *,long,long);

message_t *get_msg(char *);
void do_parse(message_t *, int *, int);
void do_post_parse(message_t *, int *, int);
void do_mime_parse(message_t *, mime_t *,int *, int);
int msg2buf(char *,message_t *);

char *do_subst(char *,int *,int);
char *do_html_subst(char *,int *);
int read_part(message_t *, mime_t *,char *, void *);
void part_convert(message_t *, mime_t *);

void detect_size(mime_t *);

void accum_left(char **, int, unsigned int, unsigned int, char *, char *,short);
void accum_copy(char **, int, unsigned int, unsigned int, char *, char *,short);
int skip_attach(mime_t *, void *,int *);

void check_message(message_t *);
char *fill_file(message_t *);

void get_source(char *);

unsigned char *do_related_stuff(unsigned char *,message_t *);

#define ISOPEN(v)		(v)?"folder_open.png":"folder.png"


#define TRTD		"<tr><td>"
#define TDTD		"</td><td>"
#define TDTR		"</td></tr>"

#define MPART_BREAK	"<table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\"><tr><td class=\"x\"> Next part: </td></tr></table><br>"


#define JSFL_DISP	0x1
#define JSFL_OPEN	0x2
#define JSFL_CURR	0x4

#endif
