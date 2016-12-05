#include <stdlib.h>
#include <locale.h>
#include "fcgi_stdio.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

#include "wmail.h"
#include "template.h"
#include "buffers.h"
#include "driver.h"
#include "session.h"
#include "env.h"
#include "utils.h"
#include "message.h"
#include "compose.h"
#include "params.h"

extern struct error_struct error;
extern session_t current;
extern header_t headers[];
extern driver_t *drv;
extern env_t env;
extern char hostname[HOSTNAME_LEN];

static DIR *open_attach_dir(char *rbuf) {
	DIR *dir;
	struct stat st;

	sprintf(rbuf,SESSION_DIR"/.%s",current.sid);
	if (stat(rbuf,&st) < 0) return NULL;

	if (!S_ISDIR(st.st_mode)) return NULL;

	if ((dir = opendir(rbuf)) < 0) {
		SETERR(E_OPENDIR,rbuf);
		return NULL;
	}

	return dir;
}

static int collect_buf(char *vbuf,char *rbuf,char *de,int off,unsigned long *tsz) {
	char *ptr,*tp,buf[PATH_LEN];
	struct stat st;
	size_t sz;
	FILE *f;
	char sb[MSG_LINE_SIZE];
	int len;
	char digit[512];

	sprintf(buf,"%s/%s",rbuf,de);
	if (stat(buf,&st) < 0) {
		SETWARN(E_FILE,buf);
		return 1;
	}

	
	sz = st.st_size / 1024;
	*tsz += sz;
	f = fopen(buf,"r");
	if (!f) {
		SETERR(E_FILE,buf);
		return 0;
	}
	ptr = fgets(sb,MSG_LINE_SIZE,f);
	fclose(f);
	tp = strchr(ptr,'\n');
	if (tp) {
		*tp = 0;
		if (*--tp == '\r') *tp = 0;
	}

	if (!sz) sz = 1;
	len = strlen(ptr);
	memcpy(vbuf + off,ptr,len);
	off += len;

	if (sz > (FILE_QUOTA * 1024) || *tsz > (MAX_QUOTA * 1024)) {
		sprintf(digit,"(<b>%u K, файл превысил квоту и не будет прикреплен</b>)",sz);
		if (unlink(buf) < 0) SETWARN(E_FILE,buf);
	} else sprintf(digit,"&nbsp;(<b>%u&nbsp;K</b>)&nbsp;\
	<a target=\"ifr_target\" href="SELF"?todel=%s>удалить</a><br />",sz,de);

	len = strlen(digit);
	memcpy(vbuf + off,digit,len);
	off += len;

	return off;
}

char *filelisting(int flag) {
	char *vbuf = NULL,rbuf[PATH_LEN];
	DIR *dir;
	struct dirent *de;
	int i = 1;
	int rv = 0;
	unsigned long tsz = 0;

	if (!(dir = open_attach_dir(rbuf))) return NULL;

	vbuf = (char *) malloc(sizeof(char) * 8);
	if (!vbuf) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}
	memcpy(vbuf,NBSP,6);
	vbuf[7] = 0;

	while ((de = readdir(dir)) != NULL) {
		if (de->d_name[0] != '_') continue;

		if (flag) {
			flag = 0;
			break;
		}
		vbuf = (char *) realloc(vbuf,sizeof(char) * i * PATH_LEN + 32);
		if (!vbuf) {
			SETERR(E_NOMEM,NULL);
			return NULL;
		}
		rv = collect_buf(vbuf,rbuf,de->d_name,rv,&tsz);
		if (rv == 1) continue;
		else if (!rv) return NULL;
		++i;
	}

	vbuf[rv] = 0;
	if (flag) vbuf = NULL;
	return vbuf;
}

void rm_file(char *file) {
	char buf[PATH_LEN];

	sprintf(buf,SESSION_DIR"/.%s/%s",current.sid,file);
	if (unlink(buf) < 0) {
		SETWARN(E_FILE,buf);
		return;
	}
}

void create_attach_dir(void) {
	char *buf;
	struct stat st;

	if (!current.prefs.uploaddir) {
		buf = (char *) malloc(sizeof(char)*PATH_LEN);
		if (!buf) {
			SETERR(E_NOMEM,NULL);
			return;
		}
		sprintf(buf,SESSION_DIR"/.%s",current.sid);
		current.prefs.uploaddir = buf;
	} 
	if (stat(current.prefs.uploaddir,&st) < 0) {
		if (mkdir(current.prefs.uploaddir,0700) < 0) {
			SETERR(E_FILE,current.prefs.uploaddir);
		}
	} 
}

