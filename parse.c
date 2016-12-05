#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#define _GNU_SOURCE

#include <string.h>
#include <ctype.h>
#include <iconv.h>

#include "fcgi_stdio.h"
#include "message.h"
#include "session.h"
#include "driver.h"
#include "utils.h"
#include "parse.h"
#include "env.h"
#include "wmail.h"


subst_t asubst[] = {
	{ '<',"&lt;", 4 },
	{ '>',"&gt;", 4 },
	{ '&',"&amp;", 5 },
	{ '\n',"<br />", 6 },
	{ '"', "&quot;",6 },
	{ 0,NULL,0 }
};

extern gb_t gb;

static void parse_mix(char **,char **,char *) ;

int parse_cte(message_t *msg, char *ctype) {
	mime_t *mime;

	if (!msg->p0) mime = alloc_mime();
	else mime = msg->p0;

	parse_mime_cte(msg,mime,ctype);
	msg->p0 = mime;

	return 0;
}

int parse_mime_cte(message_t *msg, mime_t *mime, char *ctype) {
	skipsp(&ctype);

	if (!strncasecmp(ctype,"7bit",4)) mime->enc = CTE_7BIT;
	else if (!strncasecmp(ctype,"base64",6)) mime->enc = CTE_BASE64;
	else if (!strncasecmp(ctype,"quoted-printable",16)) mime->enc = CTE_QP;
	else mime->enc = CTE_UNDEF;

	return 0;
}
	 

int parse_mime_cdisp(message_t *msg, mime_t *mime, char *cdisp) {
	char *ptr;

	ptr = strchr(cdisp,';');
	if (ptr) *ptr++ = 0;

	if (!strncasecmp(cdisp,"attachment",10)) {
		mime->flags &= ~MFL_ISDISPLAY;
		mime->flags |= MFL_CATT;
	} else if (!strncasecmp(cdisp,"inline",6)) {
		
	//	mime->flags |= MFL_INLINE;
	}

	if (parse_ct_args(mime,ptr)) return 1;
	
	return 0;
}


int parse_cdisp(message_t *msg,char *cdisp) {
	mime_t *mime;

	if (!msg->p0) mime = alloc_mime();
	else mime = msg->p0;

	parse_mime_cdisp(msg,mime,cdisp);
	msg->p0 = mime;

	return 0;
}

static void skip_chars(char **str) {
	int len;

	len = strlen(*str);
	if (**str == '<' &&
		*((*str) + len - 1) == '>') {
		*((*str) + len - 1) = 0;
		(*str)++;
	}
}

int parse_mime_ctid(message_t *msg, mime_t *mime, char *cdisp) {

	skipsp(&cdisp);
	skip_chars(&cdisp);
	mime->ctid = cdisp;
//	print_hello();
//	printf("c=%s<br>\n",cdisp);
	return 0;
}

int parse_ctid(message_t *msg,char *ctid) {
	mime_t *mime;

	if (!msg->p0) mime = alloc_mime();
	else mime = msg->p0;

	parse_mime_ctid(msg,mime,ctid);
	msg->p0 = mime;

	return 0;
}

int parse_ctype_m(message_t *msg, char *ctype) {
	mime_t *mime;

	if (!msg->p0) mime = alloc_mime();
	else mime = msg->p0;
	
	if (parse_mime(msg,mime,ctype)) return 1;
	msg->p0 = mime;

	return 0;
}

