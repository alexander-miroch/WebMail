

#ifndef _H_WMAIL
#define _H_WMAIL

#define E_NOMEM			0x01
#define E_FILE			0x02
#define E_DUP			0x03
#define E_TOOLONG		0x04
#define E_TFORMAT		0x05
#define E_MAXDRV		0x06
#define E_NODRV			0x07
#define E_POST			0x08
#define E_PARSE			0x09
#define E_OPENDIR		0x0a
#define E_MAXFOLDERS		0x0b
#define E_NESTED		0x0c
#define E_MAXMESSAGES		0x0d
#define E_RENAME		0x0e
#define E_MSGFILE		0x0f
#define E_MSGPARSE		0x10
#define E_TIME			0x11
#define E_UTF			0x12
#define E_DBASE			0x13

#define ROOT_SIZE		256
#define DEF_IP			"127.0.0.1"
#define SELF			"/wmail.fcgi"

#define PAGE_404		0x0
#define PAGE_LOGIN		0x1
#define PAGE_FOLDER		0x2
#define PAGE_MBOX		0x3
#define PAGE_MSG		0x4
#define PAGE_ATTACH		0x5
#define PAGE_COMPOSE		0x6
#define PAGE_CFOLDER		0x7
#define PAGE_RESEND		0x8
#define PAGE_UPLOAD_RESULT	0x9
#define PAGE_PREFS		0xa
#define PAGE_ABOOK		0xb
#define PAGE_ABOOK_ADD		0xc
#define PAGE_ABOOK_VIEW		0xd
#define PAGE_ABOOK_GET		0xe
#define PAGE_SOURCE		0xf
#define PAGE_PRINT		0x10
#define PAGE_FILTER		0x11
#define PAGE_FILTER_ADD		0x12
#define PAGE_FILTER_EDIT	0x13
#define PAGE_DONE		0x14

//#define DOMAIN			"miroch.host.euroset.ru"
#define	ROOT			"/var/www/html"
#define SESSION_DIR		ROOT"/tmp"
#define PREFS_DIR		ROOT"/prefs"
#define ERROR_LOG		ROOT"/error.log"

#define MAIL_PATH		"/var/qmail/mailnames"
#define MAX_FOLDERS		512

#define MAX_MESSAGES		16384
#define	MESSAGE_LEN		128

/* Like dir entry */
#define FOLDER_LEN		512
#define EMAIL_LEN		128

#define PATH_LEN		(FOLDER_LEN + EMAIL_LEN + 128) // 128 - msg len max

#define MAX_PASSWD_LEN		16


#define SETERR(x,y)		set_error(x,E_FAULT,__FILE__,__FUNCTION__,__LINE__,y)
#define SETWARN(x,y)		set_error(x,E_WARNING,__FILE__,__FUNCTION__,__LINE__,y)
#define SETNOTICE(x,y)		set_error(x,E_NOTICE,__FILE__,__FUNCTION__,__LINE__,y)

#define DEFAULT_CHARSET		"utf-8"
#define DEFAULT_CHARSET_ALIAS	"utf8"

#define MAX_CC			32

struct fidata;

struct error_struct {
	int error;
	int status;
	char function[32];
	char file[32];
	int line;
	char message[256];
};

void do_page_folder(char *);

#define E_NOTICE	0x1
#define E_WARNING	0x2
#define E_FAULT		0x3

#define	ASIZE(arr)	(sizeof(arr)/sizeof(char *))
void set_error(int,int,const char *,const char *,int,char *);
void check_error(void);

void do_page_msg(char *msg);
char *do_sum_pages(int,void (*pcol)(char *,unsigned char *,char,char,int));

int detect_page(int);
char *alias(char *);

void setup_filter(struct fidata *);
void setup_filters(void);
void setup_spam_filter(void);

enum {
	DO_NOTHING,
	DO_DELETE,
	DO_COPY,
	DO_CFOLDER,
	DO_DFOLDER,
	DO_MOVE,
	DO_SEND,
	DO_EFOLDER,
	DO_SAVEPREFS,
	DO_DELREC,
	DO_DRAFT,
	DO_MARK,
	DO_DELFILTER,
	DO_SWITCHFILTER,
	DO_APPLYFILTER
};

#define MAX_TODO	DO_APPLYFILTER

#define HOSTNAME_LEN		64
#define MAX_PAGES		1024
int validate_email(char *);
int validate_password(char *,int);
int validate_sf(int);
int validate_cf(int);
int validate_rg(int,int);
int validate_todo(int);
int validate_sby(char);
int validate_folder(char *);
int validate_cfolder(char *);
int validate_subfolder(char *);
int validate_msg(char *);
int validate_todel(char *);
int validate_cc(char *);
int validate_sid(char *);
int validate_prname(char *);
int validate_prsign(char *);
int validate_prpp(int);
int validate_prio(int);
int validate_abnick(char *);
int validate_abtel(char *);
int validate_mark(int);
int validate_fact(char *);
int validate_field(int);
int validate_wtodo(int);
int validate_forder(int);
int validate_fedit(char *);



#define	MAX_OFFSET			20971520 //20MB


#define MAX_SIGN			1024
#define MAX_PRNAME			64
#define MAX_PP				128

#define MAX_CC				32

void do_cleans(void);
void do_copy(void);
void do_cfolder(void);
void do_move(void);
void process_request(void);
void print_hello(void);
char *get_page(int);
void setroot(void);

void do_pre_work(void);
void do_delete(void);
void do_saveprefs(void);
unsigned long get_folders(void);

#define AUTH_FAILED	"<tr><td class=\"error\" height=\"32\">Ошибка авторизации</td></tr>"

#define IMGDIR		"/images"
#define IMG_ATTACH	"<img class=\"mfl\" src=\""IMGDIR"/attachment.png\">"
#define IMG_FWD		"<img class=\"mfl\" src=\""IMGDIR"/mail_forwarded.png\">"
#define IMG_REPLIED	"<img class=\"mfl\" src=\""IMGDIR"/mail_answered.png\">"
#define IMG_HPRIO	"<img class=\"mfl\" src=\""IMGDIR"/mail_priority_high.png\">"
#define IMG_LPRIO	"<img class=\"mfl\" src=\""IMGDIR"/mail_priority_low.png\">"
#define IMG_SIGNED	"<img class=\"mfl\" src=\""IMGDIR"/mail_priority_low.png\">"
#define IMG_UNSEEN	"<img class=\"mfl\" src=\""IMGDIR"/mail_unseen.png\">"
#define IMG_FLAGGED	"<img class=\"mfl\" src=\""IMGDIR"/mail_flagged.png\">"
#define IMG_JOIN	"<img class=\"mfl\" src=\""IMGDIR"/tree/joinbottom.png\">"

#define NBSP		"&nbsp;"

#define PAGE_WRAP       32
#define WRAP(i)         (i % PAGE_WRAP || !i) ? NBSP : "<br />"

#endif
