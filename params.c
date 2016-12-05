#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

#include "params.h"
#include "fcgi_stdio.h"
#include "session.h"
#include "utils.h"
#include "filter.h"
#include "wmail.h"

extern session_t current;
extern char *pages[];
int pagedetected = 0;
extern jmp_buf jenv;

void inline setpage(int page) {
	if (!pagedetected) 
		current.page = page;
}

void do_page(char *v) {
	if (!strncmp(v,pages[PAGE_LOGIN],strlen(pages[PAGE_LOGIN]))) {
		pagedetected = 0;
		setpage(PAGE_LOGIN);
	}
}

void do_email(char *v) {
	UDEC(v)
	current.email = strdup(v);
	DOCHECK(validate_email(current.email),jenv);
}

void do_edit_what(char *v) {
	UDEC(v)
	DOCHECK(validate_email(v),jenv);
	current.adata.edit_what = v;
}

void do_adel(char *v) {
	UDEC(v)
	DOCHECK(validate_email(v),jenv);
	current.adata.edit = v;
	setpage(PAGE_ABOOK_ADD);
	pagedetected = 1;
}

void do_start_from(char *v) {
	current.sf = strtol(v,(char **)NULL,10);
	DOCHECK(validate_sf(current.sf),jenv);
}

void do_sort_by(char *v) {
	DOCHECK(validate_sby(*v),jenv);
	current.prefs.sby = *v;
}

void do_msg(char *v) {
	DOCHECK(validate_msg(v),jenv);
	setpage(PAGE_MSG);
	current.msg = v;
}

void do_amsg(char *v) {
	UDEC(v)
	DOCHECK(validate_email(v),jenv);
	current.adata.msg = v;
}

void do_password(char *v) {
	int myl;

	UDEC(v)
	myl = strlen(v);
	DOCHECK(validate_password(v,myl),jenv);
	current.password = strdup(v);
}

void do_todo(char *v) {
	current.todo = atoi(v);
	DOCHECK(validate_todo(current.todo),jenv);
}

void do_folder(char *v) {
	UDEC(v)
	DOCHECK(validate_folder(v),jenv);
	if (current.page != PAGE_MSG) setpage(PAGE_FOLDER);
	current.folder = v;
	if (!strncmp(v,"sent-mail",9)) {
		current.ff = FF_ISSENT;
	}
}

void do_marr(char *v) {

	if (!current.data.marr) {
		current.data.marr = (char **) malloc(sizeof(char *) * MAX_MESSAGES);
		if (!current.data.marr) {
			SETERR(E_NOMEM,NULL);
			return;
		}
	}

	UDEC(v)

	DOCHECK(validate_msg(v),jenv);
	current.data.marr[current.data.marr_len++] = v;
}	

void do_abarr(char *v) {
	if (!current.adata.abarr) {
		current.adata.abarr = (char **) malloc(sizeof(char *) * MAX_RECORDS);
		if (!current.adata.abarr) {
			SETERR(E_NOMEM,NULL);
			return;
		}
	}

	UDEC(v)
	DOCHECK(validate_email(v),jenv);
	current.adata.abarr[current.adata.abarr_len++] = v;
}

void do_ra(char *v) {
	current.cdata.ra = 1;
}

void do_c_mark(char *v) {
	current.data.mark = atoi(v);
	DOCHECK(current.data.mark,jenv);
}

void do_rg(char *v) {
	char *p;

	p = strchr(v,'-');
	if (!p) return;

	*p++ = 0;

	current.data.s = atoi(v);
	current.data.e = atoi(p);
	DOCHECK(validate_rg(current.data.s,current.data.e),jenv);
	setpage(PAGE_ATTACH);
}

void do_copyfolder(char *v) {
	UDEC(v)
	DOCHECK(validate_subfolder(v),jenv);
	if (*v == '0' && *++v == 0) return;
	current.data.copyfolder = v;
}

void do_crfolder(char *v) {
	UDEC(v)
	DOCHECK(validate_cfolder(v),jenv);
	if (*v == '0' && *++v == 0) return;
	current.data.cfolder = v;
}

