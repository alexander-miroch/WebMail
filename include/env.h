typedef struct {
	char *ip;
	char *cookie;
	char *req_method;
	char *ctype;
	int clength;
	char *qs;
	char *ref;
} env_t;

void init_env(void);
void read_post(void);
void read_get(void);
char *read_cookie(void);
void set_cookie(char *);
void process_post(char *buf, short);
void do_val(char *,char *, short);

typedef struct gb {
	char **data;
	unsigned int len;
} gb_t;

void free_gb(void);
void init_gb(void);

#define PGET	0x1
#define PPOST	0x2
