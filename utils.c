#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>


#include "fcgi_stdio.h"

#include "utils.h"
#include "unicode.h"
#include "wmail.h"
#include "driver.h"
#include "message.h"

extern struct error_struct error;

char *months[] = {
	"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec",NULL
};

//unsigned char *url_encode(unsigned char *s, unsigned char *t) {  
void url_encode(unsigned char *s,unsigned char *t) {  
	unsigned char *p, *tp;

	tp = t; 
	for(p = s; *p; p++) {  
		if((*p > 0x00 && *p < ',') || (*p > '9' && *p < 'A') ||  
		(*p > 'Z' && *p < '_') || (*p > '_' && *p < 'a') || 
		(*p > 'z' && *p < 0xA1)) {
			sprintf((char *)tp, "%%%02X", *p);  
			tp += 3;
		} else {
			*tp = *p;
			tp++;     
		}
	}
	*tp = '\0'; 
	return ;
}


int url_decode(char *str, int len) {
	char *dest = str;
	char *data = str;

	while (len--) {
		if (*data == '+') {
			*dest = ' ';
		} else if (*data == '%' && len >= 2 && isxdigit((int) *(data + 1))
			&& isxdigit((int) *(data + 2))) {
			*dest = (char) htoi(data + 1);
			data += 2;
			len -= 2;
		} else {
			*dest = *data;
		}
		data++;
		dest++;
	}
	*dest = '\0';
	return dest - str;
}

int htoi(char *s) {
	int value;
	int c;

	c = ((unsigned char *)s)[0];
	if (isupper(c))
		c = tolower(c);
	value = (c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10) * 16;

	c = ((unsigned char *)s)[1];
	if (isupper(c))
		c = tolower(c);
	value += c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10;

	return (value);
}
/*
char * build_path(char *email,char *folder,char *add) {
	char *path,*ptr;
	int len;

	len = strlen(MAIL_PATH) + strlen(email);
	if (folder) len += strlen(folder);
	if (add) len += 3;

	path = (char *) malloc(sizeof(char) * (len + 16)); // for slashes + Maildir
	if (!path) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}

	ptr = strchr(email,'@');
	if (!ptr) {
		SETERR(E_PARSE,email);
		return NULL;
	}

	*ptr = 0;
	ptr++;
	if (folder) {
		if (add) sprintf(path,"%s/%s/%s/Maildir/%s/%s",MAIL_PATH,ptr,email,folder,add);
		else sprintf(path,"%s/%s/%s/Maildir/%s",MAIL_PATH,ptr,email,folder);
	} else {
		sprintf(path,"%s/%s/%s/Maildir",MAIL_PATH,ptr,email);
	}
	ptr--;
	*ptr = '@';

	return path;
}
*/
int inline is_our_charset(char *c) {
	if (!strncasecmp(c,DEFAULT_CHARSET,5)) return 1;
	if (!strncasecmp(c,DEFAULT_CHARSET_ALIAS,4)) return 1;
	return 0;
}

int inline is_hbreak(char *val) {
        if ((*val) == '\n'){
		if ((*++val) == '\0') {
			return 1; 
		} else --val;
	}
        if ((*val) == '\r' && (*++val) == '\n' && (*++val) == '\0') return 1;
        return 0;
}

int inline is_blank(char *val) {
	if ((*val == '\r' && *++val == '\n') ||
		*val == '\n') return 1;
	return 0;
}
int inline is_boundary(char *val) {
	if (*val == '-' && *++val == '-') return 1;
	return 0;
}

char *do_utf7(char *s) {
	unicode_char *ch;
	int err;
	char *rv;

	ch = unicode_modutf7touc(s,&err);
	if (ch) {
		rv = unicode_utf8_fromu(ch,&err);
		free(ch);
	} else {
		SETERR(E_UTF,NULL);
		return NULL;
	}
	return rv;
}

char *do_toutf7(char *s) {
	unicode_char *uc;
	int err;
	char *rv;

	uc = unicode_utf8_tou(s,&err);
	if (uc) {
		rv = unicode_uctomodutf7(uc);
		free(uc);
	} else {
		SETERR(E_UTF,NULL);
		return NULL;
	}
	return rv;
}