void do_cf(char *v) {
	int p;

	p = atoi(v);
	DOCHECK(validate_cf(p),jenv);
	switch (p) {
		case CF_CFOLDER:
			setpage(PAGE_CFOLDER);
			break;
		case CF_DFOLDER:
			current.todo = DO_DFOLDER;
			break;
		case CF_COMPOSE:
			setpage(PAGE_COMPOSE);
			pagedetected = 1;
			break;
		case CF_RESEND:
			setpage(PAGE_RESEND);
			pagedetected = 1;
			break;
		case CF_EFOLDER:
			current.todo = DO_EFOLDER;
			break;
		case CF_PREFS:
			setpage(PAGE_PREFS);
			pagedetected = 1;
			break;
		case CF_DROPSESS:
			drop_session();
			break;
		case CF_ABOOK:
			setpage(PAGE_ABOOK);
			break;
		case CF_ABOOK_ADD:
			setpage(PAGE_ABOOK_ADD);
			pagedetected = 1;
			break;
		case CF_ABOOK_VIEW:
			setpage(PAGE_ABOOK_VIEW);
			pagedetected = 1;
			break;
		case CF_ABOOK_GET:
			setpage(PAGE_ABOOK_GET);
			pagedetected = 1;
			break;
		case CF_SOURCE:
			setpage(PAGE_SOURCE);
			pagedetected = 1;
			break;
		case CF_DRAFT:
			setpage(PAGE_COMPOSE);
			pagedetected = 1;
			current.cdata.isdraft = 1;
			break;
		case CF_PRINT:
			setpage(PAGE_PRINT);
			pagedetected = 1;
			break;
		case CF_FILTER:
			setpage(PAGE_FILTER);
			pagedetected = 1;
			break;
		case CF_FILTER_ADD:
			setpage(PAGE_FILTER_ADD);
			pagedetected = 1;
			break;
		case CF_FILTER_EDIT:
			setpage(PAGE_FILTER_EDIT);
			pagedetected = 1;
			break;
		default:
			break;
	}
}

void do_todel(char *v) {
	DOCHECK(validate_todel(v),jenv);
	current.cdata.todel = v;
	setpage(PAGE_UPLOAD_RESULT);
	pagedetected = 1;	
}

void do_to(char *v) {
	UDEC(v)
	DOCHECK(validate_email(v),jenv);
	current.cdata.to = v;
}

void do_rmsg(char *v) {
	UDEC(v)
	if (!v || *v == 0) return;
	current.rmsg = v;
	current.fmsg = NULL;
	DOCHECK(validate_msg(current.rmsg),jenv);
}
void do_fmsg(char *v) {
	UDEC(v)
	if (!v || *v == 0) return;
	current.fmsg = v;
	current.rmsg = NULL;
	DOCHECK(validate_msg(current.fmsg),jenv);
}

void do_subj(char *v) {
	UDEC(v)
	current.cdata.subject = (unsigned char *) v;
}

void c_copy(char *v) {
	UDEC(v)
	DOCHECK(validate_cc(v),jenv);
	current.cdata.copy = v;
}

void c_prio(char *v) {
	int p;

	p = atoi(v);
	DOCHECK(validate_prio(p),jenv);
	current.cdata.prio = p;
}

void c_body(char *v) {
	UDEC(v)
	current.cdata.body = (unsigned char *) v;
}

void do_prname(char *v) {

	UDEC(v)
	DOCHECK(validate_prname(v),jenv);
	if (current.prefs.name) free(current.prefs.name);
	current.prefs.name = (unsigned char *) strdup(v);
	current.todo = DO_SAVEPREFS;
}

void do_prpp(char *v) {
	current.prefs.pp = atoi(v);
	DOCHECK(validate_prpp(current.prefs.pp),jenv);
	current.todo = DO_SAVEPREFS;
}

void do_pract(char *v) {
	current.prefs.otd = 0;
	current.prefs.sby_order = SBY_ORDER_ASC;
}

void do_otd(char *v) {
	current.prefs.otd = 1;
}

void do_prsign(char *v) {
	UDEC(v)
	DOCHECK(validate_prsign(v),jenv);
	if (current.prefs.sign) free(current.prefs.sign);
	current.prefs.sign = (unsigned char *) strdup(v);
	current.todo = DO_SAVEPREFS;
}


void do_sby_order(char *v) {
	if (*v != SBY_ORDER_ASC && *v != SBY_ORDER_DESC) return;
	current.prefs.sby_order = *v;
}

void do_abnick(char *v) {
	int len;
	UDEC(v)
	ABTR(v,AB_NAME_LEN)
	DOCHECK(validate_abnick(v),jenv);
	strncpy((char *)AB->nick,v,len);
}

