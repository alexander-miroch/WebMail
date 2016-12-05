#ifndef _TEMPLATE
#define _TEMPLATE

#define TEMPLATE_DIR	"templates"
#define HASH_TABLE_SIZE		256


#define S_OK			0
#define S_DOG1			1
#define S_DOG2			2
#define S_LARROW1		3
#define S_LARROW2		4
#define S_RARROW1		5
#define S_RARROW2		6

#define MAX_SUB_LEN	64

#define ZHASH(val)		add2hash(val,"")

int open_template(char *);
unsigned int do_hash(char *, unsigned int);

void init_hash(void);
void add2hash(char *,char *);
char *get_val(char *);
void process_tmpl(int);
int parse_buf(char *);
int check_state(char *);
void clean_hash(void);

void to_read(int,char *);
void do_template(char *);

#endif
