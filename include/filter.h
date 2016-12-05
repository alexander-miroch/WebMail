#ifndef _FILTER_H
#define _FILTER_H

#define FTYPE_HEADER	0x1
#define FTYPE_BODY	0x2
#define FTYPE_HPRES	0x3

struct fidata;
enum {
	FACT_IGNORE,
	FACT_MOVE,
	FACT_COPY,
	FACT_DEL,
	FACT_SENDMSG,
	FACT_MARK_PRIO
};

#define MAX_FACT	FACT_MARK_PRIO

#define MAX_FILTERS	32
#define MAX_FACT_LEN	64
#define MAX_FORDER	99


#define F_ACTIVE	0x0
#define F_INACTIVE	0x1


typedef struct fhs_s {
	int fheaders[MAX_FILTERS];
	unsigned int size;
} fhs_t;

typedef struct hfilter_s {
	int header;
	char *regex;
	int rev;
} hfilter_t;


#define NUM_FIELD_ACTS  3
#define NUM_FIELDS      3

typedef struct filter_s {
	short type;
	int is_reverse;
	int andor;
	int action_type[NUM_FIELD_ACTS];
	void *action_data[NUM_FIELD_ACTS];
	union {
		hfilter_t hdr[NUM_FIELDS];
		char *body;
		int header_present;
	};
	struct filter_s *next;
	char key[6];
} filter_t;


int do_filters(char *,char **,int);
void free_filter(filter_t *);
void clean_filters(void);
int apply_filters(char *, char **,char **,int);
int if_match(char *, char *);
char *do_action(char *, int, void *);
char *do_actions(char *, int *, void **);
void add_filter(filter_t *);
filter_t *alloc_filter(void);

void save_filter(struct fidata *, char *, int);
void do_filter_add(void);
void do_filter_view(void);
void do_filter_show_form(void);
void free_fidata(struct fidata *);
void f_setkey(struct fidata *);
int check_fi_space(char *);
int no_acts(struct fidata *);
char *show_filters(char *);
struct fidata * get_filter(char *, char *);
void f_addhash(struct fidata *);
void do_delfilter(void);
void do_applyfilter(void);
void do_switchfilter(void);
struct fidata **collect_filters(char *);
int sby_order(const void *, const void *);

int inline fcount(struct fidata **fds);

void fd_del(char *, char *);
void fd_switch(char *, char *);

#define KEY_LEN		6
#define MSG_BUF		65535

unsigned int f2buf(char *, struct fidata *, int);
char *form_msg(char *, struct fidata *,unsigned long);
char *form_act(char *, struct fidata *,unsigned long);


#define FT		current.fdata.fidata
#define	MAX_FI_SPACE	1024	// Kbytes per user

#endif
