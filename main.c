/*
*
* This program is free software
* 
*
*
*/

#define _GNU_SOURCE

#include <stdlib.h>
#include <locale.h>
#include "fcgi_stdio.h"
//#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>
#include <time.h>

#include "wmail.h"
#include "template.h"
#include "buffers.h"
#include "driver.h"
#include "session.h"
#include "env.h"
#include "utils.h"
#include "compose.h"
#include "filter.h"
#include "params.h"

char root[ROOT_SIZE];
char weare[ROOT_SIZE+16];
struct error_struct error;
extern env_t env;
char hostname[HOSTNAME_LEN];
jmp_buf jenv;
extern int pagedetected;

char *err_arr[] = {
	"OK",
	"Memory alloc error",
	"File problem",
	"Duplicate!",
	"Too long!",
	"Wrong format",
	"Too many drivers",
	"Can't find driver",
	"POST Error",
	"Parse error",
	"Can't open dir",
	"Too many folders",
	"Nested error",
	"Too many messages",
	"Rename error",
	"Message format error",
	"Message parse error",
	"Can't get localtime",
	"Utf error",
	"Database error",
	NULL, NULL
};

extern env_t env;
extern session_t current;
driver_t *drv;
extern driver_t fs_drv;
int debug = 1;

int main(int argc,char *argv[]) {
	unsigned short af = 0;

	error.error = error.line = *error.message = *error.function = *error.file = 0;
	setroot();
	init_buffers(DEFAULT_BUFFERS);

	register_driver(&fs_drv,1);
	drv = select_driver();
	if (!drv) {
		SETERR(E_NODRV,NULL);
	}

	gethostname(hostname,HOSTNAME_LEN);

	init_hash();
	init_gb();

	setlocale(LC_ALL,"");
	drv->init(drv);
	while (FCGI_Accept() >= 0) {
		init_env();
		af = 0;

		if (setjmp(jenv) != 0) {
			SETERR(E_POST,"");
		}
		init_session();
		read_post();
		read_get();
		if (!current.authenticated && current.page == PAGE_LOGIN) {
			if (current.email && current.password)
				current.authenticated = drv->auth(current.email,current.password);
			if (!current.authenticated) {
				add2hash("err_mes",AUTH_FAILED);
				af = 1;
			} 
			else {
				remember(env.ip);
				ZHASH("err_mes");
			}
		}
		if (!current.authenticated) {
			current.page = PAGE_LOGIN;
			print_hello();
			add2hash("x_page",get_page(PAGE_LOGIN));
			add2hash("action",SELF);
			if (!af) ZHASH("err_mes");
		} else {
			current.path = drv->build_path(current.email,NULL,NULL);
			save_session();
			setup_filters();
			do_pre_work();
			if (!current.ll) set_ll();
			if (current.ll) add2hash("last_loginq",current.ll);
			else ZHASH("last_login");
			current.page = detect_page(current.page);
			if (current.page == PAGE_ATTACH) {
				do_cleans();
				continue;
			}

			if (current.page != PAGE_SOURCE) {
				print_hello();
			}

			if (current.page != PAGE_COMPOSE && 
					current.page != PAGE_UPLOAD_RESULT &&
					current.page != PAGE_ABOOK_GET &&
					current.page != PAGE_SOURCE &&
					current.page != PAGE_PRINT) {
				do_template("header");
				do_template("left");
			}
		}
		do_template(get_page(current.page));
		if (current.page != PAGE_COMPOSE && 
				current.page != PAGE_UPLOAD_RESULT &&
				current.page != PAGE_LOGIN && 
				current.page != PAGE_ABOOK_GET &&
				current.page != PAGE_SOURCE &&
				current.page != PAGE_PRINT) {
			do_template("footer");
		}
		flush_buffers();
		do_cleans();
		FCGI_Finish();
	}

	return 0;
}

void setup_filters(void) {
	fidata_t **fds;
	char path[PATH_LEN];
	int c, j = 0;

	check_prefs_dir();
        sprintf(path,PREFS_DIR"/%s/filters",current.email);
	fds = collect_filters(path);
        if (!fds) return;

        c = fcount(fds);
        qsort(fds,c,sizeof(fidata_t *),sby_order);
	for(; j < c; ++j) {
		setup_filter(fds[j]);
		free(fds[j]);
	}
	free(fds);
	setup_spam_filter();

}

void setup_filter(fidata_t *fidata) {
	filter_t *f;
	fpart_t *fp;
	int j = 0;

	if (fidata->status == F_INACTIVE) return;
	//print_hello();
	f = alloc_filter();
	f->type = FTYPE_HEADER;
	f->andor = fidata->andor;
	for (; j < NUM_FIELDS; ++j) {
		fp = &fidata->fp[j];
		if (!fp || !fp->field) continue;

		//printf("SET #%d field=%d<br>",j,fp->field);
		f->hdr[j].header = fp->field;
		f->hdr[j].rev = !fp->yno;
		if (fp->value) {
			f->hdr[j].regex = strdup((char *)fp->value);
		}
	}
	for (j = 0; j < NUM_FIELD_ACTS; ++j) {
		f->action_type[j] = fidata->act[j];
		f->action_data[j] = strdup((char *)fidata->act_data[j]);
	}
	memcpy(f->key,fidata->key,sizeof(fidata->key));

	add_filter(f);
}

