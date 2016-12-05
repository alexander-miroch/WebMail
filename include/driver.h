
#ifndef _H_DRIVERS
#define _H_DRIVERS

#include "fcgi_stdio.h"

#define MAX_DRIVERS	4

struct vals {
	char *name;
	unsigned int nmes,tmes;
};

typedef struct collector {
	struct vals *base;
        int len;
} collect_t;

typedef struct alis {
        char *in,*out;
	int cl;
} alis_t;

typedef struct fdata {
        void *stream;
        int size;
} fdata_t;

fdata_t *alloc_fdata(void);

typedef struct driver {
	alis_t *aliases;
	int (*init)(struct driver *);
	int (*auth)(char *,char *);
	collect_t *(*collect_folders)(void (*)(collect_t *,char *, int,int));
	collect_t *(*collect_messages)(char *,void (*)(collect_t *,char *,int,int));
	int (*check_4new)(collect_t *);
	int (*get_flags)(char *);
	void (*set_flags)(char *,char *,int, char **);
	fdata_t *(*get_msgstream)(char *,char *);
	void (*finish)(fdata_t *);
	char *(*fgets)(char *,int, void *);
	long (*getpos)(void *);
	int (*setpos)(void *,long int);
	int (*delete_msg)(char *,char *);
	int (*copy_tofolder)(char *,char *,char *,char **);
	int (*create_folder)(char *);
	void (*drop_folder)(void);
	void (*empty_folder)(void);
	void (*save_sent)(char *,char *);
	int (*special_exists)(char *);
	char *(*build_path)(char *,char *,char *);
	unsigned long (*get_usage)(void);
	void (*apply_filters)(char *,char *,char **,int);
} driver_t;

struct drivers {
	struct drivers *next;
	driver_t *drv;
	int state;
};


struct drivers *newdrvs(void);
driver_t *select_driver();
int register_driver(driver_t *,int);
void init_drivers(void);
void list_wr(collect_t *,char *,int,int);
char *out_wr(collect_t *,void (*)(char **,int, unsigned int, unsigned int, char *,char *,short));
char *out_ms(collect_t *);
void list_ms(collect_t *,char *);
collect_t *init_cl(int);
void free_cl(collect_t *);


#endif
