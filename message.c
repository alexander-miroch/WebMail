#include <string.h>

#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <iconv.h>
#include <errno.h>
#include <sys/mman.h>

#include "fcgi_stdio.h"
#include "session.h"
#include "driver.h"
#include "utils.h"
#include "wmail.h"
#include "parse.h"
#include "message.h"
#include "params.h"

extern driver_t *drv;
extern session_t current;
extern subst_t asubst[];

header_t headers[] = {
	{ .name = "Received",		.parse = parse_rcvd, 		.pparse = 0,		.mparse = 0 },
	{ .name = "From",		.parse = parse_from, 		.pparse = 0,		.mparse = parse_mime_from },
	{ .name = "Subject",		.parse = parse_subject, 	.pparse = pparse_subj, 	.mparse = parse_mime_subject },
	{ .name = "X-Priority",		.parse = parse_xpriority,	.pparse = 0,		.mparse = 0 },
	{ .name = "Content-Type",	.parse = parse_ctype, 		.pparse = 0,		.mparse = parse_mime },
	{ .name = "Content-Transfer-Encoding",	.parse = parse_cte, 	.pparse = 0,		.mparse = parse_mime_cte },
	{ .name = "Content-Disposition",	.parse = parse_cdisp, 	.pparse = 0,		.mparse = parse_mime_cdisp },
	{ .name = "Content-ID",		.parse = parse_ctid, 		.pparse = 0,		.mparse = parse_mime_ctid },
	{ .name = "To",			.parse = parse_to, 		.pparse = 0,		.mparse = 0 },
	{ .name = "Reply-To",		.parse = parse_rpto, 		.pparse = 0,		.mparse = 0 },
	{ .name = "X-Spam-Status",	.parse = 0, 			.pparse = 0,		.mparse = 0 },
	{ .name = "Cc",			.parse = parse_cc, 		.pparse = 0,		.mparse = 0 },
	{ .name = "Date",		.parse = parse_date, 		.pparse = 0,		.mparse = 0 }
	
};

message_t *alloc_msg(void) {
	message_t *msg;
	int i = 0;

	msg = (message_t *) malloc(sizeof(message_t));
	if (!msg) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}

	msg->headers = (char **) malloc (sizeof(char *) * NUM_HEADERS);
	if (!msg->headers) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}

	for (; i < NUM_HEADERS; ++i) msg->headers[i] = 0;
	msg->prio = msg->flags = 0;
	msg->p0 = NULL;
	msg->file = NULL;
	msg->from = msg->email_from = NULL;
	msg->to = msg->replyto = NULL;
	msg->email_to = msg->email_rpto = NULL;
	msg->cc = msg->bcc = NULL;
	msg->cc_email = NULL;
	msg->date = msg->senddate = NULL;
	msg->body = NULL;
	msg->subject = NULL;
	msg->pflags = 0;

	return msg;
}

void free_msg(message_t *msg) {
	int i = 0;

	// Free fields here

	if (msg->headers) {
		for (; i < NUM_HEADERS; ++i) {
			if (msg->headers[i]) free(msg->headers[i]);
		}
		free(msg->headers);
	}

	if (msg->body) free(msg->body);
	if (msg->date) free(msg->date);
	if (msg->senddate) free(msg->senddate);
	if (msg->p0) free_mime(msg->p0);
	if (msg->cc) free(msg->cc);

	free(msg);
}

fdata_t *alloc_fdata(void) {
	fdata_t *fd;

	fd = malloc(sizeof(fdata_t));
	if (!fd) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}

	fd->stream = NULL;
	fd->size = 0;

	return fd;
}

void get_source(char *msgfile) {
	void *stream;
	fdata_t *fdata;
	char buf[MSG_LINE_SIZE];
	int h_all[] = { H_CONTENT_TYPE }; 
	message_t *msg;
	char *cset;

	fdata = drv->get_msgstream(NULL,msgfile);
	if (!fdata || !fdata->stream) {
		SETERR(E_FILE,msgfile);
		return;
	}
	stream = fdata->stream;
	
	msg = alloc_msg();
	read_headers(stream,msg->headers,h_all,HDRS_COUNT(h_all));
	SET_CT;
		do_parse(msg,h_all,HDRS_COUNT(h_all)); 
	RET_CT;

	cset = (msg->p0 && msg->p0->charset) ? msg->p0->charset : DEFAULT_CHARSET;
	drv->setpos(stream,0);

	printf("Content-Type: text/plain; charset=\"%s\"\n",cset);
	printf("Content-Disposition: inline; filename=\"Message Source\"\n\n");
	free_msg(msg);

	while (drv->fgets(buf,MSG_LINE_SIZE,stream) != NULL) {
		printf("%s",buf);
	}


	drv->finish(fdata);
	return;
}

