#include "fcgi_stdio.h"
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#include "wmail.h"
#include "driver.h"
#include "session.h"
#include "message.h"
#include "utils.h"
#include "filter.h"

/*
	Maildir  driver
*/

#define POPPASSWD       "/var/users/poppasswd"
#define POPUSER_UID	110
#define POPUSER_GID	31

extern session_t current;
extern struct error_struct error;
extern char hostname[HOSTNAME_LEN];

enum a_f {
	F_INC,F_SENT,F_REMOVED
};

alis_t all_aliases[] = {
	{ ".","Входящие", 1 },
	{ "sent-mail","Отправленные", 9 },
	{ "Trash","Удаленные", 5 },
	{ "Spam", "Сомнительные", 4 },
	{ "Draft", "Черновики", 5 },
	{ NULL,NULL, 0 }
};

int fs_init(driver_t *drv) {

	drv->aliases = all_aliases;
	return 0;
}


int fs_auth(char *login,char *password) {
	FILE *fp;
	char *flogin,*fpassword;
	char buf[MSG_LINE_SIZE];
	short int found = 0;

	fp = fopen(POPPASSWD,"r");
	if (!fp) {
		SETERR(E_FILE,POPPASSWD);
		return 0;
	}

	while (fgets(buf,MSG_LINE_SIZE,fp) != NULL) {
		if ((flogin = strtok(buf, ":")) == NULL) {
			SETERR(E_PARSE,POPPASSWD);
			return 0;
		}
		if (!strcmp(login,flogin)) {
			if ((fpassword = strtok(NULL, ":")) == NULL) {
				SETERR(E_PARSE,POPPASSWD);
				return 0;
			}
			found = 1;
			break;
		}
	}
	fclose(fp);

	if (found) {
		if (!strcmp(password,fpassword)) {
			return 1;
		} 
	}
	return 0;
}

int fs_delete_folder(char *folder) {
	return 1;
}

int fs_fetch_message(char *name) {
	return 1;
}

int fs_move_message(char *from,char *to) {
	return 1;
}

int fs_delete_message(char *name) {
	return 1;
}

static int check_dir(char *path,int doren,int *nm) {
	int j = 0, q = 0;
	DIR *dir;
	struct dirent *de;
	int len = 0;
	char npath[PATH_LEN];
	
	dir = opendir(path);
	if (!dir) {
		SETWARN(E_OPENDIR,path);
		return 0;
	}
	while ((de = readdir(dir)) != NULL) {
		if (de->d_name[0] == '.') continue;
		if (!len) len = strlen(path);
		
		path[len-4] = 0;

		++q;
		if (doren) {
			sprintf(npath,"%s/cur/%s:2,",path,&de->d_name[0]);
			sprintf(path,"%s/new/%s",path,&de->d_name[0]);
			if (rename(path,npath) < 0) {
				SETWARN(E_RENAME,NULL);
			}
			do_filters(npath,NULL,0);
			++j;
		} else {
			char *vptr;
			int f;

			vptr = strrchr(de->d_name,':');
			if (!vptr) {
				SETWARN(E_MSGPARSE,de->d_name);
				continue;
			}

			f = 0;
			while (*vptr) {
				if (*vptr == 'S') {
					f = 1;
					break;
				}
				++vptr;
			}
			if (!f) ++j;
		}
	}
	closedir(dir);
	if (nm) *nm = q;
	return j;
	
}

char *fs_fgets(char *s,int size, void *stream) {
	return fgets(s,size,(FILE *) stream);
}

void fs_finish(fdata_t *f) {
	fclose((FILE *) f->stream);
	free(f);
}

fdata_t *fs_get_msgstream(char *path,char *msg) {
        char buf[PATH_LEN];
	fdata_t *fdata;
        FILE *f;
	struct stat st;

	if (!path) {
	        if (*current.folder == '.')
        	        sprintf(buf,"%s/cur/%s",current.path,msg);
	        else sprintf(buf,"%s/.%s/cur/%s",current.path,current.folder,msg);
	} else sprintf(buf,"%s",path);

	if (stat(buf,&st) < 0) {
		if (errno == ENOENT)
			strncat(buf,"S",1);
		else {
	                SETWARN(E_FILE,buf);
        	        return NULL;
		}
	}

	fdata = alloc_fdata();
        f = fopen(buf,"r");
        if (!f) {
	       	SETWARN(E_FILE,buf);
		return NULL;
	}

	fdata->stream = f;
	fdata->size = st.st_size;

        return fdata;
}


void fs_apply_filters(char *folder,char *msg,char **fdarr,int len) {
	char mpath[PATH_LEN];

	if (*folder == '.')
		sprintf(mpath,"%s/cur/%s",current.path,msg);
	else
		sprintf(mpath,"%s/.%s/cur/%s",current.path,folder,msg);

	do_filters(mpath,fdarr,len);
}



