#include <stdlib.h>
#include <locale.h>
#include "fcgi_stdio.h"
//#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>
#include <ctype.h>
#include <sys/types.h>
#include <regex.h>

#include "wmail.h"
#include "message.h"


void inline zero_buf(char *buf,unsigned int s,unsigned int e) {
	unsigned int i;

	for (i = s; i < e; ++i) {
		buf[i] = 0x20;
	}
}

void inline trunc_buf(char *buf,unsigned int s) {
	buf[s] = 0;
}

void inline malform_buf(char *buf,unsigned int s,unsigned int e) {
	unsigned int i;

	for (i = s; i < e; ++i) {
		if (buf[i] == 0x20 || buf[i] == '\t') continue;
		buf[i++] = '_';
		buf[i] = '_';
		break;
	}
}

void inline malform_clean_buf(char *buf,unsigned int s,unsigned int e) {
	buf[e-3]='X';
	buf[e-2]='S';
	buf[e-1]='S';
}

void inline zero_tag(char *buf,unsigned int s) {
	char *p;

	p = &buf[s];
	while (*p!= '>') {
		*p++ = 0x20;
	}
}

enum {
	PZERO,
	PTRUNC,
	PMALFORM,
	PMALFORM_CLEAN,
	PZEROTAG,
};

#define R_MAX 512

int do_reg(char *buf,char *reg,int atype) {
	regex_t preg;
	size_t nmatch = 1;
	regmatch_t rm[1];
	char ebuf[256];
	int rv;
	static int rec = 0;

	if (rec > R_MAX) {
		SETERR(E_PARSE,NULL);
		return 0;
	}

	rv = regcomp(&preg,reg,REG_EXTENDED|REG_ICASE);
	if (rv) {
		regerror(rv,&preg,ebuf,256);
		SETERR(E_PARSE,ebuf);
		return 0;
	}

	//printf("test %s<br>",buf);
	rv = regexec(&preg,buf,	nmatch, rm,REG_NOTBOL);
	if (rv == REG_NOMATCH) {
	//	printf("NOT_MATCHED<br>");
		regfree(&preg);
		return 0;
	} else if (!rv) {
		char *ptr;
		int s,e;

		s = rm[0].rm_so;
		e = rm[0].rm_eo;
		if (atype == PZERO) zero_buf(buf,s,e);
		if (atype == PTRUNC) trunc_buf(buf,s);
		if (atype == PMALFORM) {
			malform_buf(buf,s,e);
		}
		if (atype == PMALFORM_CLEAN) {
			malform_clean_buf(buf,s,e);
		}
		if (atype == PZEROTAG) zero_tag(buf,s);
		
		ptr = buf + rm[0].rm_eo;
		regfree(&preg);
		++rec;
		do_reg(ptr,reg,atype);
		--rec;
	//	printf("MATCHED %d %d-%d<br>",nmatch,rm[0].rm_so,rm[0].rm_eo);
		return 1;
	} else {
		regerror(rv,&preg,ebuf,256);
		SETERR(E_PARSE,ebuf);
		return 0;
	}
}

char *do_html_subst(char *buf, int *len) {
	char *sbuf;
	int plen,tlen;

	
	sbuf = buf;
	plen = *len;
	//printf("Content-type:text/html\n\n");

	do_reg(buf,".*<body[^>]*>",PZERO);
	do_reg(buf,"</body>.*",PTRUNC);
	do_reg(buf,"<script[^>]*>.*</script>",PZERO);
	do_reg(buf,"<style[^>]*>.*</style>",PZERO);
	do_reg(buf,"<link[^>]*>",PZERO);
	do_reg(buf,"[[:space:]/]+style[[:space:]]*=",PMALFORM);
	do_reg(buf,"((&#0*61;?|&#x0*3D;?|=)|((u|&#0*85;?|&#x0*55;?|&#0*117;?|&#x0*75;?)[[:space:]]*(r|&#0*82;?|&#x0*52;?|&#0*114;?|&#x0*72;?)[[:space:]]*(l|&#0*76;?|&#x0*4c;?|&#0*108;?|&#x0*6c;?)[[:space:]]*(\\()))[[:space:]]*(&#0*34;?|&#x0*22;?|\"|&#0*39;?|&#x0*27;?|\')?[^>]*[[:space:]]*(s|&#0*83;?|&#x0*53;?|&#0*115;?|&#x0*73;?)[[:space:]]*(c|&#0*67;?|&#x0*43;?|&#0*99;?|&#x0*63;?)[[:space:]]*(r|&#0*82;?|&#x0*52;?|&#0*114;?|&#x0*72;?)[[:space:]]*(i|&#0*73;?|&#x0*49;?|&#0*105;?|&#x0*69;?)[[:space:]]*(p|&#0*80;?|&#x0*50;?|&#0*112;?|&#x0*70;?)[[:space:]]*(t|&#0*84;?|&#x0*54;?|&#0*116;?|&#x0*74;?)[[:space:]]*(:|&#0*58;?|&#x0*3a;?)",PZEROTAG);
	do_reg(buf,"[[:space:]\"'/](o|&#0*79;?|&#0*4f;?|&#0*111;?|&#0*6f;?)(n|&#0*78;?|&#0*4e;?|&#0*110;?|&#0*6e;?)[[:alnum:]]+[[:space:]]*=",PZEROTAG);
	do_reg(buf,"<[^>a-z]*s[[:space:]]*c[[:space:]]*r[[:space:]]*i[[:space:]]*p[[:space:]]*t",PMALFORM_CLEAN);
	do_reg(buf,"<[^>a-z]*e[[:space:]]*m[[:space:]]*b[[:space:]]*e[[:space:]]*d",PMALFORM_CLEAN);
	do_reg(buf,"<[^>a-z]*m[[:space:]]*e[[:space:]]*t[[:space:]]*a",PMALFORM_CLEAN);
	do_reg(buf,"<[^>a-z]*x[[:space:]]*m[[:space:]]*l",PMALFORM_CLEAN);
	do_reg(buf,"<[^>a-z]*b[[:space:]]*a[[:space:]]*s[[:space:]]*e",PMALFORM_CLEAN);
	do_reg(buf,"<[^>a-z]*j[[:space:]]*a[[:space:]]*v[[:space:]]*a",PMALFORM_CLEAN);
	do_reg(buf,"<[^>a-z]*o[[:space:]]*b[[:space:]]*j[[:space:]]*e[[:space:]]*c[[:space:]]*t",PMALFORM_CLEAN);
	do_reg(buf,"<[^>a-z]*i[[:space:]]*f[[:space:]]*r[[:space:]]*a[[:space:]]*m[[:space:]]*e",PMALFORM_CLEAN);
	do_reg(buf,"<[^>a-z]*f[[:space:]]*r[[:space:]]*a[[:space:]]*m[[:space:]]*e",PMALFORM_CLEAN);
	
	tlen = strlen(buf);
	*len += (tlen - plen);
	



	return sbuf;
}