static int setname(char *buf) {
	struct stat st;
	time_t tm;

	tm = time(NULL);
	if (tm == (time_t) -1) {
		SETWARN(E_TIME,NULL);
		return 0;
	}
	while (1) {
		sprintf(buf,"%s/_%ld",current.prefs.uploaddir,tm);
		if (stat(buf,&st) == 0) {
			++tm;
		} else break;
	}
	return 1;
}

FILE *open_file(void) {
	char buf[PATH_LEN];
	FILE *stream;

	if (!setname(buf)) return NULL;;
	stream = fopen(buf,"w+");
	if (!stream) {
		SETWARN(E_FILE,buf);
		return NULL;
	} else return stream;
}

char *set_to(void) {
	char *buf;
	int len;

	if (!current.cdata.to) {
		SETERR(E_PARSE,NULL);
		return NULL;
	}
	len = strlen(current.cdata.to);
	buf = (char *) malloc(sizeof(char) * len + 16);	
	if (!buf) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}

	memcpy(buf,"<",1);
	memcpy(buf+1,current.cdata.to,len);
	memcpy(buf+1+len,">\0",2);

	return buf;
	
}

char *set_cc(void) {
	if (!current.cdata.copy) return NULL;
	return strdup(current.cdata.copy);
}

char *set_fname(char *fname) {
	char *buf;
	unsigned char *p;
	int tlen,len,v;

	if (is_ascii((unsigned char *)fname)) return NULL;
	tlen = len = strlen(fname);

	tlen *= 3;
	tlen +=16;

	buf = (char *) malloc(sizeof(char) * tlen + 16);
	if (!buf) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}
	memcpy(buf,"=?utf-8?B?",10);
	p = base64_encode((unsigned char *)fname,len,&v);
	if (!p) {
		free(buf);
		SETERR(E_FILE,NULL);
		return NULL;
	}
	memcpy(buf+10,p,v);
	memcpy(buf+10+v,"?=\0",3);

	return buf;
}

char *set_subject(void) {
	char *buf = NULL;
	int vtrue = 0,len,tlen = 0;
	unsigned char *p;
	int v;

	if (!current.cdata.subject) return NULL;
	len = strlen((char *)current.cdata.subject);
	/* RFC 2822 */
	if (len > 998) {
		len = 998;
		current.cdata.subject[len] = 0;
	}
	vtrue = is_ascii(current.cdata.subject);

	tlen = len;
	if (!vtrue) tlen *= 3;	
	tlen += 16;

	buf = (char *) malloc(sizeof(char) * tlen + 16); 
	if (!buf) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}

	if (!vtrue) {
		memcpy(buf,"=?utf-8?B?",10);
		p = base64_encode((unsigned char *)current.cdata.subject,len,&v);
		if (!p) {
			SETERR(E_FILE,NULL);
			return NULL;
		}
		memcpy(buf+10,p,v);
		memcpy(buf+10+v,"?=\0",3);
	} else {
		memcpy(buf,current.cdata.subject,len);
		memcpy(buf+len,"\0",1);
	}

	return buf;
}

char *set_from(void) {
	char *buf;
	unsigned char *p;
	int plen = 0,len,elen,v;

	if (current.prefs.name) 
	plen = strlen((char *)current.prefs.name);
	
	elen = strlen(current.email);
	len = elen + plen + 2;

	buf = (char *) malloc(sizeof(char) * len * 3 + 16); 
	if (!buf) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}

	if (current.prefs.name) {
		memcpy(buf,"=?utf-8?B?",10);
		p = base64_encode((unsigned char *)current.prefs.name,plen,&v);
		if (!p) {
			SETERR(E_FILE,NULL);
			return NULL;
		}
		memcpy(buf+10,p,v);
		memcpy(buf+10+v,"?= <",4);
		memcpy(buf+14+v,current.email,elen);
		memcpy(buf+14+v+elen,">\0",2);
	} else {
		memcpy(buf,current.email,elen);
		memcpy(buf+elen,"\0",1);
	}
	
	return buf;
}

#define BSIZE	256
char *gen_boundary(void) {
	char *b, buf[BSIZE];
	time_t tv;
	int p[2];

	if (time(&tv) == (time_t) -1) {
                SETERR(E_TIME,NULL);
                return NULL;
        }
	
	p[0] = tv & 0xff;
	p[1] = tv & 0xff00;
	
	snprintf(buf,BSIZE,"-----NextPart-%d%d%d-Wmail",p[0],getpid(),p[1]);
	b = strdup(buf);
	if (!b) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}
	return b;
}