void list_wr(collect_t *base, char *what, int n, int tm) {
	int idx;

	idx = base->len;
	base->base[idx].name = strdup(what);
	base->base[idx].nmes = n;
	base->base[idx].tmes = tm;
	base->len++;

	return ;
	
}

int cmp_utf(const void *p1,const void *p2) {
	struct vals *v1,*v2;

        v1 = (struct vals *) p1;
        v2 = (struct vals *) p2;

	if (!v1) return 1;
	if (!v2) return -1;
//	printf("Content-type:text/html\n\n");
//	printf("xxx=%s<br>",v1->name);
	return strcmp(v1->name,v2->name);
	//return strcmp(* (char * const *) (*v1)->name, * (char * const *) (*v2)->name);
}

void inline skipsp(char **s) {
	while (**s == ' ' || **s == '\t') (*s)++;;
}

int inline numdots(char *s) {
	char *p = s;
	int i = 0;

	while ((p = strchr(++p,'.'))) ++i;
	return i;
}

void inline set_month(char *array, char *p) {
	int i;

	for (i = 0; i < 12; ++i) {
		if (!strncmp(p,months[i],3)) {
			++i;
			if (i<10) array[0] = '0';
			else {
				i-=10;
				array[0] = '1';
			}
			sprintf(&array[1],"%d",i);
			break;
		}
	}
}

void free_cl(collect_t *cl) {
	register int i = 0;
	if (cl->base) {
		for (; i < cl->len; ++i) {
			free(cl->base[i].name);
		}
		free(cl->base);
	} 
	free(cl);
}

collect_t *init_cl(int size) {
	collect_t *cl;
	
	cl = (collect_t *) malloc(sizeof(collect_t));
        if (!cl) {
                SETERR(E_NOMEM,NULL);
                return NULL;
        }

        cl->len = 0;
        cl->base = (struct vals *) malloc(sizeof(struct vals) * size);
        if (!cl->base) {
                SETERR(E_NOMEM,NULL);
                return NULL;
        }


	return cl;
}

short int inline is_ascii(unsigned char *s) {
	unsigned char *ptr = s;

	while (*ptr) {
		if (*ptr > 127) return 0;
		++ptr;
	}
	return 1;
}

int inline min(int v1,int v2) {
	if (v1>v2) return v2; else return v1;
}
int inline max(int v1,int v2) {
	if (v1>v2) return v1; else return v2;
}

char *get_time(char *out,unsigned long addon,char *fmt) {
	time_t tv;
        struct tm *tmp;
        int b;

	if (time(&tv) == (time_t) -1) {
                SETWARN(E_TIME,NULL);
                return NULL;
        }
	tv += addon;
        tmp = localtime(&tv);
        if (!tmp) {
                SETWARN(E_TIME,NULL);
                return NULL;
        }
	if (fmt) strftime(out,TIME_SIZE,fmt,tmp);
	else b = strftime(out,TIME_SIZE,"%a, %d %b %Y %H:%M:%S +0400",tmp);
        if (!b) {
                SETWARN(E_TIME,NULL);
                return NULL;
        }
        out[b] = 0;
	
	return out;
}

char *seen(char *msg) {
	int len;
	int f = 0;
	char *ptr;

	ptr = strrchr(msg,',');
	if (!ptr) {
		SETWARN(E_MSGPARSE,msg);
		return NULL;
	}

	while (*ptr) {
		if (*ptr == 'S') {
			f = 1;
			break;
		}
		++ptr;
	}

	if (!f) {
		len = strlen(msg);
		ptr = (char *) malloc(sizeof(char) * len + 2);
		if (!ptr) {
			SETWARN(E_NOMEM,NULL);
			return NULL;
		}

		memcpy(ptr,msg,len);
		memcpy(ptr+len,"S\0",2);
	} else return NULL;

	return ptr;
}

int inline get_s(int size,int pp) {
	int s;

	s = (size / pp);
	if (size % pp) ++s;
	s = (s > MAX_PAGES) ? MAX_PAGES : s;
	
	return s;
}

