#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>
#include "fcgi_stdio.h"

#include "wmail.h"
#include "env.h"
#include "params.h"
#include "session.h"
#include "utils.h"
#include "message.h"
#include "compose.h"
#include "parse.h"

extern int errno;
extern jmp_buf jenv;

extern session_t current;
extern int error;
env_t env;
extern params_t params_get[];
extern params_t params_post[];
extern int pagedetected;

gb_t gb;

void init_env(void) {
	char *t = 0;
	
	t = getenv("X_HTTP_FORWARDED_FOR");
	if (t) env.ip = t;
	else env.ip = getenv("REMOTE_ADDR");

	env.req_method = getenv("REQUEST_METHOD");
	env.cookie = getenv("HTTP_COOKIE");
	env.ctype = getenv("CONTENT_TYPE");
	env.qs	= getenv("QUERY_STRING");
	env.ref = getenv("HTTP_REFERER");
	
	t = getenv("CONTENT_LENGTH");
	env.clength = (t) ? atoi(t) : 0;
	
	return;
}

void init_gb(void) {
	gb.len = 0;
	gb.data = (char **) malloc(sizeof(char *) * MAX_MESSAGES * 3 + 3);
	if (!gb.data) {
		SETERR(E_NOMEM,NULL);
		return;
	}
	return;
}

void free_gb(void) {
	register unsigned int i = 0;

	for (; i < gb.len ; ++i) {
		free(gb.data[i]);
	}
	gb.len = 0;
}

void read_get(void) {
	char *qs;

	qs = env.qs;
	if (!qs) return;

	process_post(qs,PGET);
}

char *read_cookie(void) {
	char *cookie;
	char *sid, *sp;

	cookie = env.cookie;
	if (!cookie) return NULL;

	sid = strstr(cookie,"sid=");
	if (!sid) return NULL;

	sid += 4;
	sp = strchr(sid,';');
	if (sp) *sp = 0;

	DOCHECK(validate_sid(sid),jenv);
	return strdup(sid);
}

void set_cookie(char *sid) {
	char out[TIME_SIZE];

	get_time(out,SESSION_EXPIRES,NULL);
	//printf("Set-Cookie: sid=%s; expires=%s; domain=%s; path=/\r\n",sid,out,DOMAIN);
	printf("Set-Cookie: sid=%s; expires=%s; path=/\r\n",sid,out);
}

void read_post(void) {
	ssize_t cb,accum;
	int len,blen,i;
	char *buf,qbuf[MSG_LINE_SIZE * 2];
	char *ptr,*bnd,*p;
	char **hdrs;
	int mh[] = { H_CONTENT_TYPE, H_CONTENT_TRANSFER_ENCODING, H_CONTENT_DISPOSITION };
	FILE *s;
	
	len = env.clength;
	if (!len) return;

	gb.data[gb.len++] = buf = (char *) malloc(sizeof(char) * len + 1);
	if (!buf) {
		SETERR(E_NOMEM,NULL);
		return;
	}

	if (!strncmp(env.ctype,"multipart/form-data",19)) {
		env.ctype += 19;
		if (!(bnd = strstr(env.ctype,"boundary="))) {
			SETERR(E_POST,NULL);
			return;
		} else  {
			bnd += 9;
			if ((ptr = strchr(bnd,';'))) *ptr = 0;
			blen = strlen(bnd);
		}

		current.prefs.uploaddir = NULL;
		create_attach_dir();
		hdrs = (char **) malloc(sizeof(char *) * NUM_HEADERS);
		if (!hdrs) {
			SETERR(E_NOMEM,NULL);
			return;
		}
		for (i = 0; i < NUM_HEADERS; ++i) hdrs[i] = 0;
		ptr =  fgets(qbuf,MSG_LINE_SIZE,FCGI_stdin);
		if (is_boundary(qbuf)) {
			ptr += 2;
			if (strncmp(ptr,bnd,blen)) {
				SETERR(E_POST,NULL);
				return;
			}
		} else {
			SETERR(E_POST,NULL);
			return;
		}

		read_headers(FCGI_stdin,hdrs,mh,HDRS_COUNT(mh));
		if (!hdrs[H_CONTENT_TYPE]) {
			SETERR(E_POST,NULL);
			return;
		}

		p = strchr(hdrs[H_CONTENT_TYPE],';');
		if (p) *p = 0;


		p = post_cdisp(hdrs[H_CONTENT_DISPOSITION]);

		s = open_file();
		if (!s) {
			SETERR(E_FILE,p);
			return;
		}
	
		if (p) {
			fputs(p,s);
			fputs("\n",s);
		} else {
			fputs("filename=\"file.dat\"",s);
			fputs("\n",s);
		}
		fputs(hdrs[H_CONTENT_TYPE],s);
		fputs("\n",s);
		accum = cb = 0;
		while (!feof(FCGI_stdin)) {
			cb = fread(qbuf,sizeof(*qbuf),MSG_LINE_SIZE,FCGI_stdin);
			accum += cb;
			if (accum  > len) {
				break;
			}
			
			fwrite(qbuf,sizeof(*qbuf),cb,s);
			if (cb < MSG_LINE_SIZE) {
				int rv;

				qbuf[cb] = 0;
				rv = fseek(s, -(blen + 8),SEEK_CUR); // 2 * '\r\n' + 'end boundary --' + '-- start'
				if (rv < 0) {
					fclose(s);
					SETERR(E_POST,NULL);
					return;
				}
				cb = ftell(s);
				rv = fread(qbuf,sizeof(*qbuf),blen + 8,s);
				if (!rv && ferror(s)) {
					fclose(s);
					SETERR(E_POST,NULL);
					return;
				}

				qbuf[blen+8]=0;
				if (!(ptr = strstr(qbuf,bnd))) {
					fclose(s);
					SETERR(E_POST,NULL);
					return;
				}
				break;
			} 
		}
		ftruncate(fileno(s),cb);
		fclose(s);
	
		current.page = PAGE_UPLOAD_RESULT;
		pagedetected = 1;
	} else {
		cb = fread(buf,len,1,FCGI_stdin);
		if (cb != 1) {
			free(buf);
			SETERR(E_POST,strerror(errno));
			return;
		}
		buf[len] = 0;
		process_post(buf,PPOST);
	}
}

void process_post(char *buf, short q) {
	char *tp;
	char *n,*v;

	for (tp = strtok(buf,"&"); tp != NULL; tp = strtok(NULL,"&")) {
		n = tp;
		v = strchr(tp,'=');
		if (!v) SETERR(E_PARSE,NULL);
		*v = 0,	++v;	
		if (v) if (*v) do_val(n,v,q);
	}
}

void do_val(char *n, char *v, short q) {
	params_t *p,*sp;
	int i;
	
	if (q == PGET) sp = params_get;
	else if (q == PPOST) sp = params_post;
	else return;
//	print_hello();
	for (i = 0, p = &sp[i]; p->name; ++i, p = &sp[i]) {
//		printf("comp %s -> %s<br>",n,p->name);	
		if (!strcmp(n,p->name)) {
			p->do_work(v);
			break;
		}
	}
	return;
}