int parse_mime(message_t *msg, mime_t *mime, char *ctype) {
	char *ptr;

	ptr = strchr(ctype,';');
	if (ptr) *ptr++ = 0;

//	print_hello();
//	if (ptr) printf("CT=%s<br>",ctype);
	mime->type = ctype;
	if (!strncasecmp(ctype,"text",4)) {
		ctype += 4;
		if (*ctype++ != '/') return 1;
		if (!strncasecmp(ctype,"html",4)) mime->flags |= MFL_ISOUR;
		if (mime->childs) {
			free(mime->childs);
			mime->childs = 0;
			mime->total = 0;
		}
		mime->flags |= MFL_ISDISPLAY;
		if (mime->parent && (mime->parent->flags & MFL_ISALTER) && !(mime->parent->flags & MFL_ALTERFOUND)) {
			clear_disp(mime->parent);
			if (mime->flags & MFL_ISOUR) {
				mime->parent->flags |= MFL_ALTERFOUND;
			} 
			mime->flags |= MFL_ISDISPLAY;
		}
	} else if (!strncasecmp(ctype,"multipart",9)) {
		ctype += 9;
		if (*ctype++ != '/') return 1;
	 
		mime->type = ctype;
		mime->flags &= ~MFL_LAST;
		mime->flags &= ~MFL_ISDISPLAY;
	
		if (!strncasecmp(ctype,"alternative",11)) {
			mime->flags |= MFL_ISALTER;
		} else if (!strncasecmp(ctype,"parallel",8)) {
			mime->flags |= MFL_ISPARALLEL;
		} else if (!strncasecmp(ctype,"signed",6)) {
			if (msg) msg->flags |= FL_SIGNED;
			/* TODO 
			   attach lookup
			*/
		} else if (!strncasecmp(ctype,"related",7)) {
			mime->flags |= MFL_RELATED;
			if (msg) msg->flags |= FL_RELATED;
			if (mime->parent) clear_disp(mime->parent);
		}
		if (msg) msg->flags |= FL_ATTACH;
	} else if (!strncasecmp(ctype,"message",7)) {
		if (!(mime->flags & MFL_CATT)) mime->flags |= (MFL_ISDISPLAY | MFL_MESSAGE);
	} else {
		mime->flags &= ~MFL_ISDISPLAY;
		if (msg) {
			msg->flags |= FL_ATTACH;
		}
	}

	if (parse_ct_args(mime,ptr)) return 1;

	return 0;
}

int parse_ct_args(mime_t *mime,char *ptr) {
	char *p1,*p2,*tp;
	char sc;
	int len;

	for (p1 = strtok(ptr,";"); p1; p1 = strtok(NULL,";")) {
		p2 = strchr(p1,'=');
		if (!p2) return 1;
		*p2++ = 0;
		tp = p2;
		if (*p2 == '\"' || *p2 == '\'') {
			sc = *p2;
			tp = ++p2;
			while (*p2 != sc) {
				if (*p2 == 0) return 1;
				++p2;
			}
			*p2++ = 0;
		} else {
			len = strlen(tp);
			if (tp[len-2] == '\r' && tp[len-1] == '\n') {
				tp[len-2] = 0;
			} else if (tp[len-1] == '\r' || tp[len-1] == '\n') {
				tp[len-1] = 0;
			}
		}
		skipsp(&p1);
		if (mime->flags & MFL_LAST) {
			if (!strncasecmp(p1,"charset",7)) {
				mime->charset = tp;
			} else if (!strncasecmp(p1,"name",4)) {
				mime->name = tp;
			} else if (!strncasecmp(p1,"filename",8)) {
				mime->filename = tp;
			}
		} else {
			if (!strncasecmp(p1,"boundary",8)) {
				mime->boundary = tp;
			} else if (!strncasecmp(p1,"protocol",8)) {
			} else if (!strncasecmp(p1,"micalg",6)) {
			}
		}
	}
	return 0;
}