int fs_check_4new(collect_t *cl) {
	register int i;
	int len;
	char *folder;
	char mpath[PATH_LEN];
	int count = 0;

	len = cl->len;
	for (i = 0; i < len; ++i) {
		folder = cl->base[i].name;
		if (*folder == '.') sprintf(mpath,"%s/new",current.path);
		else sprintf(mpath,"%s/.%s/new",current.path,folder);
		count += check_dir(mpath,1,0);
	}
	

	return count;
}

collect_t *fs_collect_messages(char *folder, void (*collect)(collect_t *, char *,int,int)) {
	char mpath[PATH_LEN];
	DIR *dir;
	struct dirent *de;
	collect_t *cl;
	int i = 0;

	if (*folder == '.')
		sprintf(mpath,"%s/cur",current.path);
	else sprintf(mpath,"%s/.%s/cur",current.path,folder);

	dir = opendir(mpath);	
	if (!dir) {
		SETERR(E_OPENDIR,mpath);
		return NULL;
	}

	cl = init_cl(MAX_MESSAGES * MESSAGE_LEN); //1M
	if (!cl) {
		closedir(dir);
		return NULL;
	}
	
	while ((de = readdir(dir)) != NULL) {
		if (de->d_name[0] == '.') continue;

		if (i > MAX_MESSAGES) {
			SETWARN(E_MAXMESSAGES,NULL);
			break;
		}
		(*collect)(cl,&de->d_name[0],0,0);
		++i;
	}
	
	closedir(dir);
	return cl;
}

collect_t *fs_collect_folders(void (*collect)(collect_t *,char *,int,int)) {
	DIR *dir;
	struct dirent *de;
	int i = 0;
	collect_t *cl;
	char *dv;
	int len,nm,tm;

	len = strlen(current.path);
	dv = (char *) malloc(sizeof(char) * (len + FOLDER_LEN));
	if (!dv) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}

	memcpy(dv,current.path,len);
	memcpy(dv+len,"\0",1);

	dir = opendir(current.path);
	if (!dir) {
		SETERR(E_OPENDIR,current.path);
		free(dv);
		return NULL;
	}
	
	cl = init_cl(MAX_FOLDERS * (FOLDER_LEN + 1024));
	if (!cl) {
		free(dv);
		closedir(dir);
		return NULL;
	}

	while ((de = readdir(dir)) != NULL) {
		if (!(de->d_name[0] == '.' && de->d_name[1] && de->d_name[1] != '.')) continue;
		if (i > MAX_FOLDERS) {
			SETERR(E_MAXFOLDERS,current.path);
			closedir(dir);
			free(dv);
			return NULL;
		}

		sprintf(dv+len,"/%s/cur",de->d_name);
		nm = check_dir(dv,0,&tm);
		(*collect)(cl,&de->d_name[1],nm,tm);
		
		++i;
	}

	memcpy(dv+len,"/cur",4);
	memcpy(dv+len+4,"\0",1);
	nm = check_dir(dv,0,&tm);
	(*collect)(cl,".",nm,tm);

	free(dv);
	closedir(dir);
	return cl ;
}

void fs_set_flags(char *path, char *msgfile,int flags, char **retmsg) {
	char mpath[PATH_LEN],npath[PATH_LEN];
	char *folder,*ptr;
	char cflags[MAX_FLAGS];
	int i=0;
	char sv;


	if (!path) {
		folder = current.folder;
		if (*folder == '.') sprintf(mpath,"%s/cur/%s",current.path,msgfile);
		else sprintf(mpath,"%s/.%s/cur/%s",current.path,folder,msgfile);
	} else sprintf(mpath,"%s",path);

	ptr = strrchr(mpath,',');
	if (!ptr) {
		SETERR(E_MSGFILE,NULL);
		return;
	}
	++ptr;

	/* By alphabet order */
	if (flags & FL_DRAFT) cflags[i++] = 'D';
	if (flags & FL_FLAGGED) cflags[i++] = 'F';
	if (flags & FL_PASSED) cflags[i++] = 'P';
	if (flags & FL_REPLIED) cflags[i++] = 'R';
	if (flags & FL_SEEN) cflags[i++] = 'S';
	if (flags & FL_TRASHED) cflags[i++] = 'T';
	
	cflags[i] = 0;
	sv = *ptr;
	*ptr = 0;
	sprintf(npath,"%s%s",mpath,cflags);
	*ptr = sv;

	if (rename(mpath,npath) < 0) {
		SETWARN(E_RENAME,NULL);
	}
	if (retmsg) *retmsg = strdup(npath);
}