char *get_attach(char *msgfile, FILE *f,long s,long e) {
	void *stream;
	fdata_t *fdata;
	int size,len,off;
	char buf[MSG_LINE_SIZE];
	int mh[] = { H_CONTENT_TYPE, H_CONTENT_TRANSFER_ENCODING, H_CONTENT_DISPOSITION };
	mime_t *m;
	int v;
	char *p = NULL;
	int fn = 0;

	fdata = drv->get_msgstream(NULL,msgfile);
	if (!fdata || !fdata->stream) {
		SETERR(E_FILE,msgfile);
		return NULL;
	}
	stream = fdata->stream;

	if (drv->setpos(stream,s) < 0) {
		SETERR(E_FILE,msgfile);
		return NULL;
	}

	m = alloc_mime();
	read_headers(stream,m->headers,mh,HDRS_COUNT(mh));
	do_mime_parse(NULL,m,mh,HDRS_COUNT(mh)); 

	
	off = len = 0;
	s = drv->getpos(stream);
	size = e - s ;

	if (!f) {
		printf("Content-type: %s\n",
		(m->type) ? m->type : "application/data");
		printf("Content-Transfer-Encoding: binary\n");
		printf("Content-Disposition: attachment;\n\tfilename=\"%s\"\n\n",
		(m->filename) ? m->filename : "somefile.dat");
	} else {
		char *fname,fn = 0;

		if (m->filename) {
			fname = rfc2047(m->filename,NULL);
			if (fname) {
				fn = 1;
			} else fname = m->filename;
		} else fname = DUMMY_FILE;
		fprintf(f,"%s\n", fname);
		fprintf(f,"%s\n", (m->type) ? m->type : "application/data");
		if (fn) free(fname);
	}

	if (!size) {
		free_mime(m);
		return NULL;
	}
	if (m->enc == CTE_BASE64) {
		char *pb,*p,*sp;
		int v,fd,os;
		off_t off;

		os = (s % 4096);
		off = (s / 4096);
		off *= 4096;

		fd = fileno(stream);
		sp = pb = mmap(0,size+os,PROT_READ,MAP_PRIVATE,fd,off);
		if (pb == (char *) -1) {
		//	munmap(pb,size+os);
			SETERR(E_FILE,NULL);
			return NULL;
		}
		pb += os;
	
		sp = &pb[size-1];
		if (*sp == '\n') --sp;
		while (*sp != '\n') {
			--size;
			--sp;
		}		


		p = (char *)base64_decode((unsigned char *)pb,size,&v);

		if (f) fwrite(p,sizeof(*p),v,f);
		else fwrite(p,sizeof(*p),v,FCGI_stdout);
		free(p);
		munmap(sp,size+os);
	} else 
	while (drv->fgets(buf,MSG_LINE_SIZE,stream) != NULL) {
		len = strlen(buf);
		off += len;
		if (off >= size) break;
	
		/* STUPID. I forgot to remove */
		if (m->enc == CTE_BASE64) {
			p = (char *) base64_decode((unsigned char *)buf,len,&v);
			fn = 1;
		} else 
		if (m->enc == CTE_QP) {
			p = quoted_printable_decode(buf,p,0);
			v = strlen(p);
			fn = 1;
		} else {
			p = buf;
			v = len;
			fn = 0;
		}

		if (f) fwrite(p,sizeof(*p),v,f);
		else fwrite(p,sizeof(*p),v,FCGI_stdout);

		if (fn) free(p);
		fn = 0;
	}
	
	free_mime(m);
	drv->finish(fdata);
	return NULL;
}


message_t *get_msg(char *msgfile) {
	message_t *msg;
	void *stream;
	fdata_t *fdata;
	int h_all[] = { H_RECEIVED , H_FROM, 
			  H_SUBJECT, H_XPRIORITY, H_CC,
			  H_CONTENT_TYPE, H_CONTENT_TRANSFER_ENCODING, H_CONTENT_ID, H_TO, H_REPLY_TO, H_DATE
			};

	msg = alloc_msg();
	msg->flags = drv->get_flags(msgfile);
	msg->file = msgfile;

	fdata = drv->get_msgstream(NULL,msgfile);
	if (!fdata || !fdata->stream) {
		SETERR(E_FILE,msgfile);
		return NULL;
	}

	stream = fdata->stream;

	//print_hello();

	read_headers(stream,msg->headers,h_all,HDRS_COUNT(h_all));
//print_hello();
	SET_CT;	
		do_parse(msg,h_all,HDRS_COUNT(h_all)); 
	RET_CT;
	read_body(stream,msg);

	if (!(msg->flags & FL_SEEN))
		drv->set_flags(NULL,msgfile,msg->flags | FL_SEEN, NULL);

	if (msg->flags & FL_RELATED)
		msg->body = do_related_stuff(msg->body,msg);

	drv->finish(fdata);
/*	if (!msg->body) {
		free_msg(msg);
		return NULL;
	}*/
	return msg;
}

char inline *get_prio(int prio) {
	if (!prio) return NULL;
	if (prio < CPRIO_MED) return IMG_HPRIO;
	if (prio > CPRIO_MED) return IMG_LPRIO;
	return NULL;
}

#define DATE_ADDON_SIZE 64
char *get_header_bar(message_t *msg) {
	char *buf, addon[DATE_ADDON_SIZE];
	size_t size;
	char *prio_img;

	// Subject allready set
	if (msg->from) size = strlen(msg->from); else size = 64;
	if (msg->subject) size += strlen(msg->subject);	else size += 32;
	size += 15; // for date
	
	buf = (char *) malloc(sizeof(char) * (size + 2048 + 8192));
	if (!buf) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}
	prio_img = get_prio(msg->prio);
	if (msg->senddate) 
		snprintf(addon,DATE_ADDON_SIZE," (отправлено: %s)",msg->senddate);
	else {
		addon[0]=0;
	}

	// This stuff is 2048
	sprintf(buf,"<table width=\"100%%\" cellpadding=\"0\" cellspacing=\"0\">\
			<tr><td class=\"pre\"><table width=\"100%%\" cellpadding=\"2\">\
			<tr><td><b>Дата</b>: %s%s </td></tr>\
			<tr><td><b>От</b>: %s &lt;%s&gt;</td></tr>\
			<tr><td><b>Копии</b>: %s </td></tr>\
			<tr><td><b>Тема</b>: %s <br /></td></tr>\
			</table></td><td width=\"40\" class=\"pre0\" valign=\"top\">%s%s%s%s</td></tr>",
		msg->date, addon,
		msg->from ? msg->from : "Отправитель неизвестен",
		msg->from ? msg->email_from : "",
		msg->cc ? msg->cc : "нет",
		msg->subject,
		(msg->flags & FL_FLAGGED) ? IMG_FLAGGED : "",
		(prio_img) ? prio_img : "",
		(msg->flags & FL_REPLIED) ? IMG_REPLIED : "",
		(msg->flags & FL_PASSED) ? IMG_FWD : ""
		);
	// And this is 8192
	if (msg->flags & FL_ATTACH) {
		show_mime(msg,msg->p0,buf);
	}

	strncat(buf,"</table>",8);
	return buf;
}

void detect_size(mime_t *mime) {

	if (mime->enc == CTE_BASE64) {
		mime->size = ((mime->end - mime->rstart) * 3) / 4;
	} else mime->size = mime->end - mime->rstart;
	mime->size /= 1024;
	if (!mime->size) mime->size = 1;
}

