#include <stdlib.h>
#include <locale.h>
#include "fcgi_stdio.h"
//#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <regex.h>
#include <time.h>
#include <gdbm.h>

#include "wmail.h"
#include "template.h"
#include "buffers.h"
#include "driver.h"
#include "session.h"
#include "env.h"
#include "utils.h"
#include "compose.h"
#include "filter.h"
#include "parse.h"



filter_t *filters = NULL;
fhs_t fhs;
extern env_t env;
extern session_t current;
extern driver_t *drv;
extern driver_t fs_drv;
short body_is = 0;
extern header_t headers[];


static void set_fhs(filter_t *);

static void set_fhs(filter_t *f) {
	int i = 0;

	if (f->type == FTYPE_HEADER) {
		for (; i < NUM_FIELDS; ++i) {
			if (!f->hdr[i].header) continue;
			fhs.fheaders[fhs.size++] = f->hdr[i].header;
			fhs.fheaders[fhs.size] = 0;
		}
	} else if (f->type == FTYPE_BODY) body_is = 1;
}

void add_filter(filter_t *f) {
	filter_t *itf;

	f->next = NULL;
	if (!filters) {
		set_fhs(f);
		filters = f;
		return;
	}

	for (itf = filters; itf->next; itf = itf->next) { }
	set_fhs(f);
	itf->next = f;
}

void clean_filters(void) {
	filter_t *f,*pf = NULL;

	for (f = filters; f; f = f->next) {
		if (pf) free_filter(pf);	
		pf = f;
	}
	if (pf) free_filter(pf);	
	filters = NULL;
	body_is = 0;
}

	
	

int if_match(char *t,char *r) {
	int rv;
	regex_t preg;

	rv = regcomp(&preg,r,REG_EXTENDED|REG_NOSUB);
	if (rv) {
		SETERR(E_FILE,NULL);
		return 0;
	}

	rv = regexec(&preg,t,0,NULL,0);
	regfree(&preg);

	if (!rv) return 1;
	return 0;
}

char *do_action(char *path, int type, void *data) {
	char *ptr;
	char *npath;
	long flags;
//	printf("Content-type:text/html\n\n");
//	printf("doing action %d %s(%s)<br>",type,data,path);

	// TODO: switch type

	ptr = strrchr(path,'/');
	if (!ptr) return NULL;
	++ptr;

	//return strdup(path);
	//print_hello();
	switch (type) {
		case FACT_MOVE:
			if (!data) return NULL;
			npath = (char *) malloc(sizeof (char) * PATH_LEN);
			if (!npath) {
				SETERR(E_NOMEM,NULL);
				return NULL;
			}
			//	printf("wiill copy %s to %s<br>",path,data);	
			if (drv->copy_tofolder(path,ptr,(char *)data,&npath)) {
				SETWARN(E_FILE,path);
			//	free(npath);
				return npath;
			}
			//	printf("wiill dely %s to %s<br>",path,ptr);	
			if (drv->delete_msg(path,NULL) < 0) {
				SETWARN(E_FILE,path);
			//	free(npath);
				return NULL;
			}
			return npath;
		case FACT_COPY:
			if (!data) return NULL;
			npath = (char *) malloc(sizeof (char) * PATH_LEN);
			if (!npath) {
				SETERR(E_NOMEM,NULL);
				return NULL;
			}
			if (drv->copy_tofolder(path,ptr,(char *)data,&npath)) {
				SETWARN(E_FILE,path);
			//	free(npath);
				return NULL;
			}
			return npath;
		case FACT_DEL:
			npath = (char *) malloc(sizeof (char) * PATH_LEN);
			if (!npath) {
				SETERR(E_NOMEM,NULL);
				return NULL;
			}
			/* Copy to temporary dir if anyone wants to resend it */
			if (drv->delete_msg(path,NULL) < 0) {
				SETWARN(E_FILE,path);
				return NULL;
			}
			return NULL;
		case FACT_SENDMSG:
			if (!data) return NULL;
			sendmail(path,"'Wmail Resend Agent <cocacola@cocacola.ru>'",data);
			return strdup(path);
		case FACT_MARK_PRIO:
			flags = drv->get_flags(ptr);
			drv->set_flags(path,ptr,flags | FL_FLAGGED,&npath);
			return npath;
		
	}
	return strdup(path);
}

