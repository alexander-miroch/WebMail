#include <string.h>
#include <setjmp.h>
#include <ctype.h>
#include "fcgi_stdio.h"
#include "wmail.h"
#include "params.h"
#include "filter.h"
#include "abook.h"
#include "message.h"

extern jmp_buf jenv;

int inline in_chr_e(char *str,char *chrs,int len) {
        char *p,*pp;
	short dog = 0, dots = 0;
	int cnt = 0;

	if (*str == '.') return -1;
        for (p = str; *p; p++) {
                for (pp = chrs; *pp; pp++) {
                        if (*p == *pp) return -1;
                }
		++cnt;
		if (cnt > len) return -1;
		if (*p == '@') ++dog;
		else if (*p == '.') ++dots;

        }
	if (dog != 1 || !dots) return -1;
	--p; if (*p == '.') return -1;
	--p; if (*p == '.') return -1;

        return 1;
}

int inline in_chr_f(char *str,char *chrs,int len) {
        char *p,*pp;
	int cnt = 0;
	int dotfound = 0;

        for (p = str; *p; p++) {
                for (pp = chrs; *pp; pp++) {
                        if (*p == *pp) return -1;
                }
		++cnt;
		if (cnt > len) return -1;
		if (*p == '.') {
			if (dotfound) return -1;
			dotfound = 1;
		} else dotfound = 0;
		if (!isascii(*p)) return -1;
        }
	return 1;
}

int inline in_chr_sf(char *str,char *chrs,int len) {
        char *p,*pp;
	int cnt = 0;

        for (p = str; *p; p++) {
                for (pp = chrs; *pp; pp++) {
                        if (*p == *pp) return -1;
                }
		++cnt;
		if (cnt > len) return -1;
        }
	return 1;
}
int inline in_chr_p(char *str, char *chrs, int len) {
        char *p,*pp;
	int cnt = 0;

	for (p = str; *p; p++) {
		if (chrs) {
			for (pp = chrs; *pp; pp++) {
				if (*p == *pp) return -1;
			}
		}
		++cnt;
		if (cnt > len) return -1;
		if (!isascii(*p)) return -1;
	}
	return 1;

}

int inline in_chr(char *str, char *chrs, int len) {
        char *p,*pp;
	int cnt = 0;

	for (p = str; *p; p++) {
		if (chrs) {
			for (pp = chrs; *pp; pp++) {
				if (*p == *pp) return -1;
			}
		}
		++cnt;
		if (cnt > len) return -1;
	}
	return 1;

}
int validate_msg(char *v) {
	if (!isdigit(*v)) return -1;
	return in_chr_f(v,"/\n\t",MESSAGE_LEN);
}

int validate_folder(char *v) {
	if (*v == '.') {
		++v; 
		if (*v) return -1; else return 1;
	}

	return in_chr_f(v,"/\n\t",FOLDER_LEN);
}

int validate_cfolder(char *v) {
	if (isspace(*v)) return -1;
/*	if (*v == '.') {
		++v; 
		if (*v) return -1; else return 1;
	}*/
	return in_chr_sf(v,"/.",FOLDER_LEN);
}

int validate_subfolder(char *v) {
	if (*v == '.') {
		++v; 
		if (*v) return -1; else return 1;
	}

	return in_chr_f(v,"/\n\t",FOLDER_LEN);
}

int validate_sby(char c) {
	if (!islower(c) || !isascii(c)) return -1;
	return 1;
}

int validate_password(char *p, int len) {
	return in_chr_p(p,NULL,MAX_PASSWD_LEN);
}

int validate_cc(char *cc) {
	char *p,*sf = cc;
	unsigned short i;

	if (!cc || !*cc) return 0;
	for (i = 0,p = strchr(sf,','); p; p = strchr(p,','), ++i) {
		*p = 0;
		if (validate_email(sf) < 0) return -1;
		*p++ = ',', sf = p;
		if (i > MAX_CC) return -1;
	}

	return validate_email(sf);
}

int validate_sid(char *sid) {
	char *p = sid;
	int l = 0;

	while (*p) {
		++l;
		if (l > 32) return -1;
		if (!isxdigit(*p)) return -1;
		++p;
	}
	return 0;
}

int validate_abnick(char *nick) {
	return in_chr(nick,"\'\"<>;\n",AB_NAME_LEN);
}

int validate_abtel(char *tel) {
	return in_chr(tel,"<$>;&#!=\n",AB_TEL_LEN);
}

int validate_mark(int mark) {
	if (mark < 1 || mark > MAX_MARK) return -1;
	return 0;
}

int validate_email(char *email) {

	return in_chr_e(email,"\'\"^,+!#$<>\n\t?&*()%;:/\\ ",EMAIL_LEN);
}

int validate_todel(char *td) {
	int cnt = 0;;
	if (*td != '_') return -1;

	++td;
	while (*td) {
		if (!isdigit(*td)) return -1;
		++td, ++cnt;
		if (cnt > 10) return -1;
	}
	return 1;
}
int validate_rg(int v,int p) {
	if (v > p) return -1;
	if (v < 0 || p > MAX_OFFSET) return -1;	
	if (v < 0 || v > MAX_OFFSET) return -1;
	return 1;
}

int validate_prname(char *v) {
	return in_chr(v,"#?<>\\/@\n\t\r",MAX_PRNAME);
}

int validate_sf(int v) {
	if (v < 0 || v > MAX_PAGES) return -1;	
	return 1;
}

int validate_cf(int v) {
	if (v < 0 || v > CF_MAX) return -1;	
	return 1;
}

int validate_prio(int v) {
//	if (v != CPRIO_HIGH && v != CPRIO_MED && v != CPRIO_LOW) return -1;
	if (v < CPRIO_HIGH || v > CPRIO_LOW) return -1;
	return 1;
}

int validate_todo(int v) {
	if (v < 0 || v > MAX_TODO) return -1;	
	return 1;
}
int validate_prpp(int v) {
	if (v < 1 || v > MAX_PP) return -1;	
	return 1;
}

int validate_prsign(char *v) {
	if (strlen(v) > MAX_SIGN) return -1;
	return 1;
}

int validate_fact(char *v) {
	if (strlen(v) > MAX_FACT_LEN) return -1;
	return 1;
}

int validate_field(int v) {
	if (v < 0 || v > MAX_HDR) return -1;
	return 1;
}

int validate_wtodo(int v) {
	if (!v) return 1;
	if (v != FACT_MOVE && v != FACT_COPY && v != FACT_DEL)
		return -1;
	return 1;
}

int validate_forder(int v) {
	if (v < 0 || v > MAX_FORDER) return -1;
	return 1;
}

int validate_fedit(char *v) {
	char *ptr = v;
	int i = 0;

	while(*ptr) {
		if (!isdigit(*ptr)) return -1;
		if (i > 7) return -1;
		++i;
		++ptr;
	}
	return 1;
}