void process_body(FILE *f,message_t *msg, int vtrue) {
	DIR *dir;
	FILE *fd;
	struct dirent *de;
	char fbuf[PATH_LEN],rbuf[PATH_LEN], *bnd;
	char sb[MSG_LINE_SIZE],*ptr,*sp;
	unsigned char *p;
	int len;
	ssize_t cb;

	
	if (msg->flags & FL_ATTACH) {
		if (!(dir = open_attach_dir(rbuf))) {
			SETERR(E_OPENDIR,rbuf);
			return;
		}
		bnd = msg->p0->boundary;

		fputs("This is a multi-part message in MIME format.\n\n",f);
		fprintf(f,"--%s\n",bnd);
		fprintf(f,"Content-Type: text/plain; charset=\"utf-8\"\n");
		if (!vtrue) {
			fputs("Content-Transfer-Encoding: quoted-printable\n",f);
			msg->body = (unsigned char *) quoted_printable_encode((char *)msg->body,msg->bodylen,0);
			msg->bodylen = strlen((char *)msg->body);
		}
		fputs("\n",f);
		if (msg->body) {
			fwrite(msg->body,sizeof(*msg->body),msg->bodylen,f);
			fputs("\n",f);
		}
		while ((de = readdir(dir)) != NULL) {
			if (de->d_name[0] != '_') continue;

			sprintf(fbuf,"%s/%s",rbuf,de->d_name);
			fd = fopen(fbuf,"r");
			if (!fd) {
				SETERR(E_FILE,fbuf);
				return;
			}
			fprintf(f,"--%s\n",bnd);
			ptr = fgets(sb,MSG_LINE_SIZE,fd);
			ptr[strlen(ptr)-1] = 0;
			sp = set_fname(ptr);
			if (!sp) fprintf(f,"Content-Disposition: attachment; filename=\"%s\"\n",ptr);
			else {
				fprintf(f,"Content-Disposition: attachment; filename=\"%s\"\n",sp);
				free(sp);
			}	
			
			ptr = fgets(sb,MSG_LINE_SIZE,fd);
			fprintf(f,"Content-Type: %s",ptr);

			if (!strncmp(ptr,"text/",5)) {
				fprintf(f,"Content-Transfer-Encoding: quoted-printable\n\n");
				while (!feof(fd)) {
					cb = fread(sb,sizeof(*sb),54,fd);
					p = (unsigned char *) quoted_printable_encode(sb,cb,0);
					fwrite(p,sizeof(*p),strlen((char *)p),f);
					//fwrite("\r\n",1,2,f);
				}
			} else {
				fprintf(f,"Content-Transfer-Encoding: base64\n\n");
				while(!feof(fd)) {
					cb = fread(sb,sizeof(*sb),54,fd);
					p = base64_encode((unsigned char *)sb,cb,&len);
					fwrite(p,sizeof(*p),len,f);
					fwrite("\r\n",1,2,f);
				}
			}
			
			fclose(fd);
		}
		fprintf(f,"--%s--\n",bnd);
	} else {
		if (!msg->body) return;
		if (!vtrue) {
			msg->body = (unsigned char *) quoted_printable_encode((char *)msg->body,msg->bodylen,0);
			msg->bodylen = strlen((char *)msg->body);
		}
		fwrite(msg->body,sizeof(*msg->body),msg->bodylen,f);
	}
}

void addheaders(FILE *f,message_t *msg, int vtrue) {
	char date[200];

	get_time(date,0,NULL);
	fprintf(f,"Received: from %s by\n\t%s (Wmail) with HTTP;\n\t%s\n",env.ip,hostname,date);
	fprintf(f,"From: %s\n",msg->from);
	fprintf(f,"To: %s\n",msg->to);
	if (msg->cc) fprintf(f,"Cc: %s\n",msg->cc);
	if (msg->subject) fprintf(f,"Subject: %s\n",msg->subject);
	else fprintf(f,"Subject: \n");
	fprintf(f,"Date: %s\n",msg->date);
	fprintf(f,"Reply-To: %s\n",current.email);
	if (msg->flags & FL_ATTACH) {
		fprintf(f,"Content-Type: multipart/mixed;\n\tboundary=\"%s\"\n",msg->p0->boundary);
	} else {
		fprintf(f,"Content-Type: text/plain; charset=\"utf-8\"\n");
		if (!vtrue) fputs("Content-Transfer-Encoding: quoted-printable\n",f);
	}
	fputs("MIME-Version: 1.0\n",f);
	fputs("X-Mailer: WMail 0.9-beta\n",f);
	if (msg->prio != CPRIO_MED) fprintf(f,"X-Priority: %d\n",msg->prio);
	fputs("\n",f);
	
}

