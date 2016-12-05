#include "fcgi_stdio.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "wmail.h"
#include "session.h"
#include "template.h"
#include "buffers.h"
#include "driver.h"
#include "params.h"
#include "message.h"
#include "utils.h"
#include "env.h"
#include "compose.h"
#include "abook.h"
#include "filter.h"

extern session_t current;
extern driver_t *drv;
extern env_t env;

extern int pagedetected;

char *pages[] = {
	"404",
	"login",
	"folder",
	"mbox",
	"message",
	"attach",
	"compose",
	"cfolder",
	"compose",
	"upload",
	"prefs",
	"abook",
	"abook_add",
	"abook_view",
	"abook_get",
	"source",
	"print",
	"filter",
	"filter_add",
	"filter_edit",
	"done",
	NULL,
};

char * get_page(int page) {
	return pages[page];
}

unsigned long get_folders(void) {
	char *left,*clist;
	unsigned long nm;
	collect_t *cl;
       
	cl = drv->collect_folders(list_wr);
	if (!cl) return -1;

	nm = drv->check_4new(cl);

	left = out_wr(cl,accum_left);
	if (!left) {
		free_cl(cl);
		return -1;
	}
	clist = out_wr(cl,accum_copy);
	if (!clist) {
		free(left);
		free_cl(cl);
		return -1;
	}

	add2hash("folders",left);
	if (current.prefs.otd && current.page == PAGE_MSG) ZHASH("copyfolder");
	else add2hash("copyfolder",clist);
	free(left);
	free(clist);
	free_cl(cl);

	return nm;
}