char *do_actions(char *path, int *types, void **datas) {
	int i = 0, sm = -1;
	int type;
	void *data;
	char *pp = path, *np;

	//printf("DI ACTIONS<br>");
	for (; i < NUM_FIELD_ACTS; ++i) {
		type = types[i];
		if (type == FACT_IGNORE) continue;
	//	printf("i=%d type=%d<br>",i,type);
		if (type == FACT_DEL) {
			sm = i;
			continue;
		}
		data = datas[i];
	//	printf("%d do action %d(%s) %s<br>",i,type,(data)?data:"nodata",pp);
		np = do_action(pp,type,data);
	//	if (np)
	//		printf("%d ret %s<br>",i,np);
		if (pp && pp != path) free(pp);
		if (np) pp = np;
	}
	//printf("sm=%d<br>",sm);
	if (sm != -1) {
		data = datas[sm];
	//	printf("%d do delayed action %d(%s) %s<br>",sm,type,(data)?data:"nodata",pp);
		np = do_action(pp,FACT_DEL,data);
	//	if (np)
	//		printf("%d ret %s<br>",i,np);
		if (pp && pp != path) free(pp);
		if (np) free(np);
		return NULL;
		
	}

//	if (pp && pp != path) free(pp);
	return np;
}

int apply_filters(char *path,char **hdrs,char **fdarr,int flen) {
	char sbuf[PATH_LEN],*ptr;
	filter_t *f;
	int h, i, p;
	int matched;
	char *pp = path, *np;
	char *tmp_buf;
	int endloop = 0;
	int cf;

	ptr = sbuf;
	for (f = filters; f && !endloop; f = f->next) {
		cf = 0;
		if (fdarr) {
			for (p = 0; p < flen; ++p) {
				if (f->key && *f->key) {
					if (!strcmp(fdarr[p],f->key)) {
						cf = 1;
						break;
					}
				}
			}
			if (!cf) continue;
		}
		switch (f->type) {
			case FTYPE_HEADER:
				for (i = 0; i < NUM_FIELDS; ++i) {
					h = f->hdr[i].header;
					if (!h) continue;
					if (!hdrs[h]) {
						break;
					}
					/* Will free in gb */
					tmp_buf = strdup(hdrs[h]);
					parse_subject_x(&ptr,tmp_buf);
		
					//printf("Sbuf=%s<br>",ptr);
					//matched = if_match(hdrs[h],f->hdr[i].regex);
					matched = if_match(ptr,f->hdr[i].regex);
					matched = (f->hdr[i].rev) ? !matched : matched;
					if (matched) {
						if (!f->andor) {
							np = do_actions(pp,f->action_type,f->action_data);
							if (pp && pp != path) free(pp);
							pp = np;
							if (!pp) endloop = 1;
							break;

						}
					} else if (f->andor) {
						break;
					} 
				}
				if (f->andor && matched) {
					np = do_actions(pp,f->action_type,f->action_data);
					if (pp && pp != path) free(pp);
					pp = np;
					if (!pp) endloop = 1;
				}
				break;
			case FTYPE_BODY:
			case FTYPE_HPRES:
				break;
			default:
				SETWARN(E_PARSE,NULL);
				return 0;
		}
	}
	if (pp && pp != path) free(pp);
	return 0;
}

int do_filters(char *path,char **fdarr,int flen) {
	fdata_t *fdata;
	void *stream;
	int *mh,x;
	char *hdrs[MAX_FILTERS];

	fdata = drv->get_msgstream(path,NULL);
        if (!fdata || !fdata->stream) {
                SETERR(E_FILE,path);
                return 0;
        }
        stream = fdata->stream;
	mh = fhs.fheaders;
	for (x = 0; x < MAX_FILTERS; ++x) hdrs[x] = NULL;
	read_headers(stream,hdrs,mh,fhs.size);

	/* TODO: read body and process body filters */
	if (body_is) {
		
	}
	drv->finish(fdata);

	return apply_filters(path,hdrs,fdarr,flen);
}

void free_filter(filter_t *f) {
	int j = 0;
	if (f->action_data) {
		for (; j < NUM_FIELD_ACTS; ++j) 
			if (f->action_data[j]) free(f->action_data[j]);
	}
	if (f->type == FTYPE_HEADER) {
		for (j = 0; j < NUM_FIELDS; ++j) {
			if (f->hdr[j].regex) free(f->hdr[j].regex);
		}
	}
	free(f);
}

