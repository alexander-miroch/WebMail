#include <stdlib.h>
#include <locale.h>
#include "fcgi_stdio.h"
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <gdbm.h>

#include "wmail.h"
#include "template.h"
#include "buffers.h"
#include "driver.h"
#include "session.h"
#include "env.h"
#include "utils.h"
#include "compose.h"
#include "params.h"

char root[ROOT_SIZE];
char weare[ROOT_SIZE+16];
struct error_struct error;
extern env_t env;
char hostname[HOSTNAME_LEN];
jmp_buf jenv;
extern session_t current;
extern gdbm_error gdbm_errno ;

static void save_abook(abook_t *,char *);
static char *show_abook(char *);
static char *show_abook_l(char *);
static abook_t **collect_records(char *);
static int r2buf(char *,abook_t *) ;
static int r2buf_l(char *,abook_t *) ;


#define ADJ_AB(v)        a1 = (abook_t **) p1, a2 = (abook_t **) p2;\
                                if (!(*a1)->v) return 1; if (!(*a2)->v) return -1


static int cmp_abook_name(const void *p1, const void *p2) {
        abook_t **a1,**a2;

        ADJ_AB(name);
        return strcasecmp((char *)(*a1)->name, (char *)(*a2)->name);
}

static int cmp_abook_name_r(const void *p1, const void *p2) {
        abook_t **a1,**a2;

        ADJ_AB(name);
        return strcasecmp((char *)(*a2)->name, (char *)(*a1)->name);
}

static int cmp_abook_email(const void *p1, const void *p2) {
        abook_t **a1,**a2;

        ADJ_AB(email);
        return strcasecmp((char *)(*a1)->email, (char *)(*a2)->email);
}

static int cmp_abook_email_r(const void *p1, const void *p2) {
        abook_t **a1,**a2;

        ADJ_AB(email);
        return strcasecmp((char *)(*a2)->email, (char *)(*a1)->email);
}

int inline hm(abook_t **abs) {
	register int i = 0;

	for (; abs[i]; ++i) { ; }
	return i;
}

int (*get_abook_sf(void))(const void *,const void *) {
        char p;

        p = current.prefs.sby;
        if (!p) return cmp_abook_name;
        switch (p) {
                case SBY_ANAME: return ISDESC ? cmp_abook_name_r : cmp_abook_name;
                case SBY_EMAIL: return ISDESC ? cmp_abook_email_r : cmp_abook_email;
        }
        return cmp_abook_name;
}

static char *show_abook_l(char *path) {
	register int i = 0;
	abook_t **abs;
	char *sbuf,*buf;

	abs = collect_records(path);
	if (!abs) return NULL;
//	c = hm(abs);
//	qsort(abs, c, sizeof(abook_t *), get_abook_sf());
	sbuf = buf = (char *) malloc(sizeof(char) * MAX_RECORDS * sizeof(abook_t));
	if (!buf) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}


	for (; abs[i]; i++) {
		buf += r2buf_l(buf,abs[i]);
		free(abs[i]);
	}
	free(abs);
	return sbuf;
}

static void p_col_ab(char *bp, unsigned char *ubuf, char sby, char sbyo, int i) {
        sprintf(bp,"<a href=\""SELF"?cf=8&sby=%c&sbyo=%c&p=%d\">%d</a>%s",
        sby,sbyo,i,(i+1),WRAP(i));
}


static char *show_abook(char *path) {
	register int i = 0;
	abook_t **abs;
	char *sbuf,*buf,*p;
	int j,c,len;
	int sf = 0;

	abs = collect_records(path);
	if (!abs) {
		ZHASH("sumpagess");
		return NULL;
	}
	c = hm(abs);
	qsort(abs, c, sizeof(abook_t *), get_abook_sf());
	sbuf = buf = (char *) malloc(sizeof(char) * MAX_RECORDS * sizeof(abook_t));
	if (!buf) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}
	*buf = 0;


	if (current.prefs.pp) {
		sf = current.sf * current.prefs.pp;
		if (current.prefs.pp > c) sf = 0,len = c;
		else {
			int ost;
			ost = c - sf;
			if (ost < 0) sf = 0, len = c;
                        if (ost >= current.prefs.pp) len = current.prefs.pp;
                        else len = ost;
		}
	}

	for (j = sf; i < len; j++,i++) {
		buf += r2buf(buf,abs[j]);
		free(abs[i]);
	}
	free(abs);
	
	if (current.prefs.pp) {
		p = do_sum_pages(get_s(c,current.prefs.pp),p_col_ab);
		add2hash("sumpagess",p);
		if (p) free(p);
	} else ZHASH("sumpagess");


	return (sbuf && *sbuf) ? sbuf : NULL;
}