void setup_spam_filter(void) {
	filter_t *f;

	//printf("Content-type:text/html\n\n");

	f = alloc_filter();
	f->type = FTYPE_HEADER;
	f->action_type[0] = FACT_MOVE;
	f->action_data[0] = strdup("Spam");

	f->hdr[0].header = H_SPAM_STATUS;
	f->hdr[0].regex = strdup("^[[:space:]]*[yY][Ee][Ss]");

	add_filter(f);
}

void do_delete(void) {
	int i,islast = 0,fneed ;
	unsigned long flags;
	char *msg;
	
	if (!strcmp(current.folder,"Trash")) islast = 1;
	for (i = 0; i < current.data.marr_len; ++i, fneed = 0) {
		msg = current.data.marr[i];
		if (!islast) {
			flags = drv->get_flags(msg);
			if (!(flags & FL_SEEN)) {
				drv->set_flags(NULL,msg,flags | FL_SEEN, NULL);
				msg = seen(msg);
				fneed = 1;
			} 
			if (drv->copy_tofolder(NULL,msg,"Trash",NULL) < 0) {
				if (fneed) free(msg);
				SETWARN(E_FILE,msg);
			}
		}
		if (drv->delete_msg(NULL,msg) < 0) {
			if (fneed) free(msg);
			SETWARN(E_FILE,msg);
		}
		if (fneed) free(msg);
	}
}

static int checker(void) {
	if (!current.data.copyfolder) return -1;

	if (!strcmp(current.data.copyfolder,current.folder)) return -1;
	return 0;
}

void do_move(void) {
	int i;

	if (checker() < 0) return;
	for (i = 0; i < current.data.marr_len; ++i) {
		if (drv->copy_tofolder(NULL,current.data.marr[i],current.data.copyfolder,NULL) < 0) {
				SETWARN(E_FILE,current.data.marr[i]);
				continue;
		}
		if (drv->delete_msg(NULL,current.data.marr[i]) < 0)
				SETWARN(E_FILE,current.data.marr[i]);
	}
}


void do_copy(void) {
	int i;

	if (checker() < 0) return;
	for (i = 0; i < current.data.marr_len; ++i) {
		if (drv->copy_tofolder(NULL,current.data.marr[i],current.data.copyfolder,NULL) < 0)
				SETWARN(E_FILE,current.data.marr[i]);
	}
}

static void set_flag(char *msg,int flag) {
	int flags;

	flags = drv->get_flags(msg);
	drv->set_flags(NULL,msg,flags | flag, NULL);
}

static void set_unflag(char *msg,int flag) {
	int flags;

	flags = drv->get_flags(msg);
	flags &= ~flag;
	drv->set_flags(NULL,msg,flags,NULL);
}


void do_mark(void) {
	int i,flag;
	void (*f)(char *,int);

	switch (current.data.mark) {
		case MARK_SEEN:
			f = set_flag;
			flag = FL_SEEN;
			break;
		case MARK_UNSEEN:
			f = set_unflag;
			flag = FL_SEEN;
			break;
		case MARK_PRIO:
			f = set_flag;
			flag = FL_FLAGGED;
			break;
		case MARK_UNPRIO:
			f = set_unflag;
			flag = FL_FLAGGED;
			break;
		default:
			return;
	}

	for (i = 0; i < current.data.marr_len; ++i) {
		f(current.data.marr[i],flag);
	}
}

void do_saveprefs(void) {
	save_prefs();
}

void do_cfolder(void) {
	char *folder;

	if (!current.data.cfolder) return;
	folder = do_toutf7(current.data.cfolder);
	drv->create_folder(folder);
	free(folder);
}

void do_efolder(void) {
	drv->empty_folder();
}

void do_dfolder(void) {
	drv->drop_folder();
}


void do_draft(void) {
	compose_msg(1);
	current.page = PAGE_DONE;
}

void do_send(void) {
	long flags;
	char *msg;

	compose_msg(0);
	current.page = PAGE_DONE;

	msg = (current.rmsg) ? current.rmsg : current.fmsg;
	if (!msg) return;
	
	flags = drv->get_flags(msg);
	if (current.rmsg && !(flags & FL_REPLIED)) {
		drv->set_flags(NULL,current.rmsg,flags | FL_REPLIED,NULL);
	} else if (current.fmsg && !(flags & FL_PASSED)) {
		drv->set_flags(NULL,current.fmsg,flags | FL_PASSED,NULL);
	}
}