void do_abemail(char *v) {
	int len;
	UDEC(v)
	ABTR(v,EMAIL_LEN)
	DOCHECK(validate_email(v),jenv);
	strncpy(AB->email,v,len);
}

void do_abname(char *v) {
	int len;
	UDEC(v)
	ABTR(v,AB_NAME_LEN)
	DOCHECK(validate_abnick(v),jenv);
	strncpy((char *)AB->name,v,len);
}

void do_abrtel(char *v) {
	int len;
	UDEC(v)
	ABTR(v,AB_TEL_LEN)
	DOCHECK(validate_abtel(v),jenv);
	strncpy(AB->rtel,v,len);
}

void do_abdtel(char *v) {
	int len;
	UDEC(v)
	ABTR(v,AB_TEL_LEN)
	DOCHECK(validate_abtel(v),jenv);
	strncpy(AB->dtel,v,len);
}

void do_abmtel(char *v) {
	int len;
	UDEC(v)
	ABTR(v,AB_TEL_LEN)
	DOCHECK(validate_abtel(v),jenv);
	strncpy(AB->mtel,v,len);
}

void do_abdaddr(char *v) {
	int len;
	UDEC(v)
	ABTR(v,AB_ADDR_LEN)
	strncpy((char *)AB->daddr,v,len);
}

void do_abraddr(char *v) {
	int len;
	UDEC(v)
	ABTR(v,AB_ADDR_LEN)
	strncpy((char *)AB->raddr,v,len);
}

void do_abcomm(char *v) {
	int len;
	UDEC(v)
	ABTR(v,AB_COMM_LEN)
	strncpy((char *)AB->comm,v,len);
}

#define DECLARE_FILTER(a,b) \
		void do_##a##b(char *v) { \
			do_##a(v,b);	\
		}


void do_f(char *v,int num) {
	int p;
	fpart_t *fp;

	fp = &FT.fp[num];

	p = atoi(v);
	if (!p) return;
	DOCHECK(validate_field(p),jenv);
	fp->field = p;
}

void do_yno(char *v,int num) {
	fpart_t *fp;

	fp = &FT.fp[num];
	if (*v == 'y') fp->yno = 1;
	else fp->yno = 0;
}

void do_fact(char *v,int num) {
	fpart_t *fp;

	UDEC(v)
	DOCHECK(validate_fact(v),jenv);
	fp = &FT.fp[num];
	/* length already validated */
	strcpy((char *)fp->value,v);
}

void do_fandor(char *v) {
	FT.andor = (*v == '1') ? 1 : 0;
}

void do_wtodo(char *v) {
	int p;

	p = atoi(v);
	DOCHECK(validate_wtodo(p),jenv);
	FT.act[0] = p;
}

void do_wfolder(char *v) {
	UDEC(v)
	DOCHECK(validate_subfolder(v),jenv);
	//FT.act_data[0] = (unsigned char *) strdup(v);
	strcpy((char *)&FT.act_data[0],v);
}

void do_fresend(char *v) {
	UDEC(v)
	DOCHECK(validate_email(v),jenv);
	FT.act[1] = FACT_SENDMSG;
	strcpy((char *)&FT.act_data[1],v);
}

void do_fprio(char *v) {
	FT.act[2] = FACT_MARK_PRIO;
}

void do_forder(char *v) {
	int p;

	p = atoi(v);
	DOCHECK(validate_forder(p),jenv);
	FT.order = p;
}

void do_fedit(char *v) {
	DOCHECK(validate_fedit(v),jenv);
	strncpy(FT.key,(char *)v,6);
}

void do_fdarr(char *v) {
	DOCHECK(validate_fedit(v),jenv);
	if (!current.fdata.fdarr) {
		current.fdata.fdarr = (char **) malloc(sizeof(char *) * MAX_FILTERS);
		if (!current.fdata.fdarr) {
			SETERR(E_NOMEM,NULL);
			return;
		}
	}

	current.fdata.fdarr[current.fdata.fdarr_len++] = v;
}

DECLARE_FILTER(f,0)
DECLARE_FILTER(f,1)
DECLARE_FILTER(f,2)
DECLARE_FILTER(yno,0)
DECLARE_FILTER(yno,1)
DECLARE_FILTER(yno,2)
DECLARE_FILTER(fact,0)
DECLARE_FILTER(fact,1)
DECLARE_FILTER(fact,2)


