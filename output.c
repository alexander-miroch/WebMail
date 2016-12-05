#include <stdlib.h>
//#include <stdio.h>
#include "fcgi_stdio.h"

#include "wmail.h"
#include "buffers.h"

struct buffers **output;
extern struct error_struct error;

void init_buffers(int n) {
	int i;
	struct buffers *pb,*cb;

	output = (struct buffers **) malloc(sizeof(struct buffers *) * n);
	if (!output) {
		SETERR(E_NOMEM,NULL);
		return;
	}

	*output = pb = 0;
	for (i = 0; i < n; ++i) {
		cb = init_buffer();
		if (!cb) exit(0);
		if (pb) { 
			pb->next = cb;
			pb = cb;
		} else {
			*output = pb = cb;
		}
	}
}

struct buffers *get_buffer(void) {
	struct buffers *tb;

	for (tb = *output; tb; tb = tb->next) {
		if (tb->status == BUF_FREE) {
			tb->status = BUF_DIRTY;
			tb->free = 0;
			return tb;
		}
	}
	return (struct buffers *) 0;
}

void free_buffer(struct buffers *b) {
	b->status = BUF_FREE;
	if (b->free) free(b->data);
}

void flush_buffers(void) {
	struct buffers *tb;

	for (tb = *output; tb; tb = tb->next) {
		if (tb->status == BUF_DIRTY) {
			tb->status = BUF_FREE;
			printf("%s",tb->data);
		}
	}

	for (tb = *output; tb; tb = tb->next) {
		if (tb->free) {
			tb->free = 0;
			free(tb->data);
		}
	}
}

struct buffers *init_buffer(void) {
	struct buffers *buf;

	buf = (struct buffers *) malloc(sizeof(struct buffers));
	if (!buf) {
		SETERR(E_NOMEM,NULL);
		return (struct buffers *)0;
	}

	buf->free = 0;
	buf->data = (char *) 0;
	buf->status = BUF_FREE;
	buf->next = 0;

	return buf;
}
