#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>

#include "fcgi_stdio.h"

#include "message.h"
#include "env.h"
#include "wmail.h"
#include "session.h"
#include "global.h"
#include "params.h"
#include "md5.h"
#include "utils.h"
#include "template.h"

extern struct error_struct error;
extern env_t env;
static char hex[]="0123456789abcdef";
session_t current;
extern fhs_t fhs;

char *serialize_session(session_t *s) {
	int min;
	int elen,plen;
	char *str;

	min = sizeof(session_t);
	elen = strlen(s->email);
	plen = strlen(s->password);

	str = (char *) malloc(min + elen + plen);
	if (!str) {
		SETERR(E_NOMEM,NULL);
		return (char *)0;
	}

	sprintf(str,SESSION_FORMAT,s->email,s->password,s->page);
	return str;
}

char *get_sid(void) {
	struct timeval tv;
	char *sid_s,*sid,*ip = DEF_IP;
	unsigned char digest[21];
	MD5_CTX context;

	ip = (env.ip) ? env.ip : ip;
	gettimeofday(&tv, NULL);	

	sid_s = (char *) malloc(sizeof(char) * 32);
	sid = (char *) calloc(1,sizeof(char) * SID_SIZE);
	if (!sid_s || ! sid) {
		SETERR(E_NOMEM,NULL);
		return (char *) 0;
	}
	
	sprintf(sid_s,"%.15s%ld%ld",ip,tv.tv_sec,(long int)tv.tv_usec);

	MD5Init(&context);
	MD5Update(&context, (unsigned char *)sid_s, strlen(sid_s));
	MD5Final(digest, &context);
	
	free(sid_s);
	to_ascii(digest,sid);
	return sid;
}

void to_ascii(unsigned char *digest,char *sid) {
	int j;
	unsigned char h;

	for (j=0; j<16; j++) {
		h = digest[j] >> 4;
		sid[2*j] = hex[h];
		h = digest[j] & 0x0f;
		sid[(2*j)+1] = hex[h];
	}
}

void new_session(void) {
	current.sid = get_sid();
	read_prefs();
	//set_ll();
}

void drop_session(void) {
	char buf[PATH_LEN];

	if (current.sid) {
		sprintf(buf,"%s/sess_%s",SESSION_DIR,current.sid);
		unlink(buf);
	}
	current.authenticated = 0;
	flush_session();
}

void flush_session(void) {
	FREE_IT(current.sid)
	FREE_IT(current.email)
	FREE_IT(current.password)
	FREE_IT(current.path)
	FREE_IT(current.ll)
	//FREE_IT(current.data.abook)
	free_prefs(&current.prefs);
}
void zero_session(void) {
	current.sid = NULL;
	current.page = 0;
	current.sf = 0;
	current.authenticated = 0;
	current.msg = NULL;
	current.todo = DO_NOTHING;

	/* FIXME: unions NULL's only */
	current.adata.abarr = NULL;
	current.adata.abarr_len = 0;
	current.adata.msg = current.adata.edit = NULL;
	current.adata.edit_what = NULL;

	bzero(&current.fdata,sizeof(fidata_whole_t));

	current.data.abook = NULL;
	current.data.marr = NULL;
	current.data.marr_len = 0;
	current.data.mark = 0;
	current.data.s = current.data.e = 0;
	current.data.copyfolder = NULL;
	current.data.cfolder = NULL;

	current.cdata.todel = NULL;
	current.cdata.copy = NULL;
	current.cdata.prio = 0;
	current.cdata.body = NULL;
	current.cdata.isdraft = 0;
	current.cdata.ra = 0;
	/* END FIXME */

	current.rmsg = current.fmsg = NULL;
	current.ff = FF_ISFOLDER;
	current.email = NULL;
	current.password = NULL;
	current.path = NULL;
	current.folder = DEFAULT_FOLDER;
	current.ll = NULL;
	zero_prefs(&current.prefs);
	
}

void zero_prefs(prefs_t *prefs) {
	prefs->name = NULL;
	prefs->sign = NULL;
	prefs->pp = DEFAULT_PERPAGE;
	prefs->otd = 0;
	prefs->sby = SBY_DATE;
	prefs->sby_order = SBY_ORDER_ASC;
	prefs->uploaddir = NULL;
}

/*
void set_prefs(prefs_t *prefs) {
	prefs->name = (unsigned char *) strdup("Александр Мироч");
	prefs->pp = DEFAULT_PERPAGE;
}
*/

void free_prefs(prefs_t *prefs) {
	FREE_IT(prefs->name)
	FREE_IT(prefs->sign)
	FREE_IT(prefs->uploaddir)
}