void show_mime(message_t *msg,mime_t *mime,char *buf) {
	int i;
	char *ptr = NULL;
	unsigned char *b;

	if (mime->flags & MFL_LAST) {
		if (mime->flags & MFL_ISDISPLAY || (mime->parent && (
			mime->parent->flags & (MFL_ISALTER|MFL_RELATED)))) {
		} else {
			b = (unsigned char *) malloc(sizeof(char)*FOLDER_LEN*3);
			if (!b) {
				SETWARN(E_NOMEM,NULL);
				return;
			}
			url_encode((unsigned char *)current.folder,b);
			ptr = seen(msg->file);
			detect_size(mime);
			sprintf(buf, "%s<tr><td class=\"attach\" colspan=\"2\">\
			"IMG_ATTACH"\
			&nbsp;<a href="SELF"?folder=%s&msg=%s&rg=%ld-%ld&sby=%c&p=%d>%s</a>\
			(%ldK)</td></tr>",
			buf,
			b,
			ptr ? ptr : msg->file,
			mime->start,mime->end,
			current.prefs.sby,
			current.sf,
			show_filename(mime->filename),
			mime->size);		
			free(b);
		}
		if (ptr) free(ptr);
	} else {
		for (i=0; i < mime->total; ++i) {
			show_mime(msg,mime->childs[i],buf);
		}
	}
}

void inline clear_disp(mime_t *mime) {
	int i;

	if (!mime->childs) return;
	for (i = 0; i < mime->total; ++i) 
		mime->childs[i]->flags &= ~MFL_ISDISPLAY;
}

int do_boundary(char *ptr,int *len) {
	int bend = 0;
	if (!*len) *len = strlen(ptr);

	if (ptr[*len-1] == '\n' && ptr[*len-2] == '\r') { ptr[*len-2] = 0; *len -= 2; }
	if (ptr[*len-1] == '\n' && ptr[*len-2] != '\r') { ptr[*len-1] = 0; --*len; }
	if (ptr[*len-1] == '-' && ptr[*len-2] == '-') { bend = 1; *len -= 2; }

	return bend;
}

void read_mimes(void *f, mime_t *m, message_t *msg) {
	char *ptr;
	static char *buf = NULL;
	int mh[] = { H_CONTENT_TYPE, H_CONTENT_TRANSFER_ENCODING, H_CONTENT_DISPOSITION, H_CONTENT_ID };
	int smh[] = { H_SUBJECT, H_FROM, H_CONTENT_TYPE, H_CONTENT_TRANSFER_ENCODING };
	//int smh[] = { H_SUBJECT, H_FROM, H_CONTENT_TRANSFER_ENCODING, H_CONTENT_TYPE, H_CONTENT_DISPOSITION };
	mime_t *cm,*sm;
	int len = 0;
	int bend = 0;
	static int ab = 0;
	static char *res = 0;
	int blen;
	
	if (!m->boundary) return; 
	if (!buf) {
		buf = (char *) malloc(sizeof(char) * MSG_LINE_SIZE);
		if (!buf) {
			SETERR(E_NOMEM,NULL);
			return;
		}
	}
	bend = 0;
	blen = strlen(m->boundary);
	
	if (!ab)
	while (((res = drv->fgets(buf,MSG_LINE_SIZE,f)) != NULL)) {
		if (is_boundary(buf)) {
			ptr = buf;
			ptr += 2;
			len = strlen(ptr);
			if (len < blen) continue;
			bend = do_boundary(ptr,&len);
			if (strncmp(ptr,m->boundary,len)) continue;
			if (bend) {
				if (blen != len) bend = 0;
			}
			break;
		}
	} 
	else {
		ptr = buf;
		ptr += 2;

		len = strlen(ptr);
		if (len >= blen) {
			bend = do_boundary(ptr,&len);
			if (bend) {
				if (blen != len) bend = 0;
			}
		}
	}
/*	if (res) printf("r=%s<br>",res);
	else {
		printf("NORES");

	}*/
	ab = 0;
	//if (!res) printf("nores!<br>");
	if (!res) {
		bend = 1;
//		SETERR(E_MSGPARSE,NULL);
		return;
	}
	if (bend) return; 

	cm = alloc_mime();
	cm->start = drv->getpos(f);
	read_headers(f,cm->headers,mh,HDRS_COUNT(mh));
	cm->rstart = drv->getpos(f);

	cm->parent = m;
	m->childs[m->total++] = cm;
	do_mime_parse(msg,cm,mh,HDRS_COUNT(mh));

	if (cm->flags & MFL_LAST) {
		if ((cm->flags & MFL_ISDISPLAY)) {
			if (cm->parent->flags & MFL_ISALTER) {
				msg->bodylen = msg->plen;
				if (!((cm->parent->flags & MFL_ALTERFOUND) &&
				(cm->parent->flags & MFL_ALTERREAD))) {
					ab = read_part(msg,cm,buf,f);
				}
			} else {
				if (cm->flags & MFL_MESSAGE) {
					sm = alloc_mime();		
					read_headers(f,sm->headers,smh,HDRS_COUNT(smh));
					do_mime_parse(NULL,sm,smh,HDRS_COUNT(smh));
					cm->sm = sm;
				}
				msg->plen = msg->bodylen;
				ab = read_part(msg,cm,buf,f);
			}
		} else {
			int v = 0;
                        ab = skip_attach(cm,f,&v);
                        if (v)  {
                                ab = 0;
                                cm->end = drv->getpos(f);
                                return;
                        }
		}
		cm->end = drv->getpos(f);
	} else {
		if (cm->parent->flags & MFL_ISALTER) {
				msg->bodylen = msg->plen;
		}
		read_mimes(f,cm,msg);
	}

	read_mimes(f,m,msg);
}