int detect_page(int page) {
	char *s_ptr = NULL,*tp;
	int nm,tl,nf = 0;
	char zbuf[PATH_LEN+256];
	unsigned char ubuf[FOLDER_LEN*3];
	unsigned long usage;

	if (page == PAGE_ABOOK_GET) {
		do_abook_get();
		return PAGE_ABOOK_GET;
	}


	if (current.folder)
	url_encode((unsigned char *)current.folder,ubuf);
	else ubuf[0]='.', ubuf[1]=0;

	tp = do_utf7(current.folder);
	add2hash("folder",alias(tp));
	free(tp);

	pagedetected = 0;
	sprintf(zbuf,SELF"?folder=%s&cf=3",ubuf);
	add2hash("mbox_write",zbuf);
	
	add2hash("mbox_prefs",SELF"?cf=6");
	add2hash("mbox_exit",SELF"?cf=7");
	add2hash("mbox_abook",SELF"?cf=8");
	add2hash("mbox_filters",SELF"?cf=15");
	if (current.msg) {
		s_ptr = seen(current.msg);
		if (!s_ptr) s_ptr = current.msg;
		else nf = 1;
	}
	
	if (page == PAGE_MSG) {
		sprintf(zbuf,"%s&msg=%s",zbuf,s_ptr);
		add2hash("mbox_reply",zbuf);
		sprintf(zbuf,"%s&msg=%s&ra=1",zbuf,s_ptr);
		add2hash("mbox_all",zbuf);
		sprintf(zbuf,SELF"?cf=4&folder=%s&msg=%s",ubuf,s_ptr ? s_ptr : current.msg);
		add2hash("mbox_resend",zbuf);
		if (s_ptr) {
			sprintf(zbuf,SELF"?cf=12&folder=%s&msg=%s",ubuf,s_ptr);
			add2hash("mbox_print",zbuf);
			sprintf(zbuf,SELF"?cf=14&folder=%s&msg=%s",ubuf,s_ptr);
			add2hash("mbox_printer",zbuf);
		} else  {
			add2hash("mbox_print","#");
			add2hash("mbox_printer","#");
		}
		if (!strcmp(current.folder,"Draft")) {
			sprintf(zbuf,"&nbsp;<img src=\""IMGDIR"/inbox.png\">&nbsp;\
			<a href=\"#\" onclick='window.open(\""SELF"?cf=13&folder=%s&msg=%s\")'>\
			Продолжить</a>",ubuf,s_ptr);
			add2hash("continue",zbuf);
		} else ZHASH("continue");
	}
	
        add2hash("mbox_user",current.email);
        add2hash("mbox_delete","#");
	add2hash("mbox_check",SELF);

	usage = drv->get_usage();
	if (usage) {
		sprintf(zbuf,"%lu",usage);
		add2hash("mbox_usage",zbuf);
	} else add2hash("mbox_usage","many");

	if (page == PAGE_DONE) {
		if (current.todo == DO_SEND)
			add2hash("user_text","Сообщение отправлено");
		else if (current.todo == DO_DRAFT)
			add2hash("user_text","Сообщение сохранено в Черновике");
		else ZHASH("user_text");
		return PAGE_DONE;
	}
	if (page == PAGE_SOURCE) {
		get_source(s_ptr);
		if (nf) free(s_ptr);
		return PAGE_SOURCE;
	}

	if (nf) free(s_ptr);
	if (page == PAGE_UPLOAD_RESULT) {
		char *ptr;

		if (current.cdata.todel) rm_file(current.cdata.todel);
	
		if ((ptr = filelisting(0)))
			add2hash("res",ptr);
		else add2hash("res","");
		return PAGE_UPLOAD_RESULT;
	}

	if (page == PAGE_COMPOSE || page == PAGE_RESEND) {
		char *ptr;
		message_t *msg;
		
		flush_attach_dir();
		sprintf(zbuf,SELF"?folder=%s",ubuf);
		if (current.cdata.todel) rm_file(current.cdata.todel);

		add2hash("action",zbuf);

		sprintf(zbuf,SELF"?cf=11");
		add2hash("adr_book",zbuf);	
		if (current.msg) {
			msg = get_msg(current.msg);
			if (!msg) return PAGE_COMPOSE;

			fill_cc(msg);
			if (!current.cdata.isdraft) {
				ptr = compose_rebody(msg);
			} else {
				ptr = compose_draft_body(msg);
				move_attaches(msg);
			}
			add2hash("r_msg_body",ptr);
			free(ptr);

			if (page == PAGE_COMPOSE) {
				if (!current.cdata.isdraft) {
					if (msg->email_rpto) add2hash("r_to",msg->email_rpto);
					else if (msg->replyto) add2hash("r_to",msg->replyto);
					else if (msg->email_from) add2hash("r_to",msg->email_from);
					else if (msg->from) add2hash("r_to",msg->from);
					else add2hash("r_to",NBSP);
					add2hash("r_mesg",current.msg);
					ptr = compose_resubj(msg->subject);
				} else {
					if (msg->to) add2hash("r_to",msg->to);
					else ZHASH("r_to");
					ZHASH("r_mesg");
					ptr = strdup(msg->subject);
				}
				ZHASH("f_mesg");
			} else {
				ZHASH("r_to");
				ptr = compose_fwdsubj(msg->subject);		
				move_attaches(msg);
				add2hash("f_mesg",current.msg);
				ZHASH("r_mesg");
			}
			//ptr = (char *) escape((unsigned char *)ptr);
			add2hash("r_subj",ptr);
			free(ptr);
			free_msg(msg);
		} else {
			ZHASH("r_subj"), ZHASH("r_mesg"), ZHASH("f_mesg");
			if (current.adata.msg) add2hash("r_to",current.adata.msg);
			else ZHASH("r_to");
			if (current.prefs.sign) {
				set_sign(zbuf,strlen((char *)current.prefs.sign));
				add2hash("r_msg_body",zbuf);
			} else ZHASH("r_msg_body");
			ZHASH("r_cc");
		}
		if ((ptr = filelisting(0))) add2hash("uploaded",ptr);
		else ZHASH("uploaded");

		return PAGE_COMPOSE;
	}


	sprintf(zbuf,SELF"?folder=%s&cf=1",ubuf);
	add2hash("folder_create",zbuf);
	tl = strlen(zbuf) - 1;
	zbuf[tl]='2';
	add2hash("folder_drop",zbuf);
	zbuf[tl]='5';
	add2hash("folder_empty",zbuf);

	if (page == PAGE_CFOLDER) {
		sprintf(zbuf,SELF"?folder=%s&sby=%c&p=%d",
                        ubuf,current.prefs.sby,current.sf);
		add2hash("action",zbuf);

		if (get_folders() < 0) return PAGE_LOGIN;
		return PAGE_CFOLDER;
	}

	if (page == PAGE_ATTACH) {
		if (current.data.s && current.data.e) {
	
			get_attach(current.msg,NULL,current.data.s,current.data.e);
			return PAGE_ATTACH;
		} else return PAGE_LOGIN;
	}

	if (page == PAGE_PRINT) {
		if (current.msg) do_page_msg(current.msg);
		return PAGE_PRINT;
	}

	if (page == PAGE_MSG) {
		sprintf(zbuf,SELF"?folder=%s&sby=%c&p=%d",
                        ubuf,current.prefs.sby,current.sf);
		add2hash("action",zbuf);
		if (current.msg) do_page_msg(current.msg);
		if (get_folders() < 0) return PAGE_LOGIN;
		if (!current.prefs.otd)
			do_page_folder(current.folder);
		else {
			add2hash("com1","<!--");
			add2hash("com2","-->");
		}
		return PAGE_MSG;
	}

	if (page == PAGE_FOLDER) {
		sprintf(zbuf,SELF"?folder=%s&sby=%c&p=%d",
                        ubuf,current.prefs.sby,current.sf);
		add2hash("action",zbuf);
		if (get_folders() < 0) return PAGE_LOGIN;
		do_page_folder(current.folder);
		return PAGE_FOLDER;
	}
	if (page == PAGE_PREFS) {
		do_page_prefs();
		if (get_folders() < 0) return PAGE_LOGIN;
		return PAGE_PREFS;
	}

	if (page == PAGE_ABOOK) {
		do_abook();
		if (get_folders() < 0) return PAGE_LOGIN;
		FREE_IT(AB);
		return PAGE_ABOOK;
	}

	if (page == PAGE_ABOOK_ADD) {
		do_abook_add();
		if (get_folders() < 0) return PAGE_LOGIN;
		FREE_IT(AB);
		return PAGE_ABOOK_ADD;
	}

	if (page == PAGE_ABOOK_VIEW) {
		do_abook_view();
		if (get_folders() < 0) return PAGE_LOGIN;
		FREE_IT(AB);
		return PAGE_ABOOK_VIEW;
	}

	if (page == PAGE_FILTER) {
		do_filter_add();
		do_filter_view();
		free_fidata(&FT);
		if (get_folders() < 0) return PAGE_LOGIN;
		return PAGE_FILTER;
	}
	
	if (page == PAGE_FILTER_ADD) {
		do_filter_show_form();
		free_fidata(&FT);
		if (get_folders() < 0) return PAGE_LOGIN;
		return PAGE_FILTER_ADD;
	}

	if (page == PAGE_FILTER_EDIT) {
		if (get_folders() < 0) return PAGE_LOGIN;
		return PAGE_FILTER_EDIT;
	}



	if ((nm = get_folders()) < 0) return PAGE_LOGIN;
	if (nm)	{
		sprintf(zbuf,"Новых сообщений: %d",nm);
		add2hash("new_messages",zbuf);
	} else add2hash("new_messages","У Вас нет новых сообщений");
	
	return PAGE_MBOX;
}