filter_t *alloc_filter(void) {
	filter_t *f;
	int i = 0;

	f = (filter_t *) malloc(sizeof(filter_t));
	if (!f) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}

	f->is_reverse = 0;
	f->andor = 0;
	for (; i < NUM_FIELD_ACTS; ++i) {
		f->action_type[i] = FACT_IGNORE;
		f->action_data[i] = 0;
	}
	f->next = NULL;
	bzero(&f->hdr,sizeof(f->hdr));
	bzero(f->key,sizeof(f->key));
	return f;
}

void free_fidata(fidata_t *fidata) {
/*	fpart_t *fp;
	int i = 0;

	fp = fidata->fp;
	if (fp->field) free(fp->field);
	if (fp->value) free(fp->value);
	for (; i < NUM_FIELD_ACTS ; ++i)
		if (fidata->act_data[i]) free(fidata->act_data[i]);
*/
	bzero(fidata,sizeof(fidata_t));
}

void f_setkey(fidata_t *fidata) {
	time_t tv;
	unsigned int key;
	
	if (time(&tv) == (time_t) -1) {
                SETWARN(E_TIME,NULL);
                return ;
        }

	key = (tv & 0xffffff);
	snprintf(fidata->key,6,"%hu%hu%hu",
		key & 0xff,
		(key >> 8) & 0xff,
		(key >> 16) & 0xff
	);
		
	return;
}

int check_fi_space(char *path) {
	struct stat st;
	unsigned long size;

	if (stat(path,&st) < 0) {
		SETWARN(E_FILE,path);
		return -1;
	}

	size = MAX_FI_SPACE * 1024;
	if (st.st_size > size) return -1;

	return 1;
}

void save_filter(fidata_t *fidata, char *path, int r) {
	GDBM_FILE dbm;
	datum key,data;
	int v = 1, flag = GDBM_INSERT;

	dbm = gdbm_open(path,0,GDBM_WRCREAT,0600,NULL);
	if (!dbm) {
		SETERR(E_DBASE,"Can't open db for filter");
		return;
	}

        gdbm_setopt(dbm,GDBM_FASTMODE,&v,sizeof(int));

	key.dptr = fidata->key;
	key.dsize = sizeof(fidata->key);
	data.dptr = (char *) fidata;
	data.dsize = sizeof(fidata_t);

	if (r) flag = GDBM_REPLACE;
	if (flag == GDBM_INSERT) {
		if (check_fi_space(path) < 0) {
			SETERR(E_DBASE,"Too many space occupied!");
			return;
		}
	}

	if (gdbm_store(dbm,key,data,flag)) {
		SETERR(E_DBASE,"Can't store filter database");
		return;
	}

	gdbm_close(dbm);
}

int no_acts(fidata_t *fidata) {
	int i , found = 0;
	fpart_t *fp;

	for (i = 0; i < NUM_FIELDS; ++i) {
		fp = &fidata->fp[i];
		if (fp->field) {
			found = 1;
			break;
		}
	}
	if (!found) return 1;

	for (i = 0; i < NUM_FIELD_ACTS ; ++i) {
		if (fidata->act[i]) {
			return 0;
		}
	}
	return 1;
}

void do_filter_add(void) {
	char zbuf[PATH_LEN];

	check_prefs_dir();
	sprintf(zbuf,PREFS_DIR"/%s/filters",current.email);

	if (no_acts(&FT)) return;

	if (FT.key && FT.key[0]) {
		save_filter(&FT,zbuf,1);
	} else {
		f_setkey(&FT);
		save_filter(&FT,zbuf,0);
	}

	return;

}