int parse_ctype(message_t *msg, char *ctype) {
	char *ptr;

	ptr = strchr(ctype,';');
	if (ptr) *ptr = 0;

	if (!strncmp(ctype,"multipart",9)) {
		ctype += 9;
		if (*ctype++ != '/') return 1;
		if (!strncasecmp(ctype,"alternative",11) || 
			!strncasecmp(ctype,"parallel",8) 
		) return 0;

		if (!strncasecmp(ctype,"signed",6)) {
			msg->flags |= FL_SIGNED;
			/* TODO 
			   attach lookup
			*/
			return 0;
		}
		msg->flags |= FL_ATTACH;
	} else if (!strncmp(ctype,"text",4)) {

	} else msg->flags |= FL_ATTACH;

	return 0;
}
char *rfc2047(char *subj,char **saved) {
	char *ptr = subj;
	char *p1,*p2,sc,enc,*charset;
	int len,sb;
	iconv_t icv;
	char *buf = NULL,*sbuf;
	size_t inb,outb;
	char *bb,*tbb;
	char *ssp;
	register int cnt = 0;


	ptr = strstr(ptr,"=?");
	if (!ptr) return NULL;

	ptr += 2;
	charset = p1 = ptr;


	while (*p1 != '?') {
		/* charset len */
		if (++cnt > 16) {
			return NULL;
		}
		++p1;
	}

	p2 = p1;
	sc = *p2;
	*p2++ = 0;
	enc = *p2++;
	if (*p2++ != '?') {
		*p1 = sc;
		return NULL;
	}
	ssp = strchr(p2,0x20);
	//if (ssp) *ssp = 0;

	ptr = strstr(p2,"?=");
	if (!ptr) {
		if (ssp) *ssp = 0x20;
		return NULL;
	}

	*ptr++ = 0;

	if (saved) *saved = ++ptr;

	if (ssp) *ssp = 0x20;

	if (enc == 'B' || enc =='b') 
		buf = (char *) base64_decode((unsigned char *)p2,strlen(p2),&len);
	else if (enc == 'q' || enc == 'Q') {
		buf = quoted_printable_decode(p2,buf,1);
	}
	else {
		*p1 = sc;
		return NULL;
	}

	sbuf = buf;

	if (is_our_charset(charset)) {
		*p1 = sc;
		return buf;
	}

	icv = iconv_open(DEFAULT_CHARSET,charset);
        if (icv == (iconv_t)-1) {
		*p1 = sc;
		return buf;
	}

	inb = strlen(buf);
	sb = outb = 3 * (inb + 1);
	bb = tbb = (char *) malloc(sizeof(char) * outb);
	if (!bb) {
		*p1 = sc;
		iconv_close(icv);
		return buf;
	}

        if ((iconv(icv,&buf,&inb,&bb,&outb)) == -1) {
		*p1 = sc;
		free(tbb);
                iconv_close(icv);
                return sbuf;
        }
        iconv_close(icv);


	*p1 = sc;
	free(sbuf);

	sb -= outb;
	tbb[sb] = 0;

/*	ptr += 2;
	subj = ptr;
*/
	return tbb;
}

int parse_mime_subject(message_t *msg, mime_t *mime, char *subj) {
	return parse_subject_x(&mime->subject,subj);
}

int parse_subject(message_t *msg, char *subj) {
	unsigned char *pt;

	for (pt = (unsigned char *) subj; *pt; pt++) {
		if (*pt >= 0x7f) {
			msg->pflags |= PFL_BSUBJ;
			break;
		}
	}
	return parse_subject_x(&msg->subject,subj);
}

int pparse_subj(message_t *msg, char *subj) {
	iconv_t icv;
	char *bb, *tbb;
	size_t sb, inb, outb;
	char *charset;

	if (!(msg->pflags & PFL_BSUBJ)) return 0;

	charset = (msg->p0 && msg->p0->charset) ? msg->p0->charset : "windows-1251";

	if (is_our_charset(charset)) return 0;
	
	icv = iconv_open(DEFAULT_CHARSET,charset);
        if (icv == (iconv_t)-1) return -1;

	inb = strlen(subj);
	sb = outb = 3 * (inb + 1);
	bb = tbb = (char *) malloc(sizeof(char) * outb);
	if (!bb) {
		iconv_close(icv);
		return 0;
	}

        if ((iconv(icv,&subj,&inb,&bb,&outb)) == -1) {
		free(tbb);
                iconv_close(icv);
                return 0;
        }

	sb -= outb;
	tbb[sb] = 0;

	msg->subject = tbb;
	iconv_close(icv);

	return 0;
	
}