char *do_iconv(char *obuf,int len,int *bytes,mime_t *m) {
	char *ibuf,*bb,*pbb;
	iconv_t icv;
	size_t inb,outb;
	int sb;

	ibuf = obuf;
	inb = len;
	sb = outb = 3 * (inb + 1);
	pbb = bb = (char *) malloc(sizeof(char) * outb);
	if (!pbb) {
		SETWARN(E_NOMEM,NULL);
		return NULL;
	}

	icv = iconv_open(DEFAULT_CHARSET,m->charset);
	if (icv == (iconv_t)-1) {
		SETWARN(E_MSGPARSE,m->charset);
		return NULL;
	}

	if ((iconv(icv,&ibuf,&inb,&bb,&outb)) == -1) {
		SETWARN(E_MSGPARSE,m->charset);
		iconv_close(icv);
		return NULL;
	}
	iconv_close(icv);

	if (sb == outb) sb = len;
	else sb -= outb;

	*bytes = sb;
	return pbb;
}


void part_convert(message_t *msg, mime_t *m) {
	char *pbb;
	char *obuf = NULL,*ibuf, *sbuf;
	int len;
	int donoth=0;
	int bytes;


	msg->body[msg->bodylen] = 0;
	ibuf =(char *) &msg->body[msg->plen];
	if (m->enc == CTE_QP) {
		obuf = quoted_printable_decode(ibuf,obuf,0);
		len = strlen(obuf);
		msg->bodylen = msg->plen + len;
	} else if (m->enc == CTE_BASE64) {
		obuf = (char *) base64_decode((unsigned char *)ibuf,(msg->bodylen - msg->plen),&len);
		msg->bodylen = msg->plen + len;
	} else {
		len = msg->bodylen - msg->plen;
		obuf = ibuf;
		donoth = 1;	
	}

	if (m->charset && !is_our_charset(m->charset)) {
		sbuf = obuf;
		pbb = do_iconv(obuf,len,&bytes,m);
		if (!pbb) {
			pbb = obuf;
			bytes = len;
		} else {
			if (!donoth) free(sbuf); 
			donoth = 0;
		}
	} else  {
		bytes = len;
		pbb = obuf;
	}



	if (m->flags & MFL_ISDISPLAY) {
		sbuf = pbb;
		if (!(m->flags & MFL_ISOUR)) {
			pbb = do_subst(sbuf,&bytes,0);
			if (sbuf != pbb && !donoth) free(sbuf);
		} else {
			pbb = do_html_subst(sbuf,&bytes);
			if (sbuf != pbb && !donoth) free(sbuf);
		}
	}

	msg->body = realloc(msg->body,sizeof(char) * (msg->plen + bytes + 1));
	if (!msg->body) {
		SETERR(E_NOMEM,NULL);
		free(pbb);
		return;
	}

	memcpy(msg->body + msg->plen, pbb, bytes);
	msg->bodylen = msg->plen + bytes;
	if (!donoth) free(pbb);
}

int skip_attach(mime_t *m, void *f,int *v) {
	char buf[MSG_LINE_SIZE];
	int l = 0;
	char *ptr;
	int ab = 0;
	int blen;

	while ((drv->fgets(buf,MSG_LINE_SIZE,f) != NULL)) {
		if (is_boundary(buf)) {
			ptr = buf;
			ptr += 2;	
			if (m->parent && m->parent->boundary) {
				blen = strlen(m->parent->boundary);
				l = strlen(ptr);
				if (l < blen) continue;
				if (!strncmp(ptr,m->parent->boundary,blen)) {
					*v = do_boundary(ptr,&l);
					if (l == blen) {
						m->end = drv->getpos(f);
						ab = 1;
						break;
					}
				}
			}
		}
	}
	return ab;
}
#define ADDON	8192
int read_part(message_t *msg, mime_t *m,char *buf, void *f) {
	int ab = 0;
	int l = 0,ladd = 0,len;
	char *ptr,*qbuf;
	int blen;
	int plen;

	if (msg->bodylen) {
		msg->body[msg->bodylen] = ' ';
		l = strlen(MPART_BREAK);
		if (m && m->sm) {
			if (m->sm->subject) ladd += strlen(m->sm->subject);
			if (m->sm->from) ladd += strlen(m->sm->from);
			if (m->sm->email_from) ladd += strlen(m->sm->email_from);
		}
		ladd += ADDON;
		msg->body = realloc(msg->body,(sizeof(char) * msg->bodylen) + l + ladd );  // 32 for "Mo subjr"
		if (!msg->body) {
			SETERR(E_NOMEM,NULL);
			return -1;
		}
		if (m && (m->flags & MFL_MESSAGE) && m->sm) {
			qbuf = (char *) malloc(sizeof(char) * ladd);	
			if (!qbuf) {
				SETERR(E_NOMEM,NULL);
				return -1;
			}
//			exit(1);
			sprintf(qbuf,"<table width=\"100%%\" cellpadding=\"0\" cellspacing=\"0\"><tr><td class=\"x\">От: </td><td class=\"xy\"> %s &lt;%s&gt; </td></tr><tr><td class=\"x\">Тема:</td><td class=\"xy\">%s</tr></table><br />",
				//msg->from,msg->email_from,msg->subject);
		//		m->sm->subject ? m->sm->subject : "Отправитель неизвестен",
				m->sm->from ? m->sm->from : "Отправитель неизвестен",
				m->sm->email_from ? m->sm->email_from : "",
				m->sm->subject ? m->sm->subject : "Без темы");
	//			m->sm->from ? m->sm->from : "Отправитель неизвестен",
//				m->sm->email_from ? m->sm->email_from : "");
			plen = strlen(qbuf);
	//		if (m->sm->flags dd& MFL_LAST) {
				memcpy(msg->body + msg->bodylen,qbuf,plen);
				msg->bodylen += plen;
				msg->plen += plen;
	//		} else read_mimes
			free(qbuf);
			//msg->bodylen += 14;
			//msg->plen += 14;
		} else {
			memcpy(msg->body + msg->bodylen,MPART_BREAK,l);
			msg->bodylen += l;
			msg->plen += l;
		}
	}
//	printf("before cycle %d %p<br>",msg->bodylen,msg->body);
//	if (msg->body) printf("m=%s<br>",msg->body);
//	printf("Btr<br>");
	while ((drv->fgets(buf,MSG_LINE_SIZE,f) != NULL)) {
//		printf("b=%s<br>\n",buf);

		if (is_boundary(buf)) {
			ptr = buf;
			ptr += 2;	
			if (m->parent && m->parent->boundary) {
				blen = strlen(m->parent->boundary);
				l = strlen(ptr);
				if (l >= blen) {
					if (!strncmp(ptr,m->parent->boundary,blen)) {
						do_boundary(ptr,&l);
						m->end = drv->getpos(f);
						ab = 1;
						break;
					}
				}
			}
		}

		msg->body = realloc(msg->body,(sizeof(char) * msg->bodylen) + MSG_LINE_SIZE + 10);
		if (!msg->body) {
			SETERR(E_NOMEM,NULL);
			return -1;
		}

		len = strlen(buf);
		memcpy(msg->body + msg->bodylen, buf, len);
		msg->bodylen += len;
	}

	if (msg && msg->body) {
		if (m->sm) part_convert(msg,m->sm);
		else part_convert(msg,m);
		msg->body[msg->bodylen] = 0;
	}

	if (m->parent && (m->parent->flags & MFL_ALTERFOUND)) m->parent->flags |= MFL_ALTERREAD;
	return ab;
}