char * fs_build_path(char *email,char *folder,char *add) {
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


int fs_get_flags(char *msgfile) {
	char *ptr;
	int flags = 0;

	ptr = strrchr(msgfile,':');
	if (!ptr) {
		SETERR(E_MSGFILE,NULL);
		return -1;
	}

	++ptr;
	if (*ptr != '1' && *ptr != '2') {
		SETERR(E_MSGFILE,NULL);
		return -1;
	}
	if (*++ptr != ',') {
		SETERR(E_MSGFILE,NULL);
		return -1;
	}

	while (*++ptr) {
		switch (*ptr) {
			case 'S': flags |= FL_SEEN; break;
			case 'R': flags |= FL_REPLIED; break;
			case 'P': flags |= FL_PASSED; break;
			case 'T': flags |= FL_TRASHED; break;
			case 'D': flags |= FL_DRAFT; break;
			case 'F': flags |= FL_FLAGGED; break;
			default: break;
		}
	}
	
	return flags;
}


#define SETF(v,len,a,b,c) v[len-4]='/'; v[len-3]=a; v[len-2]=b; v[len-1]=c; \
					if (mkdir(mpath,0700) < 0) { SETERR(E_FILE,mpath); return 1; }\
					chown(mpath,POPUSER_UID,POPUSER_GID);
					
				
			  

int fs_create_folder(char *name) {
	char mpath[PATH_LEN];
	char *folder;
	int len;

	folder = current.folder;
	if (*folder == '.') sprintf(mpath,"%s/.%s/cur",current.path,name);
	else sprintf(mpath,"%s/.%s.%s/cur",current.path,folder,name);

	len = strlen(mpath);

	SETF(mpath,len,0,0,0);
	SETF(mpath,len,'c','u','r');
	SETF(mpath,len,'t','m','p');
	SETF(mpath,len,'n','e','w');

	return 0;
}

int fs_special_exists(char *name) {
	char mpath[PATH_LEN];
	int rv;
	struct stat st;

	sprintf(mpath,"%s/.%s",current.path,name);

	if ((rv = stat(mpath,&st)) < 0) {
		if (errno == ENOENT) return 0;
		SETERR(E_FILE,mpath);
		return -1;
	}

	if (S_ISDIR(st.st_mode)) return 1;

	SETERR(E_FILE,mpath);
	return -1;
}

static int delete_folder(char *folder) {
	DIR *dir;
	struct dirent *de;
	char npath[PATH_LEN];
	struct stat st;

	dir = opendir(folder);
	if (!dir) {
		SETERR(E_OPENDIR,folder);
		return 1;
	}

	while ((de = readdir(dir)) != NULL) {
		if ((de->d_name[0] == '.' && !de->d_name[1]) ||
			(de->d_name[0] == '.' && de->d_name[1] == '.')) continue;
		
		sprintf(npath,"%s/%s",folder,de->d_name);
		if (stat(npath,&st) < 0) {
			SETERR(E_FILE,npath);
			closedir(dir);
			return 1;
		}
				
		if (S_ISDIR(st.st_mode)) {
			delete_folder(npath);
			if (rmdir(npath) < 0) {
				SETERR(E_FILE,npath);
				closedir(dir);
				return 1;
			}
		} else {
			if (unlink(npath) < 0) {
				SETERR(E_FILE,npath);
				closedir(dir);
				return 1;
			}
		}
	}
	closedir(dir);
	return 0;
}

void fs_drop_folder(void) {
	char mpath[PATH_LEN];
	char *folder;
	DIR *dir;
	struct dirent *de;
	int len = 0;

	folder = current.folder;
	if (*folder == '.') return;
	
	len = strlen(folder);

	dir = opendir(current.path);
	if (!dir) {
		SETERR(E_OPENDIR,current.path);
		return ;
	}
	while ((de = readdir(dir)) != NULL) {
		if (de->d_name[0] != '.') continue;
		
		if (!strncmp(folder,&de->d_name[1],len)) {
			if (de->d_name[len+1] && de->d_name[len+1] != '.') continue;
			sprintf(mpath,"%s/%s",current.path,de->d_name);
			delete_folder(mpath);
			if (rmdir(mpath) < 0) {
				SETERR(E_FILE,mpath);
				closedir(dir);
				return;
			}
		}
	}
	closedir(dir);
}

void fs_empty_folder(void) {
	char mpath[PATH_LEN];
	char *folder;
	DIR *dir;
	struct dirent *de;
	int len = 0;

	folder = current.folder;
	if (*folder == '.') sprintf(mpath,"%s/cur/",current.path);
	else sprintf(mpath,"%s/.%s/cur/",current.path,folder);
	len = strlen(mpath);
	dir = opendir(mpath);
	if (!dir) {
		SETERR(E_OPENDIR,current.path);
		return ;
	}
	while ((de = readdir(dir)) != NULL) {
		if (de->d_name[0] == '.') continue;
		strncpy(mpath + len, de->d_name, de->d_reclen);
		if (unlink(mpath) < 0) {
			SETWARN(E_FILE,mpath);
			continue;
		}
	}
	closedir(dir);
}

int fs_copy_tofolder(char *path,char *msgfile,char *where, char **retpath) {
	char mpath[PATH_LEN],npath[PATH_LEN];
	char *folder;
	char *buf;
	int f1,f2,nbr,nbw;
	int block;

	if (!path) {
		folder = current.folder;
		if (*folder == '.') sprintf(mpath,"%s/cur/%s",current.path,msgfile);
		else sprintf(mpath,"%s/.%s/cur/%s",current.path,folder,msgfile);
	} else sprintf(mpath,"%s",path);
	
	if (*where == '.') sprintf(npath,"%s/cur/%s",current.path,msgfile);
	else sprintf(npath,"%s/.%s/cur/%s",current.path,where,msgfile);

	if (!strcmp(npath,mpath)) {
		if (retpath) *retpath = strdup(npath);
		return 1;
	}

	f1 = open(mpath,O_RDONLY);
	if (f1 < 0) {
		SETERR(E_FILE,mpath);
		return 1;
	}

	f2 = open(npath,O_RDWR|O_CREAT,0600);
	if (f2 < 0) {
		close(f1);
		SETERR(E_FILE,npath);
		return 1;
	}
	
	block = getpagesize();
	buf = (char *) malloc(sizeof(char) * block);
	if (!buf) {
		close(f1);
		close(f2);
		SETERR(E_NOMEM,NULL);
		return 1;
	}	

	while ((nbr = read(f1,buf,block))) {
		if (nbr < 0) {
			free(buf);
			close(f1);
			close(f2);
			SETERR(E_FILE,mpath);
			return 1;
		}
		nbw = write(f2,buf,nbr);
		if (nbw != nbr) {
			free(buf);
			close(f1);
			close(f2);
			SETERR(E_FILE,npath);
			return 1;
		}
	}

	chown(npath,POPUSER_UID,POPUSER_GID);
	if (retpath) *retpath = strdup(npath);
	free(buf);
	close(f1);
	close(f2);
	return 0;
}


int fs_msg_delete(char *path,char *msgfile) {
	char mpath[PATH_LEN];
	char *folder;

	if (!path) {
		folder = current.folder;
		if (*folder == '.') sprintf(mpath,"%s/cur/%s",current.path,msgfile);
		else sprintf(mpath,"%s/.%s/cur/%s",current.path,folder,msgfile);
	} else sprintf(mpath,"%s",path);

	return unlink(mpath);
}

long fs_getpos(void *s) {
	return ftell((FILE *)s);
}

int fs_setpos(void *s,long int pos) {
	return fseek((FILE *)s,pos,SEEK_SET);
}

void fs_save_sent(char *file,char *folder) {
	char mpath[PATH_LEN];
	time_t tv;

	time(&tv);
	sprintf(mpath,"%s/.%s/cur/%ld.%d.%s:2,",current.path,folder,tv,getpid(),hostname);
	if (rename(file,mpath) < 0) {
		SETWARN(E_RENAME,NULL);
		return;
	}
	return;
}

unsigned long fs_get_usage(void) {
	char mpath[PATH_LEN];
	char buf[MSG_LINE_SIZE];
	char *ptr;
	FILE *fd;
	int len;

	ptr = strrchr(current.path,'/');
	if (!ptr) return 0;
	*ptr = 0;
	sprintf(mpath,"%s/@mbox.usage",current.path);
	*ptr = '/';
	fd = fopen(mpath,"r");
	if (!fd) {
		return 0;
	}
	
	if (fgets(buf,MSG_LINE_SIZE,fd) == NULL) return 0;
	fclose(fd);
	
	len = strlen(buf);
	buf[len-1] = 0;

	return strtol(buf,NULL,10);
}

struct driver fs_drv = {
	.init = fs_init,
	.auth = fs_auth,
	.collect_folders = fs_collect_folders,
	.collect_messages = fs_collect_messages,
	.check_4new = fs_check_4new,
	.get_flags = fs_get_flags,
	.set_flags = fs_set_flags,
	.get_msgstream = fs_get_msgstream,
	.finish = fs_finish,
	.fgets = fs_fgets,
	.getpos = fs_getpos,
	.setpos = fs_setpos,
	.delete_msg = fs_msg_delete,
	.copy_tofolder = fs_copy_tofolder,
	.create_folder = fs_create_folder,
	.drop_folder = fs_drop_folder,
	.empty_folder = fs_empty_folder,
	.save_sent = fs_save_sent,
	.build_path = fs_build_path,
	.special_exists = fs_special_exists,
	.get_usage = fs_get_usage,
	.apply_filters = fs_apply_filters
};