int init_session(void) {
	char buf[PATH_LEN];
	char *sid;
	int fd;
	int rv;

	//session_cleaner();
	zero_session();
	// check_for_existing_session()
	sid = read_cookie();
	if (!sid) {
		new_session();
		return 0;
	}

	sprintf(buf,"%s/sess_%s",SESSION_DIR,sid);
	fd = open(buf,O_RDONLY);
	if (!fd) {
		new_session();
		return 0;
	}

	rv = read(fd,buf,255);
	if (rv > 255) {
		new_session();
		return 0;
	}
	close(fd);
	buf[rv] = 0;

	if (unserialize_session(buf,&current) < 0) {
		new_session();
		return 0;
	}

	fhs.size = 0;

	read_prefs();
	//set_ll();
	current.sid = sid;
	current.authenticated = 1;
	
	return 0;
}

void save_session(void) {
	char sfile[256];
	char *szed;
	int fd;

	if (!current.sid) return;
	sprintf(sfile,"%s/sess_%s",SESSION_DIR,current.sid);

	szed = serialize_session(&current);
	if (!szed) return;

	fd = open(sfile,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR);
	if (!fd) {
		free(szed);
		SETERR(E_FILE,sfile);
		return;
	}
	write(fd,szed,strlen(szed));
	close(fd);
	free(szed);

	set_cookie(current.sid);
	return;
}

void check_prefs_dir(void) {
	char mpath[PATH_LEN];
	struct stat st;

	sprintf(mpath,PREFS_DIR"/%s",current.email);
	if (stat(mpath,&st) < 0) {
		if (errno != ENOENT) {
			SETERR(E_FILE,mpath);
			return ;
		}
		if (mkdir(mpath,0700) < 0) {
			SETERR(E_FILE,mpath);
			return ;
		}
	}
}

FILE *get_prefs_file(char *v) {
	FILE *fp;
	char mpath[PATH_LEN];

	check_prefs_dir();
	sprintf(mpath,PREFS_DIR"/%s/prefs",current.email);
	fp = fopen(mpath,v);
	if (!fp) {
		if (errno != ENOENT) SETERR(E_FILE,mpath);
		return NULL;
	}

	return fp;
}

#define LLBUF	(TIME_SIZE + 32)

void remember(char *ip) {
	char mpath[PATH_LEN];
	char tbuf[TIME_SIZE];
	char buf[LLBUF],*ptr = NULL;
	FILE *fp;

	if (!current.sid) return;
	check_prefs_dir();
	sprintf(mpath,PREFS_DIR"/%s/ll",current.email);
	fp = fopen(mpath,"r");
	if (!fp) {
		if (errno != ENOENT) {
			SETERR(E_FILE,mpath);
			return;
		}
	} else {
		ptr = fgets(buf,LLBUF,fp);
		fclose(fp);
	}
	fp = fopen(mpath,"w+");
	if (!fp) {
		if (errno != ENOENT) SETERR(E_FILE,mpath);
		return;
	}

	get_time(tbuf,0,"%H:%M %d.%m.%Y");
	fprintf(fp,"%s [%s]\n",ip,tbuf);
	if (ptr) fprintf(fp,"%s",ptr);
	fclose(fp);
}


void set_ll(void) {
	char mpath[PATH_LEN];
	char buf[LLBUF],buf2[LLBUF];
	char *ptr,*tp;
	FILE *fp;

	check_prefs_dir();
	sprintf(mpath,PREFS_DIR"/%s/ll",current.email);
	fp = fopen(mpath,"r");
	if (!fp) {
		if (errno != ENOENT) SETERR(E_FILE,mpath);
		return;
	}

	ptr = fgets(buf,LLBUF,fp);
	if (!ptr) {
		SETWARN(E_FILE,mpath);
		return;
	}

	tp = fgets(buf2,LLBUF,fp);
	if (!tp) {
		current.ll = strdup(ptr);	
	} else current.ll = strdup(tp);

	fclose(fp);
}


void save_prefs(void) {
	FILE *fp;

	fp = get_prefs_file("w+");
	fprintf(fp,"%s\n",(char *) current.prefs.name,fp);
	fprintf(fp,"%d\n",current.prefs.pp);
	fprintf(fp,"%d\n",current.prefs.otd);
	fprintf(fp,"%c\n",current.prefs.sby);
	fprintf(fp,"%c\n",current.prefs.sby_order);
	fprintf(fp,"%s",current.prefs.sign);
	fclose(fp);
}


static int gbuf(char *buf,FILE *fp) {
	if (fgets(buf,MSG_LINE_SIZE,fp) == NULL) {
		fclose(fp);
		return 0;
	}
	if (is_blank(buf)) {
		fclose(fp);
		return 0;
	}
	buf[strlen(buf)-1] = 0;
	return 1;
}