#define MAX_URI 1024

char *can_do(char *ptr,int *l) {
	char c = *ptr;
	static int state = 0;
	static char *sp;
	static short meets = 0;
	
	if (!state) meets = 0;
	switch (state) {
		case 0:	if (c == 'h') ++state, sp = ptr ; else state = 0;  break;
		case 1: if (c == 't') ++state; else state = 0; break;
		case 2: if (c == 't') ++state; else state = 0; break;
		case 3: if (c == 'p') ++state; else state = 0; break;
		case 4: 
			if (c == ':') {
				++state;
				break;
			} else if (c == 's') {
				meets = 1;
				return (char *) -1;
			} else state = 0;
		case 5: if (c == '/') ++state; else state = 0; break;
		case 6: if (c == '/') ++state; else state = 0; break;
		
		case 7:
			if (*l > MAX_URI) {
				SETERR(E_MSGPARSE,NULL); 
				break;
			}
			if (c == 0x20 || c == '\t' || c == '\n' || c == '\r' || c == '\'' || c == '"'
					|| c == ')' || c == '>' || c == '(' || c == '<' || c == 0) {
				state = 0;
				*l += 7;
				if (meets) {
					(*l)++;
					meets = 0;
				}
				return sp;		
			}
			(*l)++;
			return NULL;
	}

	return (char *) -1;
} 

#define HREF_STUFF	64

char *do_subst(char *buf,int *len,int skiplink) {
	char *sp,*ptr = buf;
	int i,off = 0, size = 0;
	int llen = *len;
	char *nbuf = NULL;
	int j = 0,f = 0;
	int len_allocated = 0;
	char *asp = NULL;
	int alen = 0,accum;
	char sc;
	
	sp = ptr;
	len_allocated = sizeof(char) * (llen) * MAX_SUBST_LEN + 5;
	while (j < llen) {
		if (!skiplink) {
			asp = can_do(ptr,&alen);
			if (asp == NULL) {
				++ptr;
				++j;
				continue;
			}
			if (asp && asp != (char *) -1) {
				len_allocated += alen + HREF_STUFF;
				nbuf = (char *) realloc(nbuf,len_allocated);
				if (!nbuf) {
					SETWARN(E_NOMEM,NULL);
					continue;
				}
				size = asp - sp;
				sc = *ptr;
				*ptr = 0;		

				memcpy(nbuf + off, sp, size);
				accum = off + size;
				memcpy(nbuf + accum, "<a href=\"",9);
				accum += 9;
				memcpy(nbuf + accum, asp,alen);
				accum += alen;
				memcpy(nbuf + accum, "\">",2);
				accum += 2;
				memcpy(nbuf + accum, asp,alen);
				accum += alen;
				memcpy(nbuf + accum,"</a>",4);
				off += 15 + alen + alen + size;
				*len += 15 + alen;
				sp = ptr;
				*ptr = sc;
				alen = 0;
				continue;
			}
		}
		for (i = 0; asubst[i].c; ++i) {
			if (*ptr == asubst[i].c) {
				if (!nbuf) {
					nbuf = (char *) malloc(len_allocated) ;
					if (!nbuf) {
						SETWARN(E_NOMEM,NULL);
						continue;
					}
				}
				
				size = ptr - sp;
			//	printf("noew sp=%s(%x) sub=%x size=%x<br>",sp,sp,ptr,size);
				
				memcpy(nbuf + off, sp, size);
				memcpy(nbuf + off + size, asubst[i].to, asubst[i].len);
				off += size + asubst[i].len;
				*len += asubst[i].len - 1;
				sp = ptr + 1;
				f = 1;
				break;
			}
		}
		if (!f) if (*ptr < 0x1f && *ptr > 0) *ptr = 0x20;
		f = 0;
		++ptr, ++j;
	}

	size = ptr - sp;
	if (!nbuf) nbuf = buf;
	else if (size) {
		memcpy(nbuf + off,sp,size);
	} else memcpy(nbuf + off,"\0",1);
	return nbuf;
}

void read_body(void *f, message_t *msg) {
	char buf[MSG_LINE_SIZE];
	int i = 1, len;

	msg->body = NULL;
	msg->plen = msg->bodylen = 0;
	msg->p0->start = drv->getpos(f);
	msg->p0->end = LAST_POS;
//	printf("Content-type:text/html\n\n");
//	printf("xxx<br>");
	if (msg->p0->flags & MFL_LAST) {
		if (msg->p0->flags & MFL_ISDISPLAY) {
			while ((drv->fgets(buf,MSG_LINE_SIZE,f) != NULL)) {
				msg->body = realloc(msg->body,sizeof(char) * (MSG_LINE_SIZE + 7) * i);
				if (!msg->body) {
					SETERR(E_NOMEM,NULL);
					return;
				}
				++i;
				len = strlen(buf);
				memcpy(msg->body + msg->bodylen, buf, len);
				msg->bodylen += len;
			}
			if (msg->bodylen) {
				part_convert(msg,msg->p0);
				msg->body[msg->bodylen] = 0;
			} else {
				msg->body = (unsigned char *) strdup(NBSP);
				msg->bodylen = strlen(NBSP);
			}
		} else {
			int v = 0;
			skip_attach(msg->p0,f,&v);
			msg->p0->start = 1;
			msg->p0->end = drv->getpos(f);
		}
	} else {
		read_mimes(f,msg->p0,msg);
	}
}

