#include "fcgi_stdio.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "wmail.h"
#include "template.h"
#include "buffers.h"
#include "driver.h"
#include "utils.h"

extern char root[ROOT_SIZE];
extern struct error_struct error;
//extern driver_t *drv;
char *hash_table[HASH_TABLE_SIZE];
int elen,blen;

int open_template(char *file) {
	char path[PATH_LEN];
	int fd;

	sprintf(path,"%s/%s/%s.tmpl",ROOT,TEMPLATE_DIR,file);
	fd = open(path,O_RDONLY);
	if (fd < 0) {
		SETERR(E_FILE,path);
		return -1;
	}
	
	return fd;
}

void do_template(char *templ) {
	int fd;

	fd = open_template(templ);
	process_tmpl(fd);
	close(fd);
}

void process_tmpl(int fd) {
	char *buf_data, c0,c1;
	ssize_t n,nw;
	int state,len;
	

	len = sizeof(char) * (DEFAULT_BUFFER_SIZE + MAX_SUB_LEN + 6);
	buf_data = (char *) malloc(len);
	if (!buf_data) {
		SETERR(E_NOMEM,NULL);
		return;
	}

	state = S_OK;
	while ((n = read(fd,buf_data,DEFAULT_BUFFER_SIZE))) {
		if (n < 0) {
			SETERR(E_FILE,NULL);
			return;
		}
		//if (n < DEFAULT_BUFFER_SIZE) {
			buf_data[n] = 0;
		//}

		state = check_state(buf_data);
		if (state != S_OK) {
			if (state == S_LARROW2) {
				to_read(fd,buf_data + n);
			} else {
				nw = read(fd, buf_data + n, 2);
				if (nw < 0) {
					SETERR(E_FILE,NULL);
					return;
				}
				c0 = *(buf_data + n);
				c1 = *(buf_data + n + 1);
			
				switch (state) {
					case S_DOG1:
						if (c0 != '<' || c1 != '<') break;
						to_read(fd, buf_data + n + 2);
						break;
					case S_LARROW1:
						if (c0 != '<') break;
						to_read(fd, buf_data + n + 2);
						break;
					case S_RARROW1:
						if (c0 == '>' && c1 == '@') break;
						to_read(fd, buf_data + n + 2);
					case S_RARROW2:
						if (c0 == '@') break;
						to_read(fd, buf_data + n + 2);
						break;
				}
			}
		}
	
		parse_buf(buf_data);

		if (n == DEFAULT_BUFFER_SIZE) {
			buf_data = (char *) malloc(len);
			if (!buf_data) {
				SETERR(E_NOMEM,NULL);
				return;
			}
		}
	}
}

void to_read(int fd, char *bp) {
	ssize_t nw;
	int n=0,i=0;
	int state = S_LARROW2;
	char c;

	while ((nw = read(fd, bp + n, 1))) {
		c = *(bp + n);
		switch (c) {
			case '>':
				if (state == S_RARROW1)
					state = S_RARROW1;
				else state = S_RARROW2;
				break;
			case '@':
				if (state == S_RARROW2) return;
				break;
			default:
				state = S_LARROW2;
				break;
		}		
					
		if (i > MAX_SUB_LEN) {
			SETERR(E_TFORMAT,NULL);
			return;
		}			
		n++;
		i++;
 	} 		
}

int check_state(char *buf) {
	char c0,c1,c2;
	char *p;
	int state;

	state = S_OK;
	c0 = buf[DEFAULT_BUFFER_SIZE-1];
	c1 = buf[DEFAULT_BUFFER_SIZE-2];
	c2 = buf[DEFAULT_BUFFER_SIZE-3];

	p = strrchr(buf,'@');
	if (p) {
		if (*++p == '<' && *++p == '<') {
			state = S_LARROW2;
		}
	}
	
	switch (c0) {
		case '@':
			if (state != S_LARROW2) {
				state = (c1 == '>' && c2 == '>') ? S_OK : S_DOG1;
			}
			break;
		case '>':
			if (state == S_LARROW2)
				state = (c1 == '>') ? S_RARROW2 : S_RARROW1;
			else state = S_OK;
			break;
		case '<':
			if (c1 == '<') {
				state =  (c2 == '@') ? S_LARROW2 : S_OK;
			} else if (c1 == '@') {
				state = S_LARROW1;
			} else state = S_OK;
			break;
		default:
			state = (state == S_LARROW2) ? S_LARROW2 : S_OK;
			break;
	}

	return state;
}
	

int parse_buf(char *buf) {
	char *b,*e,*tp,*sp,*sv;
	struct buffers *bf,*b1,*b2;

//	printf("b=%s",buf);

	bf = get_buffer();
	if (!bf) {
		SETERR(E_TOOLONG,NULL);
		return -1;
	}
	
	tp = bf->data = buf;
	bf->free = 1;
	while (( b = strstr(tp,"@<<"))) {
		e = strstr(b,">>@");
		if (!e) {
			SETERR(E_TFORMAT,NULL);
			return -1;
		}
		tp = e;
		tp += 3;
		sp = b + 3;
		sp[e-sp] = 0;
		sv = get_val(sp);
		if (!sv) {
			sp[e-sp] = '>';
			continue;
		}

		b1 = get_buffer();
		b2 = get_buffer();
		if (!b1 || !b2) {
			SETERR(E_TOOLONG,NULL);
			return -1;
		}

	//	printf("b1=%s\n",sv);
	//	printf("b2=%s [%d]\n",tp,((tp - buf) < DEFAULT_BUFFER_SIZE));
		*b = 0;
		b1->data = sv;
		if ((tp - buf) < DEFAULT_BUFFER_SIZE) b2->data = tp;
		else free_buffer(b2);
	//	printf("---B---\n");
	//	printf("b1=%s (%p) [%p]\n",b1->data,b1,b1->data);
	//	printf("b2=%s (%p) [%p]\n",b2->data,b2,b2->data);
	//	printf("---E---\n");

	//	printf("\n1->%s\n",sv);
	//	printf("2->%s\n",tp);
	}

	return 0;
}

void init_hash(void) {
	register int i;
	
	for (i = 0; i < HASH_TABLE_SIZE; ++i) 
		hash_table[i] = 0;
}

char *get_val(char *key) {
	unsigned int hv;

	hv = do_hash(key,strlen(key));
	return hash_table[hv];
}

/* TODO: collision check */
void add2hash(char *key,char *val) {
	unsigned int hv;

	hv = do_hash(key,strlen(key));
	if (hash_table[hv]) {
		SETERR(E_DUP,key);
		return;
	} else {
		hash_table[hv] = strdup(val);
		if (!hash_table[hv]) {
			SETERR(E_NOMEM,NULL);
			return;
		}
	}
	return;
}

void clean_hash(void) {
	register int i;

	for (i = 0; i < HASH_TABLE_SIZE; ++i) {
		if (hash_table[i]) {
			 free(hash_table[i]);
			 hash_table[i] = 0;
		}
	}
}

/* Only <256 table entries */
unsigned int do_hash(char *key, unsigned int len) {
	unsigned int hash,i;

	for (hash=len,i=0; i<len; ++i) {
		hash += key[i];
	}
	return (hash % HASH_TABLE_SIZE);
}
