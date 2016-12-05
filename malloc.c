
//#define DEBUG



#ifdef DEBUG

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <malloc.h>
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <ucontext.h>
#include <dlfcn.h>
#include <execinfo.h>

#include "utils.h" 
    
     /* Prototypes for our hooks.  */
static void my_init_hook (void);
static void *my_malloc_hook (size_t, const void *);
static void my_free_hook (void*, const void *);

void *(*old_malloc_hook)(size_t, const void *);
void (*old_free_hook)(void *, const void *);     
FILE *fxxx = NULL;

     /* Override initializing hook from the C library. */
void (*__malloc_initialize_hook) (void) = my_init_hook;
     
static void my_init_hook (void) {

	if (!fxxx) fxxx = fopen("/tmp/zizi","w+");
	old_malloc_hook = __malloc_hook;
	old_free_hook = __free_hook;
	__malloc_hook = my_malloc_hook;
	__free_hook = my_free_hook;
}
     
static void * my_malloc_hook (size_t size, const void *caller) {
	void *result;
       /* Restore all old hooks */
	__malloc_hook = old_malloc_hook;
	__free_hook = old_free_hook;
       /* Call recursively */
	result = malloc (size);
       /* Save underlying hooks */
	old_malloc_hook = __malloc_hook;
	old_free_hook = __free_hook;
       /* printf might call malloc, so protect it too. */
	fprintf (fxxx,"malloc (%u) returns %p \n", (unsigned int) size, result);
	fflush(fxxx);
	/* Restore our own hooks */
	__malloc_hook = my_malloc_hook;
	__free_hook = my_free_hook;
	return result;
}
     
static void my_free_hook (void *ptr, const void *caller) {
       /* Restore all old hooks */
	__malloc_hook = old_malloc_hook;
	__free_hook = old_free_hook;
       /* Call recursively */
	free (ptr);
       /* Save underlying hooks */
	old_malloc_hook = __malloc_hook;
	old_free_hook = __free_hook;


	fprintf (fxxx,"freed pointer %p\n", ptr);
	fflush(fxxx);

	__malloc_hook = my_malloc_hook;
	__free_hook = my_free_hook;
}

void signal_segv(int s, siginfo_t *si, void *data) {
	ucontext_t *uc = (ucontext_t *) data;
	char out[200],*tv;
	int f = 0;
	Dl_info dlinfo;
	void **bp = 0;
	void *ip = 0;

	static const char *si_codes[3] = {"", "SEGV_MAPERR", "SEGV_ACCERR"};
	int i;

	tv = get_time(out,0);	
	
	fprintf(fxxx,"SEGFAULT NAG! %s\n",tv);
	fprintf(fxxx, "info.si_signo = %d\n", s);
	fprintf(fxxx, "info.si_errno = %d\n", si->si_errno);
	fprintf(fxxx, "info.si_code  = %d (%s)\n", si->si_code, si_codes[si->si_code]);
	fprintf(fxxx, "info.si_addr  = %p\n", si->si_addr);
    for(i = 0; i < NGREG; i++)
        fprintf(fxxx, "reg[%02d]       = 0x%016lx\n", i, uc->uc_mcontext.gregs[i]);

	ip = (void*)uc->uc_mcontext.gregs[REG_RIP];
	bp = (void**)uc->uc_mcontext.gregs[REG_RBP];

	while(bp && ip) {
		if(!dladdr(ip, &dlinfo)) {
			break;
		}

		const char *symname = dlinfo.dli_sname;
		fprintf(fxxx, "% 2d: %p <%s+%u> (%s) exact=%p\n",
		++f,
		ip,
		symname,
		(unsigned)(ip - dlinfo.dli_saddr),
		dlinfo.dli_fname,dlinfo.dli_saddr);

		if(dlinfo.dli_sname && !strcmp(dlinfo.dli_sname, "main")) break;

		ip = bp[1];
		bp = (void**)bp[0];
		//++bp;
	}
	fflush(fxxx);
	exit(1);

}

void __attribute__((constructor)) x_init(void) {
	struct sigaction action;
	if (!fxxx) fxxx = fopen("/tmp/zizi","w+");

	memset(&action, 0, sizeof(action));
	action.sa_sigaction = signal_segv;
	action.sa_flags = SA_SIGINFO;
	sigemptyset(&action.sa_mask);
	if(sigaction(SIGSEGV, &action, NULL) < 0) {
		fprintf(fxxx,"Error:signal\n");
		fflush(fxxx);
		return ;
	}
	if(sigaction(SIGILL, &action, NULL) < 0) {
		fprintf(fxxx,"Error:signal\n");
		fflush(fxxx);
		return ;
	}
	if(sigaction(SIGABRT, &action, NULL) < 0) {
		fprintf(fxxx,"Error:signal\n");
		fflush(fxxx);
		return ;
	}

	if(sigaction(SIGBUS, &action, NULL) < 0) {
		fprintf(fxxx,"Error:signal\n");
		fflush(fxxx);
		return ;
	}


	fprintf(fxxx,"Handler started\n");
	fflush(fxxx);

}




#endif