void free_mime(mime_t *m) {
	int i;
	if (m->headers) free(m->headers);
	if (m->sm) free_mime(m->sm);
	if (m->childs) {
		for (i = 0; i < m->total; ++i) free_mime(m->childs[i]);
		free(m->childs);
	}
	free(m);
}

mime_t *alloc_mime(void) {
	mime_t *mime;
	int i = 0;

	mime = (mime_t *) malloc(sizeof(mime_t));
	if (!mime) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}
	mime->headers = (char **) malloc (sizeof(char *) * NUM_MIME_HEADERS);
	if (!mime->headers) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}

	mime->childs = (mime_t **) malloc (sizeof(char *) * NUM_MIME_PARTS);
	if (!mime->childs) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}
	for (; i < NUM_MIME_HEADERS; ++i) mime->headers[i] = 0;
	for (; i < NUM_MIME_PARTS; ++i) mime->childs[i] = 0;

	mime->from = mime->subject = mime->email_from = NULL;
	mime->sm = NULL;
	mime->enc = mime->total = 0;
	mime->parent = 0;
	mime->boundary = mime->charset = NULL;
	mime->start = mime->rstart = mime->end = 0;
	mime->flags = (MFL_LAST | MFL_ISDISPLAY);
	mime->name = mime->filename = NULL;
	mime->size = 0;
	mime->type = 0;
	mime->ctid = NULL;

	return mime;
}


void read_headers(void *f,char **hdrs, int *what, int size) {
	char buf[MSG_LINE_SIZE];
	int *tp = 0;
	char *ph = NULL;
	char *ptr, *vptr, *tpp;
	int i = 0, find = 0;

	while ((drv->fgets(buf,MSG_LINE_SIZE,f) != NULL)) {
		tpp = &buf[0];

		if (is_hbreak(tpp)) break;
		if (*buf == ' ' || *buf == '\t') {
			if (!ph) continue;
			else {
				int l1,l2;
				char *nm;

				l1 = strlen(ph);
				l2 = strlen(buf);
				nm = (char *) malloc(sizeof(char) * (l1 + l2) + 10);
				if (!nm) {
					SETWARN(E_NOMEM,NULL);
					break;
				}

				if ((ph[l1-2] == '\r' && ph[l1-1] == '\n') ||
					ph[l1-1] == '\r' || ph[l1-1] == '\n') {
					ph[l1-1] = 0;
					--l1;
				}
				memcpy(nm, ph, l1);
				vptr = &buf[0];
				//if (*buf == '\t') *buf = 0x20;
				
				if (*buf == '\t' || *buf == 0x20) {
					vptr = &buf[1];
					--l2;
				}
				//memcpy(nm + l1, buf, l2 + 1);
				memcpy(nm + l1, vptr, l2 + 1);

				if (ph) free(ph);
				ph = nm;
				hdrs[*tp] = nm;
			}
		}
		vptr = ptr = strchr(buf,':');
		if (!ptr) continue;
		*ptr = 0;
		++vptr;
		for (tp = what; i < size; tp++, i++) {
		//	printf("cmp %s with %s<br>",HEADER(*tp),buf);
			if (!strcasecmp(HEADER(*tp),buf)) {
		//		printf("find!<br>");
				if (!hdrs[*tp])	{
					int len = 0;
					skipsp(&vptr);
					do_boundary(vptr,&len);
					hdrs[*tp] = strdup(vptr);
					find = 1;
					break;
				}
			}
		}
		if (find) {
			ph = hdrs[*tp];
			find = 0;
		} else ph = NULL;

		i = 0;
		*ptr = ':';
	}
}

void do_post_parse(message_t *msg, int *what, int size) {
	int i = 0;
	int *tp;
	char *hdr; 

	for (tp = what; i < size; tp++, i++) {
		hdr = msg->headers[*tp];
		if (!hdr) continue;
		if (!headers[*tp].pparse) continue;
		if (PPARSE(msg,*tp,hdr)) {
			SETWARN(E_MSGPARSE,headers[*tp].name);
			continue;
		}		
		
	}
}

void do_mime_parse(message_t *msg, mime_t *mime,int *what, int size) {
	int i = 0;
	int *tp;
	char *hdr;

	for (tp = what; i < size; tp++, i++) {
		hdr = mime->headers[*tp];
		if (!hdr) continue;
		if (MPARSE(msg,mime,*tp,hdr)) {
			SETWARN(E_MSGPARSE,headers[*tp].name);
			continue;
		}		
	}
}


void do_parse(message_t *msg, int *what, int size) {
	int i = 0;
	int *tp;
	char *hdr;

	msg->p0 = alloc_mime();
	for (tp = what; i < size; tp++, i++) {
		hdr = msg->headers[*tp];
		if (!hdr) continue;
		if (PARSE(msg,*tp,hdr)) {
			SETWARN(E_MSGPARSE,headers[*tp].name);
			continue;
		}		
	}

	do_post_parse(msg,what,size);
}


#define ADJ_P(v)	m1 = (message_t **) p1, m2 = (message_t **) p2;\
				if (!(*m1)->v) return 1; if (!(*m2)->v) return -1

static int cmp_msg_size(const void *p1,const void *p2) {
        message_t **m1,**m2;

	ADJ_P(size);
        return (*m1)->size > (*m2)->size;
}

static int cmp_msg_size_r(const void *p1,const void *p2) {
        message_t **m1,**m2;

	ADJ_P(size);
        return (*m1)->size < (*m2)->size;
}