int parse_subject_x(char **pt, char *subj) {
	int len,res = 0;
	char *pp, *ptr = subj;
	sc_t sc[MAX_TOKENS];
	char *em = NULL;
	char *sptr = NULL, *nbuf = NULL;
	unsigned short j = 0,i = 0;
	int l_len;
	char *tpr;

	pp = ptr;
	len = strlen(subj);
	sc[i].ptr = pp;
	sc[i++].fd = 0;

	while (ptr && (ptr = strchr(ptr,'='))) {
		em = rfc2047(ptr,&sptr);
		if (!em) {
		} else {
			if (sc[i-1].ptr == ptr) --i;
			*ptr = 0;
			sc[i].ptr = em;
			sc[i++].fd = 1;
			if (*sptr && !is_blank(sptr)) {
				sc[i].ptr = ptr = sptr;
				sc[i++].fd = 0;
			} else *sptr = 0;
			if (i == MAX_TOKENS - 1) break;
		}
		if (sptr && *sptr) ptr = sptr;
		else ++ptr;
	}
	--i;
	if (!i) {
		*pt = sc[0].ptr;
		if (sc[0].fd) gb.data[gb.len++] = sc[0].ptr;
	} else {
		nbuf = malloc(sizeof(char) * len * 2);
		if (!nbuf) {
			SETERR(E_NOMEM,NULL);
			return 0;		
		}
		*nbuf = 0;
		for (j = 0; j <= i; ++j) {
			strcat(nbuf,sc[j].ptr);
			if (sc[j].fd) free(sc[j].ptr);
		}
		*pt = nbuf;
		gb.data[gb.len++] = nbuf;
	}

	pp = *pt;
	l_len = strlen(*pt) + 1;
	*pt = do_subst(pp,&l_len,1);
	tpr = *pt;

	tpr[l_len - 1] = 0;
	if (pp != *pt) {
		gb.data[gb.len++] = *pt;
	}
	return res;
}

char *parse_email_from(char *email) {
	char *p1 = email,*ptr;

	if (*email == '<') {
		p1 = ++email;
		ptr = strchr(email,'>');
		if (!ptr) {
			SETWARN(E_MSGPARSE,p1);
			return NULL;
		}
		*ptr++ = 0;
	} else {
		ptr = strchr(email,'>');
		if (ptr) *ptr = 0;
	}
	if (validate_email(email) < 0) return NULL;

	return p1;
}

char *parse_who_from(char *who) {
	char *p1 = who, *ptr;

	skipsp(&who);
	ptr = who;
	while (*ptr) {
		if (*ptr == '<' || *ptr == '>') *ptr = 0x20;
		++ptr;
	}
	if (*who == '(') {
		ptr = strrchr(who,')');
		if (!ptr) return p1;
		++p1;
		*ptr = 0;
	} 
	else if (*who == '"') {
		ptr = strrchr(who,'"');
		if (!ptr) return p1;
		++p1;
		*ptr = 0;
	}


	return p1;
}

int parse_to(message_t *msg, char *to) {
	parse_mix(&msg->to,&msg->email_to,to);
//	printf("Content-type: text/html\n\n");
//	printf("DA<br>");
	return 0;
}
int parse_rpto(message_t *msg, char *to) {
	parse_mix(&msg->replyto,&msg->email_rpto,to);
	return 0;
}

int parse_mime_from(message_t *msg, mime_t *mime, char *from) {
	parse_mix(&mime->from,&mime->email_from,from);
	return 0;
}

int parse_from(message_t *msg, char *from) {
	parse_mix(&msg->from,&msg->email_from,from);
	return 0;
}

void add(char **pv0,char *w,int col) {
	int l1,l2;
	int addon = 0;

	if (!*pv0) {
		*pv0 = strdup(w);
		return ;
	} else {
		l1 = strlen(w);
		l2 = strlen(*pv0);
		*pv0 = realloc(*pv0,sizeof(char) * (l1 + l2 + 10));
		if (!*pv0) {
			SETERR(E_NOMEM,NULL);
			return ;
		}
		if (col) {
			memcpy(*pv0 + l2," ",1);
			addon = 1;
		}
		memcpy(*pv0 + l2 + addon, w, l1);
		memcpy(*pv0 + l1 + l2 + addon, "\0",1);
	}
	return ;

}