char *fill_file(message_t *msg) {
	char *buf;
	FILE *f;
	int vtrue = 0;

	buf = (char *) malloc(sizeof(char) * PATH_LEN);
	if (!buf) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}
	sprintf(buf,SESSION_DIR"/tmp.XXXXXX");
	if (mkstemp(buf) < 0) {
		SETERR(E_FILE,NULL);
		return NULL;
	}
	
	f = fopen(buf,"w+");
	if (!f) {
		SETERR(E_FILE,buf);
		return NULL;
	}
	if (msg->bodylen > 0)
		vtrue = is_ascii(msg->body);
	else vtrue = 1;
	addheaders(f,msg,vtrue);
	process_body(f,msg,vtrue);
	//fputs(

	fclose(f);
	return buf;
	//if (unlink(buf) < 0) SETWARN(E_FILE,buf);
}

void sendmail(char *file,char *from,char *to) {
	FILE *pipe,*f;
	ssize_t cb;
	char buf[MSG_LINE_SIZE];
	char bufs[1024];

	sprintf(bufs,"%s %s -f%s %s",
		SENDMAIL,
		(to) ? "" : "-t" ,
		from,
		(to) ? to : ""
	);
	pipe = popen(bufs,"w");
	if (!pipe) {
		SETERR(E_FILE,SENDMAIL);
		return;
	}

	f = fopen(file,"r");
	if (!f) {
		pclose(pipe);
		SETERR(E_FILE,file);
		return;
	}
	
	while (!feof(f)) {
		cb = fread(buf,sizeof(*buf),MSG_LINE_SIZE,f);
		fwrite(buf,sizeof(*buf),cb,pipe);
	}

	fclose(f);
	pclose(pipe);
//	printf("will send %s<br>",file);
}

void compose_msg(int isdraft) {
	char *ptr,*out;
	message_t *msg;
	char *file;

        msg = alloc_msg();
        ptr = filelisting(1);
        if (ptr) {
		msg->flags |= FL_ATTACH;
		msg->p0 = alloc_mime();
		msg->p0->boundary = gen_boundary();
	}

	msg->from = set_from();
	msg->to = set_to();
	msg->subject = set_subject();
	msg->prio = current.cdata.prio;
	if (current.cdata.body) {
		msg->body = (unsigned char *)strdup((char *)current.cdata.body);
		msg->bodylen = strlen((char *)msg->body);
	}  
	msg->cc = set_cc();

	out = (char *) malloc(sizeof(char) * TIME_SIZE);
	if (!out) {
		free_msg(msg);
		SETERR(E_NOMEM,NULL);
		return;
	}

	msg->date = get_time(out,0,NULL);
	file = fill_file(msg);

	if (isdraft) {
		drv->save_sent(file,"Draft");
	} else {
		sendmail(file,current.email,NULL);
		drv->save_sent(file,"sent-mail");
	}
	free_msg(msg);
}

unsigned int set_sign(char *buf,int slen) {
	memcpy(buf,"\n\n--\n",5);
	memcpy(buf+5,current.prefs.sign,slen);
	memcpy(buf+5+slen,"\0",1);
	return (slen+6);
}

