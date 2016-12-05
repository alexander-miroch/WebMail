#define DEFAULT_BUFFERS		512
#define DEFAULT_BUFFER_SIZE	1024

#define BUF_FREE		0x1
#define BUF_DIRTY		0x2

struct buffers {
	char *data;
	int status,free;
	struct buffers *next;
};


void init_buffers(int);
struct buffers *init_buffer(void);
void flush_buffers(void);
struct buffers *get_buffer(void);
void free_buffer(struct buffers *);
