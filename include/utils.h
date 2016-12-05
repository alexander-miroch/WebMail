
int url_decode(char *, int);
int htoi(char *);
char *do_utf7(char *);
char *do_toutf7(char *);
void url_encode(unsigned char *,unsigned char *);

int cmp_utf(const void *,const void *);

int inline numdots(char *s);

void inline skipsp(char **s);

int inline is_hbreak(char *val);

int inline is_blank(char *val);
int inline is_boundary(char *val);

char *quoted_printable_decode(char *,char *,int);
char *quoted_printable_encode(char *, int, int);
unsigned char *base64_encode(const unsigned char *, int, int *);
unsigned char *base64_decode(const unsigned char *, int, int *);
int inline min(int,int);
int inline max(int,int);

int inline is_our_charset(char *);

typedef struct datetime {
	char *hmin;
	char *date;
} datetime_t;

void inline set_month(char *, char *);

#define ISDESC          (current.prefs.sby_order == SBY_ORDER_DESC)
#define SBYO_G		ISDESC ? SBY_ORDER_ASC : SBY_ORDER_DESC

short int inline is_ascii(unsigned char *);
char *get_time(char *,unsigned long,char *);
char *seen(char *);
int inline get_s(int,int);

#define TIME_SIZE	200