char *show_filename(char *fname) {
	char *em;

	if (!fname) return DUMMY_FILE;
	em = rfc2047(fname,NULL);
	if (em) {
		gb.data[gb.len++] = em;
		return em;
	}

	return fname;
}

static void parse_mix(char **pv0,char **pv1,char *from) {
	char *pp , *ptr = from;
	int ft = 1; // was in cycle
	char *col = NULL;
	char *em;
	char *sp,*sptr;
	int ntb = 0, sps = 0;
	

/*	*pv0 = strdup("aa@aa.ru");
	*pv1 = strdup("aa@aa.ru");
	return;
*/
	pp = ptr;
	while (1) {
	//	printf("pre - ptr=%x %c pp=%s<br>",*ptr,*ptr,pp);
		if (!*ptr) ntb = 1;
		em = NULL;
		if (*ptr == 0x20 || !*ptr) {
			if (*ptr == 0x20) *ptr = 0; //else *++ptr = 0;
			em = rfc2047(pp,&sptr);
	
	//		em = NULL;
			if (em) {
				if (col) {
					add(pv0,parse_who_from(col),sps);
					col = NULL, ft = 1;
				}
				add(pv0,em,sps);
				gb.data[gb.len++] = em;
				if (sptr && *sptr) {
					pp = ptr = sptr;
					sps = 0;	
					continue;
				} 
				sps = 1;
				pp = ptr + 1;
			} else {
				if (!*pv1) {
					em = parse_email_from(pp);
					*pv1 = em;
				}
				if (em)	pp = ptr + 1;
				else if (*pp) {
					if (ft) {
						col = pp;
						ft = 0;
						sp = ptr;
					} else {
						*sp = 0x20;
						sp = ptr;
					}
					pp = ptr + 1;
				} else pp = ptr + 1;
			}
		} else if (*ptr == '<') {
			*ptr = 0x20;
			continue;
		}
		if (ntb) break;	
		++ptr;
	}

	if (col) add(pv0,parse_who_from(col),1);
	if (!*pv1) {
		*pv1 = parse_email_from(pp);
		*pp = 0;
	} 
	if (!*pv0) *pv0 = *pv1;


	return;
/*
	pp = ptr;
	while ((ptr = strchr(ptr,0x20))) {
		em = NULL;
		wic = 1;
		*ptr = 0, ++ptr;


		em = rfc2047(pp,NULL);
		if (em) {
			add(pv0,em,0);
			gb.data[gb.len++] = em;
		} else {
			if (!*pv1) {
				em = parse_email_from(pp);
				*pv1 = em;
			}
			if (!em) {
				if (ft) col = pp, ft = 0;
				--ptr, *ptr = 0x20, ++ptr;
			}
		}

		pp = ptr;
	}

	np = strchr(pp,'<');
	if (np) *np = 0;

	if (!wic) {
		*pv0 = rfc2047(pp,NULL);
		if (!*pv0) {
			*pv0 = *pv1 = parse_email_from(pp);
			if (!*pv0) *pv0 = parse_who_from(pp);
		} else {
			gb.data[gb.len++] = *pv0;
		}
	} else {
		if (!*pv1) {
			*pv1 = parse_email_from(pp);
			*pp = 0;
		}
		if (col) *pv0 = parse_who_from(col);
		else if (pp && *pp) *pv0 = parse_who_from(pp);
		if (!*pv0) *pv0 = *pv1;
	}


	return ; */
}

int parse_xpriority(message_t *msg, char *prio) {
	int res = 0;
	
	msg->prio = atoi(prio);
//	if (*prio == '1') msg->prio = 1;
//	else msg->prio = 0;

	return res;
}

static void add_val(char **base0,char **base1,int l0,int l1,char *w0,char *w1) {

	l0++;
	l1++;

	if (!**base0) {
		strncpy(*base0,w0,l0);
	} else {
		strncat(*base0,", ",2);
		strncat(*base0,w0,l0);
	}

	if (l1 == 1) return;

	if (!**base1) {
		strncpy(*base1,w1,l1);
	} else {
		strncat(*base1,",",1);
		strncat(*base1,w1,l1);
	}
	
}