char *compose_rebody(message_t *msg) {
	char *nb,*ptr,*sptr;
	int len,acc = 0,tlen;
	int state = 0;
	int slen = 0;
	

	if (current.prefs.sign) slen = strlen((char *)current.prefs.sign);
	len = msg->bodylen + slen;
	sptr = nb = (char *) malloc(sizeof(char) * len * 3 + 256); // 256 for caption
	if (!nb) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}

	if (slen) acc = set_sign(nb,slen) - 1;	

	memcpy(nb+acc,"\n\n-- Original message --\n",25), acc += 25;
	memcpy(nb+acc,"From: ",6), acc += 6;
	tlen = strlen(msg->from); 
	memcpy(nb+acc,msg->from,tlen), acc += tlen;
	memcpy(nb+acc,"\n",1), acc++;
	memcpy(nb+acc,"Subject: ",9), acc += 9;
	if (msg->subject) {
		tlen = strlen(msg->subject);
		memcpy(nb+acc,msg->subject,tlen);
		acc += tlen;
	}
	memcpy(nb+acc,"\n\n",2), acc += 2;
	nb += acc;

	ARROW(nb);
	ptr = (char *)msg->body;
	if (ptr) while (*ptr) {
		if (*ptr == '\n') {
			*nb++ = *ptr++;
			state = 1;
			ARROW(nb);
		} 
		if (*ptr == '<') {
			if (!strncasecmp(ptr,"<br>",4)) {
				ptr += 4;
				*nb++ = '\n';
				ARROW(nb);
			} else if (!strncasecmp(ptr,"<br />",6)) {
				ptr += 6;
				*nb++ = '\n';
				ARROW(nb);
			} else {
				do {
					++ptr;
				} while (*ptr != '>');
				++ptr;
				if (*ptr == '\n' && state == 1) ++ptr;
			}
		} else {
			state = 0;
			*nb++ = *ptr++;
		}
	}

	*nb = 0;
	return (char *) sptr;

}

char *compose_draft_body(message_t *msg) {
	char *ptr,*nb,*sptr;

	sptr = nb = (char *) malloc(sizeof(char) * msg->bodylen); // 256 for caption
	if (!nb) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}
	ptr = (char *)msg->body;
	while (*ptr) {
		if (*ptr == '<') {
			if (!strncasecmp(ptr,"<br>",4)) {
				ptr += 4;
				*nb++ = '\n';
			} else if (!strncasecmp(ptr,"<br />",6)) {
				ptr += 6;
				*nb++ = '\n';
			} else {
				do {
					++ptr;
				} while (*ptr != '>');
				++ptr;
			}
		} else {
			*nb++ = *ptr++;
		}
	}
	*nb = 0;
	return sptr;
}

char *compose_fwdsubj(char *subj) {
	int len;
	char *ptr;

	len = strlen((char *)subj);
	ptr = (char *) malloc(sizeof(char) * len + 5);
	if (!ptr) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}

	*ptr++ = 'F', *ptr++ = 'w', *ptr++ = 'd', *ptr++ = ':', *ptr++ = 0x20;
	memcpy(ptr,subj,len);
	*(ptr + len) = 0;
	return (ptr-5);
}


char *compose_resubj(char *subj) {
	int len;
	char *ptr;

	len = strlen((char *)subj);
	ptr = (char *) malloc(sizeof(char) * len + 5);
	if (!ptr) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}

	*ptr++ = 'R', *ptr++ = 'e', *ptr++ = ':', *ptr++ = 0x20;
	memcpy(ptr,subj,len);
	*(ptr + len) = 0;
	return (ptr-4);
}


void flush_attach_dir(void) {
	char rbuf[PATH_LEN];
	DIR *dir;
	struct dirent *de;
	int len;
	
	if (!(dir = open_attach_dir(rbuf))) return;

	len = strlen(rbuf);
	if (len + 10 > PATH_LEN) return;
	rbuf[len] = '/';
	rbuf[len+1] = 0;

	while ((de = readdir(dir)) != NULL) {
		if (de->d_name[0] != '_') continue;
		strcpy(&rbuf[len+1],de->d_name);
		if (unlink(rbuf) < 0) {
			SETWARN(E_FILE,rbuf);
		}
	}
	closedir(dir);
}

static void move_it(char *name,long s,long e) {
	FILE *f;

	create_attach_dir();
	f = open_file();
	get_attach(name,f,s,e);
	fclose(f);
}

static void deep_mime(message_t *msg, mime_t *mime) {
	int i;

	if (mime->flags & MFL_LAST) {
                if (mime->flags & MFL_ISDISPLAY ||
                        mime->parent->flags & (MFL_ISALTER|MFL_ISPARALLEL)) {
                } else {
			move_it(msg->file,mime->start,mime->end);
		}
	} else {
		for (i=0; i < mime->total; ++i) {
                        deep_mime(msg,mime->childs[i]);
                }
	}
}

void move_attaches(message_t *msg) {
	deep_mime(msg,msg->p0);
}

void fill_cc(message_t *msg) {
	if (!current.cdata.ra && !current.cdata.isdraft) {
		ZHASH("r_cc");
		return;
	}
	if (!msg->cc) {
		ZHASH("r_cc");
		return;
	}
	if (!msg->cc_email) {
		ZHASH("r_cc");
		return;
	}
	add2hash("r_cc",msg->cc_email);
}