fidata_t **collect_filters(char *path) {
	register int i = 0;
	GDBM_FILE dbm;
	datum key,data;
	char *sptr = NULL;
	fidata_t **fds;
	
	dbm = gdbm_open(path,0,GDBM_READER,0600,NULL);
	if (!dbm) return NULL;
	
	fds = (fidata_t **) malloc(sizeof(fidata_t *) * (MAX_FILTERS + 1));
	if (!fds) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}

	for (key = gdbm_firstkey(dbm); key.dptr; key = gdbm_nextkey(dbm,key), ++i) {
		data = gdbm_fetch(dbm,key);
		if (!data.dptr) {
			free(key.dptr);
			continue;
		}
		fds[i] = (fidata_t *) data.dptr;
		if (i > MAX_FILTERS) break;
		if (sptr) free(sptr);
		sptr = key.dptr;
	}

	free(sptr);
	fds[i] = NULL;
	gdbm_close(dbm);
	return fds;
}


int sby_order(const void *p1, const void *p2) {
        fidata_t **a1,**a2;
	unsigned short o1,o2;

	a1 = (fidata_t **) p1;
	a2 = (fidata_t **) p2;
	if (!a1 || !a2) return 0;

	o1 = (*a1)->order;
	o2 = (*a2)->order;
        return (o1 > o2) ? 1 : 0;
}

char *acts[] = {
	"ничего не делать",
	"переместить в",
	"копировать в",
	"удалить",
	"переслать на",
	"пометить важным"	
};

char *form_act(char *buf,fidata_t *fidata,unsigned long len) {
	int i = 0;
	unsigned short act;
	unsigned long off = 0;
	unsigned char *actdata;
	char *tp;

	for (; i < NUM_FIELD_ACTS; ++i) {
		if (off > len + 20) {
			snprintf(buf + off, 4, " ...");
			break;
		}
		act = fidata->act[i];
		actdata = fidata->act_data[i];
		if (!act) continue;
		if (actdata && *actdata && *actdata != '0') {
			tp = do_utf7((char *)actdata);
			off += sprintf(buf + off,"%s <b>%s</b><br>",acts[act],alias(tp));
			free(tp);
		} else {
			off += sprintf(buf + off ,"%s<br>",acts[act]);
		}
	}
	return buf;
}

char *form_msg(char *buf,fidata_t *fidata,unsigned long len) {
	fpart_t *fp;
	int i = 0, ft = 1;
	unsigned long off = 0;

	for (; i < NUM_FIELDS; ++i) {
		fp = (fpart_t *) &fidata->fp[i];
		if (off > len + 20) {
			snprintf(buf + off, 4, " ...");
			break;
		}
		if (!fp->field) continue;
		if (ft) {
			off += sprintf(buf + off,"Поле \"<b>%s:</b>\" %s %s",
				headers[fp->field].name,
				fp->yno ? "содержит" : "не содержит",
				fp->value);
		}
		else {
			off += sprintf (buf + off, "<br/>&nbsp;&nbsp;%s поле \"<b>%s:</b>\" %s %s",
				fidata->andor ? "и" : "или",
				headers[fp->field].name,
				fp->yno ? "содержит" : "не содержит",
				fp->value);
		}
		ft = 0;
	}
	if (ft) {
		SETERR(E_PARSE,"Filter");
		return NULL;
	}
	return buf;
}


unsigned int f2buf(char *buf, fidata_t *fidata, int parity) {
	char msg[MSG_BUF],act[MSG_BUF];

	sprintf(buf,"<tr class=\"%s\"><td class=\"fi0\">\
<input name=\"fdarr\" type=\"checkbox\" value=\"%s\">%s\
&nbsp;<a href=\""SELF"?cf=16&fedit=%s\"><img src=\"/images/edit.png\" /></a>\
</td><td class=\"fiord\">%d</td><td class=\"fi1\">%s</td><td class=\"fi2\">%s</td>\
	</tr>",
	parity ? "even" : "odd",
	fidata->key,
	fidata->status ? "<img src=\"/images/blacklist.png\" />":"",
	fidata->key,
	fidata->order, 
	form_msg(msg,fidata,MSG_BUF),
	form_act(act,fidata,MSG_BUF)
	);
	return strlen(buf);
}
int inline fcount(struct fidata **fds) {
        register int i = 0;

        for (; fds[i]; ++i) { ; }
        return i;
};

char *show_filters(char *path) {
	fidata_t **fds;
	int c,i = 0;
	char *sbuf,*buf;

	fds = collect_filters(path);
	if (!fds) return NULL;

	c = fcount(fds);
	qsort(fds,c,sizeof(fidata_t *),sby_order);

	sbuf = buf = (char *) malloc(sizeof(char) * MAX_FILTERS * sizeof(fidata_t));
	if (!sbuf) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}
	*sbuf = 0;

	for (; i < c; ++i) {
		buf += f2buf(buf, fds[i], i % 2);
		free(fds[i]);		
	}
	free(fds);

	return (sbuf && *sbuf) ? sbuf : NULL;
}

