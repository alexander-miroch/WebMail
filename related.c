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

extern session_t current;

mime_t *mime_by_ctid(mime_t *mime,unsigned char *ctid) {
	int i;
	mime_t *m;

	if (mime->ctid &&
		!strcmp(mime->ctid,(char *)ctid)) return mime;

	for (i=0; i < mime->total; ++i) {
		m = mime_by_ctid(mime->childs[i],ctid);
		if (m) return m;
        }
	return NULL;
}

int inline inchr(unsigned char c) {
	if (c == 0x20 || c == '\n' || c == '"' || c == '\'' || c ==')' || c == '(' ) {
		return 1;
	}
	return 0;
}

#define FLUSH	*sp = NULL; \
		state = 0; 
unsigned char inline *can_cid(unsigned char *buf, unsigned char **sp) {
	static int state = 0;
	static unsigned char *sbuf;
	unsigned char c;

	c = *buf;
	switch (state) {
		case 0: 
			if (c == 'c') {
				++state;
				*sp = buf;
				return 0;
			} 
			break;
		case 1: if (c == 'i') { ++state; } else { FLUSH } break ;
		case 2: if (c == 'd') { ++state; } else { FLUSH } break ;
		case 3: if (c == ':') {	++state; } else { FLUSH } break;
		case 4:
			sbuf = buf;
			if (sbuf && *sbuf) {
				if (inchr(*sbuf)) {
					FLUSH
				} else ++state;
			} else { FLUSH }
			break;
		case 5:
		//	printf("%c ",*buf);
			if (inchr(*buf)) {
				state = 0;
				return sbuf;
			}
			break;
		default:
			break;	
			
	}
	return 0;
}

unsigned char *do_related_stuff(unsigned char *buf,message_t *msg) {
	mime_t *m;
	unsigned char *ptr, *fp;
	unsigned char *sp = NULL,*cid;
	int len,slen;
	unsigned char *tbuf = NULL, *rbuf = NULL;
	unsigned long off = 0;
	unsigned char sc;
	char mpath[PATH_LEN];	
	unsigned char ubuf[FOLDER_LEN];
	char *file;
	
	fp = ptr = buf;
	len = msg->bodylen;
	while (*ptr) {

		cid = can_cid(ptr,&sp);
		if (cid) {
			sc = *ptr;
			*ptr = 0;
			m = mime_by_ctid(msg->p0,cid);
			if (m) {
				if (!tbuf) {
					rbuf = tbuf = (unsigned char *) malloc (sizeof(char) * (len + 40000));
					if (!tbuf) {
						SETERR(E_NOMEM,NULL);
						return NULL;
					}
				}
				slen = cid - fp - 4;  // strlen("cid:")
				memcpy(tbuf + off, fp, slen);
				off += slen;
		
				file = seen(msg->file);
				url_encode((unsigned char *)current.folder,ubuf);
				snprintf(mpath,PATH_LEN,SELF"/?folder=%s&msg=%s&rg=%ld-%ld",
					ubuf,
					//current.folder,
					file ? file : msg->file,
					m->start,m->end
					);
				slen = strlen(mpath);
				memcpy(tbuf + off, mpath, slen);
			
				off += slen;
			
				*ptr = sc;
				fp = ptr;
				
			} else *ptr = sc;
		}
		++ptr;
	}

/*	print_hello();
	if (rbuf) printf("RELARED %s\n",rbuf);
	else printf("RELARED xx\n");
	printf("***");
*/
	if (rbuf) {
		slen = len - (fp - buf);
		memcpy(tbuf + off, fp, slen);	
		tbuf[off + slen] = 0;
		free(buf);
		return rbuf;	
	} else return buf;
}