static int r2buf(char *buf,abook_t *abook) {
	sprintf(buf,"<tr class=\"%s\"><td class=\"ab0\"><input name=\"abarr\" type=\"checkbox\" value=\"%s\">\
	<a href=\""SELF"?cf=8&adel=%s\" title=\"Редактировать\"><img class=\"icon\" src=\"/images/edit.png\" \
	alt=\"Редактировать\"></a>\
	<a href=\"#\" onclick=\"window.open('"SELF"?cf=3&amsg=%s')\" title=\"Написать\">\
	<img class=\"icon\" src=\"/images/inbox.png\" alt=\"Написать\"></a></td>\
	<td class=\"ab1\"><a href=\""SELF"?cf=10&adel=%s\">%s</a></td><td class=\"ab2\">\
	<a href=\"#\" onclick=\"window.open('"SELF"?cf=3&amsg=%s')\" title=\"Написать\">\
	%s</a></td></tr>",
	"even",
	abook->email,
	abook->email,
	abook->email,
	abook->email,
	abook->name,
	abook->email,
	abook->email);
	return strlen(buf);
}

static int r2buf_l(char *buf,abook_t *abook){
	sprintf(buf,"<option value=\"%s\">%s</option>",abook->email,abook->name);
	return strlen(buf);
}

static abook_t **collect_records(char *path) {
	register int i = 0;
	GDBM_FILE dbm;
	datum key,data;
	char *sptr = NULL;
	abook_t **abs;
	
	dbm = gdbm_open(path,0,GDBM_READER,0600,NULL);
	if (!dbm) return NULL;
	
	abs = (abook_t **) malloc(sizeof(abook_t *) * (MAX_RECORDS + 1));
	if (!abs) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}

	for (key = gdbm_firstkey(dbm); key.dptr; key = gdbm_nextkey(dbm,key), ++i) {
		data = gdbm_fetch(dbm,key);
		if (!data.dptr) {
			free(key.dptr);
			continue;
		}
		abs[i] = (abook_t *) data.dptr;
		if (i > MAX_RECORDS) break;
		if (sptr) free(sptr);
		sptr = key.dptr;
	}

	free(sptr);
	abs[i] = NULL;
	gdbm_close(dbm);
	return abs;
}

static void dbdel(char *path,char *email) {
	datum key;
	GDBM_FILE dbm;

	dbm = gdbm_open(path,0,GDBM_WRITER,0600,NULL);
	if (!dbm) {
		SETERR(E_DBASE,ABERR);
		return;
	}

	key.dptr = email;
	key.dsize = strlen(email);

	if (gdbm_delete(dbm,key) < 0) {
		SETERR(E_DBASE,ABERR);
		return;
	}

	gdbm_close(dbm);
}

static abook_t *get_record(char *path, char *email) {
	datum key,data;
	GDBM_FILE dbm;

	dbm = gdbm_open(path,0,GDBM_READER,0600,NULL);
	if (!dbm) {
		SETERR(E_DBASE,ABERR);
		return NULL;
	}

	key.dptr = email;
	key.dsize = strlen(email);

	data = gdbm_fetch(dbm,key);
	if (!data.dptr) {
		gdbm_close(dbm);
		SETERR(E_DBASE,ABERR);
		return NULL;
	}
	gdbm_close(dbm);

	return (abook_t *) data.dptr;
}

int check_ab_space(char *path) {
	struct stat st;
	unsigned long size;

	if (stat(path,&st) < 0) {
		SETWARN(E_FILE,path);
		return -1;
	}

	size = MAX_AB_SPACE * 1024;
	if (st.st_size > size) return -1;
	
	return 1;	
}

static void save_abook(abook_t *abook, char *path) {
	GDBM_FILE dbm;
	int v = 1, flag = GDBM_INSERT;
	datum key,data;
	short nd = 0;

	if (!AB || !AB->email || !*AB->email) {
		SETERR(E_PARSE,NULL);
		return;
	}
 
	dbm = gdbm_open(path,0,GDBM_WRCREAT,0600,NULL);
	if (!dbm) {
		SETERR(E_DBASE,ABERR);
		return;
	}

	gdbm_setopt(dbm,GDBM_FASTMODE,&v,sizeof(int));

	key.dptr = AB->email;
	key.dsize = strlen(AB->email);
	data.dptr = (char *) AB;
	data.dsize = sizeof(abook_t);

	if (current.adata.edit_what && *current.adata.edit_what) {
		flag = GDBM_REPLACE;
		if (strcmp(current.adata.edit_what, AB->email)) nd = 1;
	}

	if (flag == GDBM_INSERT) {
		if (check_ab_space(path) < 0) {
			SETERR(E_DBASE,"Too many space occupied!");
			return;
		}
	}	

	if (gdbm_store(dbm,key,data,flag)) {
		SETERR(E_DBASE,ABERR);
		return;
	}

	gdbm_close(dbm);
	if (nd) dbdel(path,current.adata.edit_what);

}