params_t params_get[] = {
	{ .name = "folder", 	.do_work = do_folder },			// OK
	{ .name = "sby", 	.do_work = do_sort_by },		// OK
	{ .name = "sbyo", 	.do_work = do_sby_order },		// OK
	{ .name = "msg", 	.do_work = do_msg },			// OK
	{ .name = "amsg", 	.do_work = do_amsg },			// OK
	{ .name = "p",		.do_work = do_start_from },		// OK
	{ .name = "rg",		.do_work = do_rg },			// OK
	{ .name = "cf",		.do_work = do_cf },			// OK
	{ .name = "todel",	.do_work = do_todel },			// OK
	{ .name = "adel",	.do_work = do_adel },			// OK
	{ .name = "ra",		.do_work = do_ra },			// OK
	{ .name = "fedit",	.do_work = do_fedit },			// OK
	{ .name = NULL, 	.do_work = NULL }
};

params_t params_post[] = {
	{ .name = "page", 		.do_work = do_page },    	// OK
	{ .name = "email", 		.do_work = do_email },		// OK
	{ .name = "password", 		.do_work = do_password },	// OK
	{ .name = "todo",		.do_work = do_todo },		// OK
	{ .name = "markas",		.do_work = do_c_mark },		// OK
	{ .name = "marr",		.do_work = do_marr },		// OK
	{ .name = "abarr",		.do_work = do_abarr },		// OK
	{ .name = "copyto",		.do_work = do_copyfolder },	// OK
	{ .name = "c_to",		.do_work = do_to },		// OK
	{ .name = "c_subj",		.do_work = do_subj },		// OK - no need ?
	{ .name = "c_copy",		.do_work = c_copy },		// OK
	{ .name = "c_body",		.do_work = c_body },		// OK - no need ?
	{ .name = "c_prio",		.do_work = c_prio },		// OK
	{ .name = "f_create",		.do_work = do_crfolder },	// OK
	{ .name = "r_msg",		.do_work = do_rmsg },		// OK
	{ .name = "f_msg",		.do_work = do_fmsg },		// OK
	{ .name = "pr_name",		.do_work = do_prname },		// OK
	{ .name = "pr_pp",		.do_work = do_prpp },		// OK
	{ .name = "pr_sign",		.do_work = do_prsign },		// OK
	{ .name = "pr_otd",		.do_work = do_otd },		// OK
	{ .name = "pr_act",		.do_work = do_pract },		// OK
	{ .name = "pr_sby",		.do_work = do_sort_by },	// OK
	{ .name = "pr_sbyo",		.do_work = do_sby_order },	// OK
	{ .name = "ab_nick",		.do_work = do_abnick },		// OK
	{ .name = "ab_email",		.do_work = do_abemail },	// OK
	{ .name = "ab_name",		.do_work = do_abname },		// OK
	{ .name = "ab_dtel",		.do_work = do_abdtel },		// OK
	{ .name = "ab_rtel",		.do_work = do_abrtel },		// OK
	{ .name = "ab_mtel",		.do_work = do_abmtel },		// OK
	{ .name = "ab_raddr",		.do_work = do_abraddr },	// OK - no need ?
	{ .name = "ab_daddr",		.do_work = do_abdaddr },	// OK - no need ?
	{ .name = "ab_comm",		.do_work = do_abcomm },		// OK - no need ?
	{ .name = "edit_what",		.do_work = do_edit_what },	// OK
	{ .name = "field0",		.do_work = do_f0 },		// OK
	{ .name = "field1",		.do_work = do_f1 },		// OK
	{ .name = "field2",		.do_work = do_f2 },		// OK
	{ .name = "yno0",		.do_work = do_yno0 },		// OK
	{ .name = "yno1",		.do_work = do_yno1 },		// OK
	{ .name = "yno2",		.do_work = do_yno2 },		// OK
	{ .name = "fact0",		.do_work = do_fact0 },		// OK
	{ .name = "fact1",		.do_work = do_fact1 },		// OK
	{ .name = "fact2",		.do_work = do_fact2 },		// OK
	{ .name = "andor",		.do_work = do_fandor },		// OK
	{ .name = "what_todo",		.do_work = do_wtodo },		// OK
	{ .name = "what_todo_folder",	.do_work = do_wfolder },	// OK	
	{ .name = "fresend",		.do_work = do_fresend },	// OK
	{ .name = "fprio",		.do_work = do_fprio },		// OK	
	{ .name = "forder",		.do_work = do_forder },		// OK
	{ .name = "fdarr",		.do_work = do_fdarr },
	{ .name = NULL, .do_work = NULL }
};