int parse_cc(message_t *msg, char *cc) {
	char *p;
	char **cl;
	char **cle;
	int i,total=0;
	int bsize = 1024;
	int l0,l1;

	if (!msg->cc) msg->cc = (char *) malloc(sizeof(char) * bsize);
	if (!msg->cc) return -1;
	if (!msg->cc_email) msg->cc_email = (char *) malloc(sizeof(char) * bsize);
	if (!msg->cc_email) return -1;

	*msg->cc = *msg->cc_email = 0;

	cl = (char **) malloc(sizeof(char *) * MAX_CC);
	if (!cl) return -1;
	cle = (char **) malloc(sizeof(char *) * MAX_CC);
	if (!cle) {
		free(cl);
		return -1;
	}

	for (i = 0,p = strtok(cc,","); (p && i<MAX_CC); p = strtok(NULL,","),++i) {
		skipsp(&p);
	
		cl[i] = 0, cle[i] = 0;	
		parse_mix(&cl[i],&cle[i],p);
	
		l0 = strlen(cl[i]);
		l1 = (cle[i]) ? strlen(cle[i]) : 0;
		
		if (l0 > l1) total += l0; else total += l1;
		if (total > bsize) break;
		
		add_val(&msg->cc,&msg->cc_email,l0,l1,cl[i],cle[i]);
	}

	free(cl);
	free(cle);
	return 0;
}

// rfc822
int parse_rcvd(message_t *msg, char *rcvd) {
	char *ptr,*p;

	ptr = strrchr(rcvd,';');
	if (!ptr) return 1;

	p = ++ptr;
	skipsp(&p);

	return parse_date(msg,p);
}


int parse_date(message_t *msg, char *rcvd) {
	int hm;
	char *ptr,*p = rcvd;
	char *date;

	if (!isdigit(*p)) {
		p = strchr(p,0x20);
		if (!p) return 1;
		++p;
	}

	skipsp(&p);
	ptr = p;
	if (!isdigit(*p)) return 1;
	if (*++p != 0x20) {
		if (!isdigit(*p)) return 1;
		if (*++p != 0x20) return 1;
		hm = 2;
	} else hm = 1;

	++p;
	date = (char *) malloc(sizeof(char) * 16);
	if (!date) return 1;
	
	if (hm == 1) {
		date[0] = '0';
		date[1] = *ptr;
	} else {
		date[0] = *ptr++;
		date[1] = *ptr;
	}

	date[2]='.';	

	ptr = strchr(p,0x20);
	if (!ptr) {
		free(date);
		return 1;
	}
	*ptr++ = 0;

	set_month(&date[3],p);
	date[5] = '.';

	p = ptr;
	ptr = strchr(p,0x20);
	if (!ptr) {
		free(date);
		return 1;
	}

	hm = ptr - p;
	if (hm == 4) memcpy(&date[6],p + 2,2);
	else if (hm == 2) memcpy(&date[6],p,2);
	else {
		free(date);
		return 1;
	}

	++ptr;
	date[8] = 0x20;
	memcpy(&date[9],ptr,5);
	date[14] = 0;


	if (!msg->date) msg->date = date;
	else msg->senddate = date;

	return 0;
}

char *post_cdisp(char *ptr) {
	char *p,*dp;
	int len;

	if (!ptr || !*ptr) return NULL;

	p = strstr(ptr,"filename=");
	if (!p) return NULL;
	
	p += 9;
	if (!p || !*p) return NULL;	
	if (*p == '\'' || *p == '"') ++p;

	dp = strchr(p,';');
	if (dp) *dp = 0;

	len = strlen(p);
	if (len < 1) return NULL;
	if (p[len-1] == '\'' || p[len-1] == '"') p[len-1] = 0;

	if (isalpha(p[0]) && p[1] == ':' && p[2] == '\\') {
		p = strrchr(p,'\\');
		if (p) ++p;
	}
	return p;


}