fidata_t *get_filter(char *path, char *mykey) {
	datum key,data;
        GDBM_FILE dbm;

        dbm = gdbm_open(path,0,GDBM_READER,0600,NULL);
        if (!dbm) {
                SETERR(E_DBASE,ABERR);
                return NULL;
        }


        key.dptr = mykey;
        key.dsize = sizeof(FT.key);

        data = gdbm_fetch(dbm,key);
        if (!data.dptr) {
                gdbm_close(dbm);
                SETERR(E_DBASE,ABERR);
                return NULL;
        }
        gdbm_close(dbm);

        return (fidata_t *) data.dptr;
}
/*
int inline checked(fidata_t *fidata, char *f) {
	int j = 0;
	fpart_t *fp;

	for (; j < NUM_FIELDS; ++j) {
		fp = &fidata->fp[j];
		if (!fp || !fp->field) continue;
		if (!strcmp((char *) fp->field, f)) return 1;
	}
	return 0;
}
*/
#define FIDATA(i) (fidata && fidata->fp && fidata->fp[i].field)

void f_addhash(fidata_t *fidata) {
	register int i = 0,j;
	char *buf, tkey[8];
	unsigned long off = 0;
	int ok = 0;

	bzero(tkey,8);
	buf = (char *) malloc(sizeof(char) * 65535 * NUM_FIELDS);
	if (!buf) {
		SETERR(E_NOMEM,NULL);
		return ;
	}
	for (; i < NUM_FIELDS; ++i) {
		ok = FIDATA(i);
		off += sprintf(buf + off,"<tr><td width=\"200\" class=\"cmes\" align=\"right\">Поле</td><td>\
			<select name=\"field%d\"><option value=\"0\">-</option>",i);
		for (j = 1; j < MAX_HDR; ++j) {
			off += sprintf(buf + off, "<option value=\"%d\" %s >%s</option>",
			j,
			(ok && fidata->fp[i].field == j) ? "selected" : "",
			headers[j].name);
		}
		off += sprintf(buf + off,"</select></td><td class=\"cmes\">\
			<select name=\"yno%d\">\
				<option value=\"yes\" %s>Содержит</option>\
				<option value=\"no\" %s>Не содержит</option>\
			</select></td><td align=\"left\">\
		<input type=\"select\" name=\"fact%d\" value=\"%s\" size=\"32\">\
		</td></tr>",i,
		(ok && fidata->fp[i].yno == 1) ? "selected" : "",
		(ok && fidata->fp[i].yno == 0) ? "selected" : "",
		i,
		ok ? (char *) fidata->fp[i].value : "" 	);
	}
	add2hash("f_div",buf);
	for (i = 0; i <= MAX_FACT; ++i) {
		sprintf(tkey,"f_do%d",i);
		if (!fidata) {
			if (!i) {
				add2hash(tkey,"selected");
				continue;
			}
			ZHASH(tkey);
		} else {
			if (fidata->act[0] == i) {
				add2hash(tkey,"selected");
			} else ZHASH(tkey);
		}
	}

	if (fidata) {
		if (fidata->act[1] == FACT_SENDMSG) {
			add2hash("f_data",(char *)fidata->act_data[1]);
		} else ZHASH("f_data");

		if (fidata->act[2] == FACT_MARK_PRIO) {
			add2hash("f_prio","checked");
		} else ZHASH("f_prio");
		
		if (fidata->order) {
			snprintf(tkey,3,"%hu",fidata->order);
			add2hash("f_order",tkey);
		} else add2hash("f_order","0");

		if (fidata->andor) {
			add2hash("f_andora","checked");
			ZHASH("f_andoro");
		} else {
			add2hash("f_andoro","checked");
			ZHASH("f_andora");
		}

	} else {
		add2hash("f_andora","checked");
		ZHASH("f_andoro");
		ZHASH("f_data");
		ZHASH("f_prio");
		ZHASH("f_order");
	}

	free(buf);
	return;
}