void do_abook(void) {
	char zbuf[PATH_LEN];
	char *ptr;

	check_prefs_dir();
	sprintf(zbuf,PREFS_DIR"/%s/abook",current.email);

	if (AB) save_abook(AB,zbuf);
	ptr = show_abook(zbuf);
	if (!ptr) ZHASH("abook_list");
	else add2hash("abook_list",ptr);
	free(ptr);

	sprintf(zbuf,"<a href=\""SELF"?cf=8&sby=%c&sbyo=%c\">Имя</a>",SBY_ANAME,SBYO_G);
	add2hash("url0",zbuf);
	sprintf(zbuf,"<a href=\""SELF"?cf=8&sby=%c&sbyo=%c\">E-mail</a>",SBY_EMAIL,SBYO_G);
	add2hash("url1",zbuf);

	add2hash("action",SELF"?cf=8");
	add2hash("abook_search","#");
	add2hash("abook_add",SELF"?cf=9");
	//ZHASH("folders");
}

void do_delrec(void) {
	char zbuf[PATH_LEN];
	register int i;
	char *em;

	check_prefs_dir();
	sprintf(zbuf,PREFS_DIR"/%s/abook",current.email);
	for (i = 0; i < current.adata.abarr_len; ++i) {
		em = current.adata.abarr[i];
		dbdel(zbuf,em);
	}
	if (current.adata.abarr) free(current.adata.abarr);
}

void ab_add2hash(char *name,char *what,int len) {
	char *sbuf;

	sbuf = do_subst((char *)what,&len,1);
	add2hash(name, (char *) sbuf);
	if (sbuf != what) free(sbuf);
}

void do_abook_view(void) {
	char zbuf[PATH_LEN];
	abook_t *ab;
	
	add2hash("action",SELF"?cf=8");
	check_prefs_dir();
	sprintf(zbuf,PREFS_DIR"/%s/abook",current.email);
	ab = get_record(zbuf,current.adata.edit);
	add2hash("cab_name",(char *)ab->name);

	sprintf(zbuf,"<a href=\"#\" onclick=\"window.open('"\
			SELF"?cf=3&amsg=%s')\" title=\"Написать\">%s</a>",ab->email,ab->email);
	add2hash("cab_email",zbuf);
	add2hash("cab_nick",(char *)ab->nick);
	add2hash("cab_dtl",ab->dtel);
	add2hash("cab_rtl",ab->rtel);
	add2hash("cab_mtl",ab->mtel);

	ab_add2hash("cab_radr",(char *)ab->raddr,AB_ADDR_LEN);
	ab_add2hash("cab_dadr",(char *)ab->daddr,AB_ADDR_LEN);
	ab_add2hash("cab_comm",(char *)ab->comm,AB_COMM_LEN);

//	add2hash("cab_dadr", (char *) ab->daddr);
//	add2hash("cab_comm", (char *) ab->comm);
//	ZHASH("folders");
	free(ab);
}

void do_abook_get(void) {
	char zbuf[PATH_LEN],*ptr;
	
	check_prefs_dir();
	sprintf(zbuf,PREFS_DIR"/%s/abook",current.email);
	ptr = show_abook_l(zbuf);
	if (!ptr) ZHASH("abook_list");
	else add2hash("abook_list",ptr);
	free(ptr);
}

void do_abook_add(void) {
	char zbuf[PATH_LEN];
	abook_t *ab;
	
	add2hash("action",SELF"?cf=8");
	check_prefs_dir();
	sprintf(zbuf,PREFS_DIR"/%s/abook",current.email);
	if (current.adata.edit) {
		add2hash("ab_edit",current.adata.edit);
		ab = get_record(zbuf,current.adata.edit);
		add2hash("cab_name",(char *)ab->name);
		add2hash("cab_email",ab->email);
		add2hash("cab_nick",(char *)ab->nick);
		add2hash("cab_dtl",ab->dtel);
		add2hash("cab_rtl",ab->rtel);
		add2hash("cab_mtl",ab->mtel);
		add2hash("cab_radr", (char *) ab->raddr);
		add2hash("cab_dadr", (char *) ab->daddr);
		add2hash("cab_comm", (char *) ab->comm);
	
	} else {
		ZHASH("ab_edit"), ZHASH("cab_name"), ZHASH("cab_email");
		ZHASH("cab_nick"), ZHASH("cab_dtl"), ZHASH("cab_rtl");
		ZHASH("cab_mtl"), ZHASH("cab_radr"), ZHASH("cab_dadr");
		ZHASH("cab_comm");
	}
	//ZHASH("folders");
}


abook_t *alloc_abook(void) {
	abook_t *ab;

	ab = (abook_t *) malloc(sizeof(abook_t));
	if (!ab) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}
	*ab->name = *ab->nick = *ab->email = 0;
	*ab->rtel = *ab->mtel = *ab->dtel = 0;
	*ab->daddr = *ab->raddr = *ab->comm = 0;
	return ab;
}