void read_prefs(void) {
	FILE *fp;
	char buf[MSG_LINE_SIZE];
	char *sign_buf;
	size_t sz;

	fp = get_prefs_file("r");
	if (!fp) return;

	if (!gbuf(buf,fp)) return;
	current.prefs.name = (unsigned char *) strdup(buf);

	if (!gbuf(buf,fp)) return;
	current.prefs.pp = atoi(buf);

	if (!gbuf(buf,fp)) return;
	current.prefs.otd = atoi(buf);
	
	if (!gbuf(buf,fp)) return;
	current.prefs.sby = *buf;
	
	if (!gbuf(buf,fp)) return;
	current.prefs.sby_order = *buf;


	sign_buf = (char *) malloc(sizeof(char) * MAX_SIGN);
	if (!sign_buf) {
		SETERR(E_NOMEM,NULL);
		return;
	} 

	sz = fread(sign_buf,sizeof(*sign_buf),MAX_SIGN,fp);
	if (!sz) {
		fclose(fp);
		free(sign_buf);
		return;
	}
	sign_buf[sz] = 0;
	current.prefs.sign = (unsigned char *)sign_buf;
	fclose(fp);
	return;
}

int unserialize_session(char *ptr,session_t *s) {
	char *tb;

	tb = ptr;
	if (!s) return -1;
	
	ptr = strchr(tb,'|');
	if (!ptr) return -1;
	*ptr = 0;
	++ptr;
	s->email = strdup(tb);

	tb = ptr;
	ptr = strchr(tb,'|');
	if (!ptr) return -1;
	*ptr = 0;
	++ptr;
	s->password = strdup(tb);

	return 0;
}


static void delete_upload(char *dname) {
	DIR *dir;
	struct dirent *de;
	char buf[PATH_LEN];

	if ((dir = opendir(dname)) == NULL) {
		SETWARN(E_OPENDIR,dname);
		return;
	}

	while ((de = readdir(dir)) != NULL) {
		if (de->d_name[0] == '.') continue;

		sprintf(buf,"%s/%s",dname,de->d_name);
		if (unlink(buf) < 0) {
			SETWARN(E_FILE,buf);
		}
	}
	
	closedir(dir);
}

static void run_cleaner(time_t tm) {
	DIR *dir;
	struct dirent *de;
	char buf[PATH_LEN];
	struct stat st;

	if ((dir = opendir(SESSION_DIR)) < 0) {
		SETWARN(E_OPENDIR,SESSION_DIR);
		return;
	}

	while ((de = readdir(dir)) != NULL) {
		if (de->d_name[0] == '.') {
			if (!de->d_name[1] || de->d_name[1] == '.') continue;
			if (!strcmp(de->d_name,SPECIAL_FILE)) continue;
			sprintf(buf,SESSION_DIR"/%s",de->d_name);
			delete_upload(buf);
			if (rmdir(buf) < 0) {
				SETWARN(E_FILE,buf);
			}
		} else {
			sprintf(buf,SESSION_DIR"/%s",de->d_name);
			if (stat(buf,&st) < 0) {
				SETWARN(E_FILE,buf);
				continue;
			}
			if (st.st_mtime + SESSION_EXPIRES < tm) {
				if (unlink(buf) < 0) {
					SETWARN(E_FILE,buf);
					continue;	
				}
			}
		}
	}
	closedir(dir);
}

void session_cleaner(void) {
	time_t tm,tm_p;
	int v;

	if (current.page == PAGE_RESEND ||
		current.page == PAGE_COMPOSE ||
			current.page == PAGE_UPLOAD_RESULT) return;	

	if (time(&tm) < 0) return;
	tm_p = tm & 0xff;
	
	v = CLEAN_PROB * 0xff;
	if (tm_p < v) {
		run_cleaner(tm);
	}
}

void do_page_prefs(void) {
	char zbuf[PATH_LEN];
	char *vals[] = { "cpr_d","cpr_f", "cpr_s", "cpr_z" };
	int i = 0,idx;
	

	add2hash("action",SELF);
	sprintf(zbuf,"%d",current.prefs.pp);
	add2hash("cpr_pp",zbuf);
	if (current.prefs.name) add2hash("cpr_name",(char *)current.prefs.name);
	else ZHASH("cpr_name");
	if (current.prefs.sign) add2hash("cpr_sign",(char *)current.prefs.sign);
	else ZHASH("cpr_sign");

	if (current.prefs.otd) add2hash("cpr_otd","checked");
	else ZHASH("cpr_otd");

	if (current.prefs.sby_order == SBY_ORDER_DESC) add2hash("cpr_sbyo","checked");
	else ZHASH("cpr_sbyo");

	switch (current.prefs.sby) {
		case SBY_DATE: idx = 0;break;
		case SBY_FROM: idx = 1;break;
		case SBY_SUBJ: idx = 2;break;
		case SBY_SIZE: idx = 3;break;
		default: idx = -1;break;
	}

	for (i = 0; i < sizeof(vals)/sizeof(char *); ++i) {
		if (i == idx) {
			add2hash(vals[i],"selected");
		} else ZHASH(vals[i]);
	}
}