void do_filter_show_form(void) {
	char zbuf[PATH_LEN];
	fidata_t *fidata;


	if (FT.key && FT.key[0]) {
		sprintf(zbuf,PREFS_DIR"/%s/filters",current.email);
		fidata = get_filter(zbuf,FT.key);
		f_addhash(fidata);
		sprintf(zbuf,SELF"?cf=15&fedit=%s",FT.key);
	} else {
		f_addhash(NULL);
		sprintf(zbuf,SELF"?cf=15");

	}
	add2hash("action",zbuf);
}

void do_filter_view(void) {
	char zbuf[PATH_LEN];
	char *ptr;

	check_prefs_dir();
	sprintf(zbuf,PREFS_DIR"/%s/filters",current.email);
	ptr = show_filters(zbuf);
	if (!ptr) 
		ZHASH("filter_list");
	else {
		add2hash("filter_list",ptr);
		free(ptr);
	}

	sprintf(zbuf,SELF"?cf=16");
	add2hash("filter_add",zbuf);

	sprintf(zbuf,SELF"?cf=15");
	add2hash("action",zbuf);
}

void fd_del(char *path, char *mykey) {
	datum key;
	GDBM_FILE dbm;
	
	dbm = gdbm_open(path,0,GDBM_WRITER,0600,NULL);
	if (!dbm) {
		SETERR(E_DBASE,ABERR);
		return;
	}
	
	key.dptr = mykey;
	key.dsize = sizeof(FT.key);

        if (gdbm_delete(dbm,key) < 0) {
		SETERR(E_DBASE,ABERR);
		return;
	}

	gdbm_close(dbm);
}

void fd_switch(char *path, char *mykey) {
	datum key,data;
	GDBM_FILE dbm;
	fidata_t *fidata;
	
	dbm = gdbm_open(path,0,GDBM_WRITER,0600,NULL);
	if (!dbm) {
		SETERR(E_DBASE,ABERR);
		return;
	}
	
	key.dptr = mykey;
	key.dsize = sizeof(FT.key);

	data = gdbm_fetch(dbm,key);
	if (!data.dptr) {
		SETERR(E_DBASE,"Can't fetch from database");
		return;
	}

	fidata = (fidata_t *) data.dptr;
	fidata->status ^= 0x1;

	if (gdbm_store(dbm,key,data,GDBM_REPLACE)) {
		SETERR(E_DBASE,"Can't store filter database");
		return;
	}

	free(data.dptr);
	gdbm_close(dbm);
}



void do_delfilter(void) {
	int i = 0;
	char zbuf[PATH_LEN];

	sprintf(zbuf,PREFS_DIR"/%s/filters",current.email);
	if (!current.fdata.fdarr) return;
	for (; i < current.fdata.fdarr_len; ++i) {
		fd_del(zbuf,current.fdata.fdarr[i]);
	}
	free(current.fdata.fdarr);
}

void do_applyfilter(void) {
	int i = 0, j;
	collect_t *cl,*cmes;
	char *ptr,*pm;

//	print_hello();
//	printf("will apply!");

	if (!current.fdata.fdarr) return;

	cl = drv->collect_folders(list_wr);
	if (!cl) return;

	for (; i < cl->len; ++i) {
		ptr = cl->base[i].name;
	//	printf("v=%s<br>",ptr);
		cmes = drv->collect_messages(ptr,list_wr);
		if (!cmes) {
			free_cl(cl);
			return;
		}
		for (j = 0; j < cmes->len; ++j) {
			pm = cmes->base[j].name;
		//	printf("--m=%s<br>",pm);
			drv->apply_filters(ptr,pm,current.fdata.fdarr,current.fdata.fdarr_len);
		}
		free_cl(cmes);
	}

	free_cl(cl);
//	printf("all ok");

	
//:w	printf("real apply");

}

void do_switchfilter(void) {
	int i = 0;
	char zbuf[PATH_LEN];

//	print_hello();
//	printf("will switch!");
	sprintf(zbuf,PREFS_DIR"/%s/filters",current.email);
	if (!current.fdata.fdarr) return;
	for (; i < current.fdata.fdarr_len; ++i) {
		fd_switch(zbuf,current.fdata.fdarr[i]);
	}
	free(current.fdata.fdarr);
}