static int cmp_msg_subj(const void *p1,const void *p2) {
	message_t **m1,**m2;

	ADJ_P(subject);
	return strcasecmp((*m1)->subject, (*m2)->subject);
}

static int cmp_msg_subj_r(const void *p1,const void *p2) {
	message_t **m1,**m2;

	ADJ_P(subject);
	return strcasecmp((*m2)->subject, (*m1)->subject);
}

static int cmp_msg_date(const void *p1,const void *p2) {
	message_t **m1,**m2;
	char c1,c2;
	int prs[] = { 6,7,3,4,0,1,9,10,12,13 }, i;
	
	ADJ_P(date);
	for (i = 0; i < 10; ++i) {
		c1 = (*m1)->date[prs[i]];
		c2 = (*m2)->date[prs[i]];

		if (c1 == c2) continue;
		if (c1 > c2) return 1; else return -1;
	}
	return 1;
}

static int cmp_msg_date_r(const void *p1,const void *p2) {
	message_t **m1,**m2;
	char c1,c2;
	int prs[] = { 6,7,3,4,0,1,9,10,12,13 }, i;
	
	ADJ_P(date);
	for (i = 0; i < 10; ++i) {
		c1 = (*m1)->date[prs[i]];
		c2 = (*m2)->date[prs[i]];

		if (c1 == c2) continue;
		if (c1 < c2) return 1; else return -1;
	}
	return 1;
}

static int cmp_msg_from(const void *p1,const void *p2) {
	message_t **m1,**m2;

	ADJ_P(from);
	return strcasecmp((*m1)->from, (*m2)->from);
}

static int cmp_msg_from_r(const void *p1,const void *p2) {
	message_t **m1,**m2;

	ADJ_P(from);
	return strcasecmp((*m2)->from, (*m1)->from);
}

static int cmp_msg_to(const void *p1,const void *p2) {
	message_t **m1,**m2;

	ADJ_P(to);
	return strcasecmp((*m1)->to, (*m2)->to);
}

static int cmp_msg_to_r(const void *p1,const void *p2) {
	message_t **m1,**m2;

	ADJ_P(to);
	return strcasecmp((*m2)->to, (*m1)->to);
}

char *out_ms(collect_t *base) {
        register int i,j;
        int len,sf = 0;
        char *tp,*buf;
	message_t *msg,**msgs;
	int pre_h[] = { H_RECEIVED , H_FROM, H_SUBJECT, H_XPRIORITY, H_CONTENT_TYPE, H_TO };
	void *stream;
	int (*cf)(const void *, const void *);
	fdata_t *fdata;

        len = base->len;
	msgs = (message_t **) malloc(sizeof(message_t *) * len + 1);
        if (!msgs) {
                SETERR(E_NOMEM,NULL);
                return NULL;
        }
        tp = buf = (char *) malloc(sizeof(char) * len * (MESSAGE_LEN + FOLDER_LEN) * 10);
        if (!buf) {
                SETERR(E_NOMEM,NULL);
		free(msgs);
                return NULL;
        }

	bzero(tp,sizeof(char) * 2);

        for (i = 0; i < len; ++i) {
		msg = alloc_msg();
		msg->flags = drv->get_flags(base->base[i].name);

		fdata = drv->get_msgstream(NULL,base->base[i].name);
		if (!fdata || !fdata->stream) {	
			free_msg(msg);
			continue;
		}
		stream = fdata->stream;
		msg->size = fdata->size / 1024;
		if (msg->size < 1) msg->size = 1;
		
		read_headers(stream,msg->headers,pre_h,HDRS_COUNT(pre_h));
		do_parse(msg,pre_h,HDRS_COUNT(pre_h));
		msg->file = base->base[i].name;
		msgs[i] = msg;
		drv->finish(fdata);
        }

	cf = get_sort_func();
	qsort(msgs,len,sizeof(message_t *),cf);
	if (current.prefs.pp)  {
		sf = current.sf * current.prefs.pp;
		if (current.prefs.pp >= base->len) sf = 0, len = base->len;
		else {
			int ost;
		
			ost = base->len - sf;
			if (ost < 0) sf = 0, len = base->len;
			if (ost >= current.prefs.pp) len = current.prefs.pp;
			else len = ost;
		}
	}

	for (i = 0, j = sf; i < len; ++i,++j) {
		msg = msgs[j];
                buf += msg2buf(buf,msg);
	}
	for (j = 0; j < base->len; ++j) free_msg(msgs[j]);

	free(msgs);
        return tp;
}

void check_message(message_t *msg) {
	if (msg->subject && !*msg->subject) {
		msg->subject = NULL;
	}

}



int msg2buf(char *buf, message_t *msg) {
	char class[4];
	unsigned char ubuf[FOLDER_LEN];
	char *prio_img;
	
	// -- [checkbox] -- [attach -- fwd -- replied -- sign -- prio] --- [from] --- [subject] -- [date]
	class[3] = 0;
	if (msg->flags & FL_SEEN) strncpy(class,"sim",3);
	else strncpy(class,"new",3);

	url_encode((unsigned char *)current.folder,ubuf);
	check_message(msg);
	prio_img = get_prio(msg->prio);	

	sprintf(buf,"<tr class=\"%s\" onmouseover=\"javascript:hl(this,1)\" onmouseout=\"javascript:hl(this,0)\">\
	<td class=\"ff0\"><input name=\"marr\" type=\"checkbox\" value=\"%s\"> %s%s%s%s%s%s%s\
	</td><td class=\"ff1\">\
	%s</td><td class=\"ff2\">\
	<a class=\"%s\" href=\""SELF"?folder=%s&sby=%c&p=%d&msg=%s\">%s</a></td><td class=\"ff3\">\
	%s</td><td class=\"ff4\">\
	%d K</td></tr>",
	"even",
	msg->file,
	(msg->flags & FL_SEEN) ? "" : IMG_UNSEEN,
	(msg->flags & FL_ATTACH) ? IMG_ATTACH : "",
	(msg->flags & FL_SIGNED) ? IMG_SIGNED: "",
	(prio_img) ? prio_img : "",
	(msg->flags & FL_FLAGGED) ? IMG_FLAGGED : "",
	(msg->flags & FL_PASSED) ? IMG_FWD: "",
	(msg->flags & FL_REPLIED) ? IMG_REPLIED: "",
	(current.ff == FF_ISSENT) ? msg->to : msg->from ? msg->from : "Отправитель неизвестен",
	class,
	ubuf, 
	current.prefs.sby ? current.prefs.sby : 'f',
	current.sf,
	msg->file,
	msg->subject ? msg->subject : "<Без темы>",
	msg->date,
	msg->size 
	);
	
	return strlen(buf);
}