void do_page_msg(char *msg) {
	char *header;
	message_t *message;

	message = get_msg(msg);
	if (!message) {
		add2hash("pre_mess","<p>&nbsp;Cообщение составлено с ошибками и не может быть отображено</p>");
		add2hash("message","<br />");
		return;
	}
	header = get_header_bar(message);
	if (header) add2hash("pre_mess",header);
	else ZHASH("pre_mess");
	if (message->body) add2hash("message", (char *) message->body);
	else ZHASH("message");
	free_msg(message);
	free(header);
}

static void p_col(char *bp, unsigned char *ubuf, char sby, char sbyo, int i) {
	sprintf(bp,"<a href=\""SELF"?folder=%s&sby=%c&sbyo=%c&p=%d\">%d</a>%s",
	ubuf,sby,sbyo,i,(i+1),WRAP(i));
}

void do_page_folder(char *folder) {
	char buf[FOLDER_LEN+256];
	collect_t *cl;
	char *list,*pages;
	unsigned char ubuf[FOLDER_LEN*3];

	cl = drv->collect_messages(folder,list_wr);
	if (!cl) return;

	url_encode((unsigned char *)folder,ubuf);
	if (current.ff != FF_ISSENT)
		sprintf(buf,"<a href="SELF"?folder=%s&sby=%c&sbyo=%c>%s</a>",ubuf,SBY_FROM,SBYO_G,"От");
	else
		sprintf(buf,"<a href="SELF"?folder=%s&sby=%c&sbyo=%c>%s</a>",ubuf,SBY_TO,SBYO_G,"Кому");
	add2hash("url0",buf);
	sprintf(buf,"<a href="SELF"?folder=%s&sby=%c&sbyo=%c>Тема</a>",ubuf,SBY_SUBJ, SBYO_G);
	add2hash("url1",buf);
	sprintf(buf,"<a href="SELF"?folder=%s&sby=%c&sbyo=%c>Дата</a>",ubuf,SBY_DATE, SBYO_G);
	add2hash("url2",buf);
	sprintf(buf,"<a href="SELF"?folder=%s&sby=%c&sbyo=%c>Вес</a>",ubuf,SBY_SIZE, SBYO_G);
	add2hash("url3",buf);

	list = out_ms(cl);
	if (!list) {
		free_cl(cl);
		return;
	}

	ZHASH("com1");
	ZHASH("com2");

	add2hash("messages",list);
	free(list);
	free_cl(cl);

	if (current.prefs.pp) {
		pages = do_sum_pages(get_s(cl->len,current.prefs.pp),p_col);
		add2hash("sumpagess",pages);
		if (pages) free(pages);
	} else add2hash("sumpagess",NBSP);
}


char *do_sum_pages(int s,void(*pcol)(char *,unsigned char *,char,char,int)) {
	int i;
	char *bp,*sbp;
	unsigned char ubuf[FOLDER_LEN];
	int nbsp_len;

	bp = (char *) malloc(sizeof(char) * (s * 1024));
	if (!bp) {
		SETERR(E_NOMEM,NULL);
		return NULL;
	}

	if (!s || s == 1) {
		nbsp_len = strlen(NBSP);
		memcpy(bp,NBSP,nbsp_len);
		memcpy(bp+nbsp_len,"\0",1);
		return bp;
	}
	sbp = bp ;	

	for (i=0; i<s; ++i) {
		url_encode((unsigned char *)current.folder,ubuf);
		if (current.sf == i)
		sprintf(bp,"%d%s",(i+1),WRAP(i));
		else pcol(bp,ubuf,current.prefs.sby,current.prefs.sby_order,i);
		bp += strlen(bp);
	}
	
	return sbp;
}