void setup_special_folders(void) {
	if (!drv->special_exists("sent-mail")) drv->create_folder("sent-mail");
	if (!drv->special_exists("Trash")) drv->create_folder("Trash");
	if (!drv->special_exists("Spam")) drv->create_folder("Spam");
	if (!drv->special_exists("Draft")) drv->create_folder("Draft");
}

void do_pre_work(void) {

	setup_special_folders();
	if (current.todo == DO_NOTHING) return ;

	switch (current.todo) {
		case DO_DELETE:
			do_delete();
			break;
		case DO_COPY:
			do_copy();
			break;
		case DO_MOVE:
			do_move();
			break;
		case DO_CFOLDER:
			do_cfolder();
			break;
		case DO_DFOLDER:
			do_dfolder();
			current.folder = strdup(".");
			break;
		case DO_SEND:
			do_send();
			break;
		case DO_EFOLDER:
			do_efolder();
			break;
		case DO_SAVEPREFS:
			do_saveprefs();
			break;
		case DO_DELREC:
			do_delrec();
			break;
		case DO_DRAFT:
			do_draft();
			break;
		case DO_MARK:
			do_mark();
			break;
		case DO_DELFILTER:
			do_delfilter();
			break;
		case DO_APPLYFILTER:
			do_applyfilter();
			break;
		case DO_SWITCHFILTER:
			do_switchfilter();
			break;
		default:
			SETWARN(E_PARSE,NULL);
			return ;
	}
	
	return ;
}


void do_cleans(void) {
	clean_hash();
	flush_session();
	free_gb();
	clean_filters();
	pagedetected = 0;
	session_cleaner();
/*	current.ff = FF_ISFOLDER;
	current.prefs.name = (unsigned char *) strdup("Александр Мироч");
	current.page = 0;
	current.rmsg = current.fmsg = NULL;
	current.msg = NULL;*/
}

void setroot(void) {
	sprintf(weare,"%s/wmail.fcgi",root);
}


void process_request(void) {

	// Get cookie

	// if (!cookie)
	// parse_form && auth && setcookie

	// check_session

	// set page	

	// output headers + body

}


void check_error(void) {
	char *str,*msg;
	//int do_ex = 0;
	FILE *ef;
	char out[TIME_SIZE];
	int b;
	struct tm *tmp;
	time_t tm;

	if (!error.error) return;

//	print_hello();
	switch (error.status) {
		case E_WARNING:
			str = "WARNING";
			break;
		case E_NOTICE:
			str = "NOTICE";
			break;
		case E_FAULT:
			str = "FATAL ERROR";
			break;
		default:
			str = "UNKNOWN ERROR";
			break;
	}

	ef = fopen(ERROR_LOG,"a");
	if (!ef) exit(1);
	time(&tm);
	if (!tm) exit(1);
	tmp = localtime(&tm);
	if (!tmp) exit(1);
	b = strftime(out,TIME_SIZE,"%a, %d %b %Y %H:%M:%S",tmp);
	if (!b) exit(1);
	out[b] = 0;

	msg = (error.message) ? error.message : NULL;
	fprintf(ef,"%s [%s]: %s: %s (%s)\n",out,env.ip,str,err_arr[error.error],msg);
	if (debug) {
		fprintf(ef,"File %s: %d in function %s\n",error.file,error.line,error.function);
		fprintf(ef,"folder %s  msg %s   page %d\n",
		(current.folder) ? current.folder : "unknown",
		(current.msg) ? current.msg : "unknown",
		(current.page) ? current.page : -1);
	}
	if (error.status == E_FAULT) {
		print_hello();
		printf("<center><strong> Во время операции с вашим почтовым ящиком произошла ошибка. <br>Для выяснения причин ошибки обратитесть в службу технической поддержки </strong>");
		exit(1);
	}
	
	fclose(ef);

}

void set_error(int e,int s,const char *fl,const char *f,int line,char *m) {
	int len;

	bzero(&error,sizeof(error));
	error.error = e;
	error.status = (!s) ? E_WARNING : s;
	error.line = line;

	if (m) {
		len = (strlen(m) > 256) ? 256 : strlen(m);
		strncpy((char *)&error.message,m,len);
	}
	if (f) {
		strncpy((char *)&error.function,f,32);
	} 
	if (fl) {
		strncpy((char *)&error.file,fl,32);
	} 
	check_error();
}

char *alias(char *v) {
	alis_t *al;

	if (!drv->aliases) return v;

	for (al = drv->aliases; al->in; ++al) {
		if (!strcmp(v,al->in)) return al->out;
	}

	return v;
	
}

void print_hello(void) {
	static int hello = 0;

	if (hello == 1) return;
	printf("Content-type: text/html; charset=%s\r\n\r\n",DEFAULT_CHARSET);
//	printf("Content-type: text/plain; charset=%s\r\n\r\n",DEFAULT_CHARSET);
//	hello = 1;
}