int (*get_sort_func(void))(const void *,const void *) {
	char p;

	p = current.prefs.sby;
	if (!p) return cmp_msg_date;
	switch (p) {
		case SBY_SUBJ: return ISDESC ? cmp_msg_subj_r : cmp_msg_subj; 
		case SBY_DATE: return ISDESC ? cmp_msg_date_r : cmp_msg_date; 
		case SBY_FROM: return ISDESC ? cmp_msg_from_r : cmp_msg_from;
		case SBY_TO:   return ISDESC ? cmp_msg_to_r : cmp_msg_to;
		case SBY_SIZE: return ISDESC ? cmp_msg_size_r : cmp_msg_size;
	}
	return cmp_msg_date;
}

int bc;

char *out_wr(collect_t *base,void (*accum)(char **,int,unsigned int,unsigned int,char *,char *,short)) {
        register int i;
        int len,ndots;
        char *buf,*tp,*folder;
	short op;

        len = base->len;
        tp = buf = (char *) malloc(sizeof(char) * len * FOLDER_LEN * 10 + 1024);
        if (!buf) {
                SETERR(E_NOMEM,NULL);
                return NULL;
        }

        qsort(base->base,len,sizeof(struct vals),cmp_utf);

	bc = 0;
        for (i = 0; i < len; ++i) {
		op = strcmp(base->base[i].name,current.folder) ? 0 : 1;
                ndots = numdots(base->base[i].name);
                if (ndots) {
                        folder = strrchr(base->base[i].name,'.');
                        ++folder;
                } else folder = base->base[i].name;

                folder = do_utf7(folder);
		accum(&buf,ndots,base->base[i].nmes,base->base[i].tmes,base->base[i].name,folder,op);
		free(folder);
        }
        return tp;

}

#define MAX_ENT		32
#define LINK_LEN	4096

void accum_left(char **buf, int margin, unsigned int nmes, unsigned int tmes, char *what, char *folder, short isop) {
		unsigned char ubuf[FOLDER_LEN];
		char data[PATH_LEN];
		char *op;
		static int pmargin,pbc,pxbc;
		static int ent[MAX_ENT];
		short flag;
		char link[LINK_LEN];
		

		if (!bc) {
			pmargin = pbc = pxbc = 0;
			bzero(ent,sizeof(int)*MAX_ENT);
		}
		++bc;

		op = ISOPEN(isop);
                url_encode((unsigned char *)what,ubuf);

		if (pmargin < margin) {
			ent[margin] = pbc;
		} else if (pmargin > margin) {
		//	ent[margin] = ent[pmargin-margin];
		} 
			
		if (current.prefs.sby && current.sf && current.prefs.sby_order && isop) {
			snprintf(link,LINK_LEN,"folder=%s&sby=%c&sbyo=%c&p=%d",
			ubuf,current.prefs.sby,current.prefs.sby_order,current.sf);
		} else 
			snprintf(link,LINK_LEN,"folder=%s",ubuf);
		
		if (!nmes) {
			if (tmes)
                	sprintf(data,"<img class=\"y\" border=\"0\" src=\""IMGDIR"/%s\">&nbsp;\
                        	<a class=\"lf\" href="SELF"?%s>%s (%u)</a>",op,link,alias(folder),tmes);
                        //	<a class=\"lf\" href="SELF"?folder=%s>%s (%u)</a>",op,ubuf,alias(folder),tmes);
			else
                	sprintf(data,"<img class=\"y\" border=\"0\" src=\""IMGDIR"/%s\">&nbsp;\
                        	<a class=\"lf\" href="SELF"?%s>%s</a>",op,link,alias(folder));
		}
		else sprintf(data,"<img class=\"y\" border=\"0\" src=\""IMGDIR"/%s\">&nbsp;\
                        <strong><a class=\"lf\" href="SELF"?%s>%s (%u/%u)</div></a></strong>",
			op,link,alias(folder),nmes,tmes);
		
 //               sprintf(data,"<img border=\"1\" src=\""IMGDIR"/%s\">&nbsp;",op);
//		else sprintf(data,"<img border=\"1\" src=\""IMGDIR"/%s\">",op);
		
		flag = (margin) ? 0 : JSFL_DISP;
		if (isop) flag |= JSFL_CURR;

		sprintf(*buf,"M.add(%d,%d,'%s',%d)\n",
		ent[margin],
		bc,
		data,
		flag
		);

		pbc = bc;
	/*	if (pmargin < margin) pxbc = bc;
		else if (pmargin > margin) pxbc =
	*/

	//	if (!ent[margin] && (pmargin == margin)) ent[margin] = bc;
		pmargin = margin;

                *buf += strlen(*buf);
}

void accum_copy(char **buf, int margin, unsigned int nmes, unsigned int tmes, char *what, char *folder, short isop) {
		char *sp,*wsp;
		int p;

		sp = wsp = (char *) malloc(sizeof(char) * 7 * (margin + 1));
		if (!wsp) {
			SETERR(E_NOMEM,NULL);
			return;
		}

		for (p = 0; p <= margin; ++p) {
			memcpy(wsp,"&nbsp; ",7);
			wsp += 7;
		}
		*wsp = 0;
                sprintf(*buf,"<option value=\"%s\">%s %s</option>",what,sp,alias(folder));
                *buf += strlen(*buf);
		free(sp);
}
