#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(XULRUNNER_SDK)
#include <npapi.h>
#elif defined(WEBKIT_DARWIN_SDK)
#include <Webkit/npapi.h>
#endif

#include "bta.h"

#define API_TRACK 0
#define API_PAYMENT_CREATED 1
#define API_PAYMENT_CLICKED 2
#define API_PAYMENT_CLOSED 3
#define API_SET_USER 4
#define API_GOT_URL 5
#define API_GOT_PIN 6
#define API_GOT_ERROR 7

static char *BTA_DATAFILE;
static char bta_payment_posturl[102400];
static char bta_user[ BTA_ID_LENGTH+1 ];
static char *bta_pv_buf;
static unsigned int bta_pv_len=0;
BTA_SYS_WINDOW browser_win;
static char *bta_notify_ptr = "hi!";

typedef struct _bta_info {
	NPP instance;
	int action;

	int type;   // 1=subscription, 2=payment
	char site[ BTA_ID_LENGTH+1 ];
	char check[33];
	char *posturl;
	char *desc;
	float price;

	struct _bta_info *next;
} bta_info;

bta_info *current_clickables = NULL;
bta_info *current_prompt = NULL;
bta_info message;
char *buf=NULL;
char current_pin[32];

static NPNetscapeFuncs *npnfuncs = NULL;

void _bta_api_gotURL(NPP inst, const char *url, char *resp);
void _bta_api_set_user(NPP inst, const char *user_token);
void _bta_api_count_site(NPP inst, const char *tag);
void _bta_api_payment_instance(NPP inst, const char *site, float price, NPBool recurring, const char *posturl, const char *desc, const char *check);
void _bta_api_clicked(NPP instance);
void _bta_api_got_pin(NPP inst, const char *pin);
void _bta_api_close_instance(NPP instance);
void _bta_api_error(NPP inst, const char *message);

////////////////////////////////////////////////////////////
// initialize/shutdown functions
////////////////////////////////////////////////////////////

int bta_api_init(NPNetscapeFuncs *npnf) {
  FILE *bta_fp = NULL;
#ifdef _WINDOWS
	const char *filename = "betterthanads.dat";
	char *dir = getenv("APPDATA");
#else
	const char *filename = ".betterthanads";
	char *dir = getenv("HOME");
#endif
	int len = strlen(dir)+strlen(filename)+10;
	
	npnfuncs=npnf;
	BTA_DATAFILE = (char *)bta_malloc(len);
	if( BTA_DATAFILE==NULL ) return FALSE;
	bta_pv_buf = (char *)bta_malloc( (BTA_ID_LENGTH+1+5)*BTA_PAGEVIEWS );
	if( bta_pv_buf==NULL ) {
		bta_free(BTA_DATAFILE);
		return 1;
	}

	sprintf(BTA_DATAFILE, "%s/%s", dir, filename);
	fprintf(stderr, "data stored in '%s'\n", BTA_DATAFILE);

	bta_fp = fopen(BTA_DATAFILE, "r+");
	if( !bta_fp ) {
		bta_free(bta_pv_buf);
		bta_free(BTA_DATAFILE);
		return 1;
	}

	if( fscanf(bta_fp, "user=%19s", bta_user) != 1 ) {
		bta_free(bta_pv_buf);
		bta_free(BTA_DATAFILE);
		fclose(bta_fp);
		return 1;
	}
	sprintf(bta_pv_buf, "user=%19s", bta_user);
	bta_pv_len=strlen(bta_pv_buf);
	while( !feof(bta_fp) && bta_pv_len<BTA_BUFFER_SIZE ) {
		bta_pv_len+=fread(bta_pv_buf+bta_pv_len, 1, BTA_BUFFER_SIZE-bta_pv_len, bta_fp);
	}
	bta_pv_buf[bta_pv_len]=0;
	fclose(bta_fp);

	npnfuncs->getvalue(0, NPNVnetscapeWindow, &browser_win);
	bta_sys_init(browser_win);
  bta_sys_start_apithread();
	return 0;
}

void bta_api_close() {
  bta_sys_stop_apithread();
	bta_sys_close();

	FILE *bta_fp = fopen(BTA_DATAFILE, "r+");
	fseek(bta_fp, 0, SEEK_SET);
	fputs(bta_pv_buf, bta_fp);
	fclose(bta_fp);

	bta_free(bta_pv_buf);
	bta_free(BTA_DATAFILE);

	if( message.posturl!=NULL ) bta_free(message.posturl);
	if( buf!=NULL ) bta_free(buf);
	//if( bta_payment_posturl ) bta_free(bta_payment_posturl);
}

void _bta_api_error(NPP inst, const char *message) {
	fprintf(stderr, "API error: %s\n", message);
}
void bta_api_error(NPP inst, const char *message) {
	fprintf(stderr, "API error: %s\n", message);
}

void *bta_malloc(int size) {
	fprintf(stderr, "malloc size %d\n", size);
  return npnfuncs->memalloc(size);
}
void bta_free(void *ptr) {
  npnfuncs->memfree(ptr);
}

void *bta_api_thread(void *x) {
	while( true ) {
		bta_sys_wait_dataready();
		if( !bta_sys_is_running() ) break;

		bta_sys_lock_dataload();
		switch( message.action ) {
			case API_TRACK:
				_bta_api_count_site(message.instance, message.site);
				break;
			case API_PAYMENT_CREATED:
				_bta_api_payment_instance(message.instance, message.site, message.price, message.type==1, message.posturl, message.desc, message.check);
				break;
			case API_PAYMENT_CLICKED:
				_bta_api_clicked(message.instance);
				break;
			case API_PAYMENT_CLOSED:
				_bta_api_close_instance(message.instance);
				break;
			case API_SET_USER:
				_bta_api_set_user(message.instance, message.site);
				break;
			case API_GOT_URL:
				_bta_api_gotURL(message.instance, message.posturl, message.desc);
				break;
			case API_GOT_PIN:
				_bta_api_got_pin(message.instance, message.check);
				break;
			case API_GOT_ERROR:
				_bta_api_error(message.instance, message.desc);
				break;
		}
		bta_sys_unlock_dataload();
	}
}

static char bta_post[ BTA_BUFFER_SIZE ];
// posts the urlencoded-string data to url asynchronously
void old_bta_post_data(NPP inst, const char *url, const char *data, const char *target) {
	int len = data?strlen(data):0;
	bta_post[0]=0;
	if( len>(BTA_BUFFER_SIZE-80) ) 
		logmsg("post data too large\n");
	else if( data!=NULL )
		sprintf(bta_post, "Content-Type: application/x-www-form-urlencoded\nContent-Length: %d\n\n%s", len, data);
	
	if( !url ) return;
	fprintf(stderr, "URL: %s\nPOST: %s\n", url, bta_post);

	int r=npnfuncs->posturlnotify(
			0,//inst, // instance
			url,
			target, // NULL=send response back to plugin
			strlen(bta_post),    // length of data to send
		  bta_post,  // data to send
			false,  // bta_post is a buffer, not a file
			NULL
		);

	fprintf(stderr, "posturlnotify(%x,'%s','%s',%d,<postdata>,false,null) error: %d\n", inst, url, target, strlen(bta_post), r);
}

struct _post_struct {
	NPP inst;
	int len;
	char *buf;
	char *url;
	char *target;
};

void __bta_post_data(void *ptr) {
	struct _post_struct *x = (struct _post_struct *)ptr;
	fprintf(stderr, "buf @ 0x%x, url @ 0x%x, target @ 0x%x\n", x->buf, x->url, x->target);
	int r=npnfuncs->posturlnotify(x->inst, x->url, x->target, x->len, x->buf, FALSE, ptr);
	fprintf(stderr, "posturlnotify(%x,'%s','%s',%d,'%s',false,null) error: %d\n", x->inst, x->url, x->target, x->len, x->buf, r);
}

void _bta_post_data(NPP inst, const char *url, const char *data, const char *target) {
	int len = 0;
	struct _post_struct *x;
  if( data==NULL || url==NULL ) return;

	x = (struct _post_struct *)bta_malloc(sizeof(struct _post_struct)+strlen(data)+strlen(url)+128);
  if( x==NULL ) return;
	x->inst=inst;
	x->buf = (char *)x+sizeof(struct _post_struct);  //(char *)bta_malloc(strlen(data))+75;
	if( x->buf==NULL ) return;
	sprintf(x->buf, "Content-Type: application/x-www-form-urlencoded\nContent-Length: %d\n\n%s", strlen(data), data);
	fprintf(stderr, "'%s'\n", x->buf);
	x->len=strlen(x->buf);
	x->url = x->buf+strlen(x->buf)+1; //(char *)bta_malloc(strlen(url));
	if( x->url==NULL ) return;
	strcpy(x->url, url);
	if( target==NULL ) {
		x->target=NULL;
	} else {
		x->target= x->url+strlen(x->url+1);//(char *)bta_malloc(strlen(target));
		if( x->target==NULL ) return;
		strcpy(x->target, target);
	}

	fprintf(stderr, "buf @ 0x%x, url @ 0x%x, target @ 0x%x\n", x->buf, x->url, x->target);

	//pthread_yield();
	npnfuncs->pluginthreadasynccall(inst, __bta_post_data, x);
}

void bta_post_data(NPP inst, const char *url, const char *data) {
  _bta_post_data(inst, url, data, NULL);
}

////////////////////////////////////////////////////////////
// BTA NPAPI callbacks
////////////////////////////////////////////////////////////

void bta_api_gotURL(NPP inst, const char *url, char *resp, void *notifyData) {
	struct _post_struct *x = (struct _post_struct *)notifyData;
	//free(x->url);
	//free(x->buf);
	//if(x->target!=NULL) free(x->target);
	bta_free(x);

	bta_sys_lock_dataload();
	message.action=API_GOT_URL;
	message.instance=inst;

	if( message.posturl!=NULL ) bta_free(message.posturl);
	message.posturl = (char *)bta_malloc(strlen(url)+strlen(resp)+2);
	if( message.posturl==NULL ) {
		bta_sys_unlock_dataload();
		return;
	}
	message.desc = message.posturl+strlen(url)+1;
	strcpy(message.posturl, url);
	strcpy(message.desc, resp);

	bta_sys_post_dataready();
	bta_sys_unlock_dataload();
}
void bta_api_set_user(NPP inst, const char *user_token) {
		bta_sys_lock_dataload();
		  message.action=API_SET_USER;
		  message.instance=inst;
			strncpy(message.site, user_token, BTA_ID_LENGTH+1);
			message.site[BTA_ID_LENGTH]=0;
		bta_sys_post_dataready();
		bta_sys_unlock_dataload();
}
void bta_api_count_site(NPP inst, const char *tag) {
		bta_sys_lock_dataload();
		  message.action=API_TRACK;
		  message.instance=inst;
			strncpy(message.site, tag, BTA_ID_LENGTH+1);
			message.site[BTA_ID_LENGTH]=0;
		bta_sys_post_dataready();
		bta_sys_unlock_dataload();
}
void bta_api_payment_instance(NPP inst, const char *site, float price, NPBool recurring, const char *posturl, const char *desc, const char *check) {
		bta_sys_lock_dataload();
		  message.action=API_PAYMENT_CREATED;
		  message.instance=inst;
		  message.price=price;
		  message.type=recurring?1:2;
			strncpy(message.site, site, BTA_ID_LENGTH+1);
			message.site[BTA_ID_LENGTH]=0;
			strncpy(message.check, check, 33);
			message.check[32]=0;

			if( message.posturl!=NULL ) bta_free(message.posturl);
			message.posturl = (char *)bta_malloc(strlen(posturl)+strlen(desc)+2);
			if( message.posturl==NULL ) {
				bta_sys_unlock_dataload();
				return;
			}
			message.desc = message.posturl+strlen(posturl)+1;
			strcpy(message.posturl, posturl);
			strcpy(message.desc, desc);

		bta_sys_post_dataready();
		bta_sys_unlock_dataload();
}
void bta_api_clicked(NPP instance) {
		bta_sys_lock_dataload();
		  message.action=API_PAYMENT_CLICKED;
		  message.instance=instance;
		bta_sys_post_dataready();
		bta_sys_unlock_dataload();
}
void bta_api_got_pin(NPP inst, const char *pin) {
		bta_sys_lock_dataload();
		  message.action=API_GOT_PIN;
		  message.instance=inst;
			strncpy(message.check, pin, 33);
			message.check[32]=0;
		bta_sys_post_dataready();
		bta_sys_unlock_dataload();
}
void bta_api_close_instance(NPP instance) {
		bta_sys_lock_dataload();
		  message.action=API_PAYMENT_CLOSED;
		  message.instance=instance;
		bta_sys_post_dataready();
		bta_sys_unlock_dataload();
}

////////////////////////////////////////////////////////////
// BTA NPAPI callbacks
////////////////////////////////////////////////////////////

// gets notification that url completed
void _bta_api_gotURL(NPP inst, const char *url, char *resp) {
	logmsg("np-bta: NP_URLNotify: ");

	if( !url ) return;
	logmsg(url);
	logmsg("\n");

	if( strcmp(url, BTA_API_PAGEVIEWS)==0 ) {
		if( strcmp(resp, "OK")==0 ) {
			// truncate file and cache
			FILE *bta_fp = fopen(BTA_DATAFILE, "w+");
			fprintf(bta_fp, "user=%19s", bta_user);
			fclose(bta_fp);
			bta_pv_buf[25]=0;
			bta_pv_len=strlen(bta_pv_buf);
			fprintf(stderr, "BetterThanAds: Posted pageviews successfully\n");
		} else {
			fprintf(stderr, "BetterThanAds: Remote Error posting pageviews: \n%s\n", resp);
		}

	} else if( strcmp(url, BTA_API_PAYMENT)==0 ) {
		if( strncmp(resp, "bta_token=", 9)==0 ) {
			// post token to postback url
			_bta_post_data(inst, bta_payment_posturl, resp, "_parent");
		} else {
			//TODO: error dialog
			bta_api_error(inst, "invalid pin?");
		}
	}
}

void _bta_api_clicked(NPP instance) {
	int sz=0;
	  bta_info *cur = current_clickables;
		while( cur!=NULL ) {
			if( cur->instance==instance ) break;
			cur=cur->next;
		}
		if( cur==NULL )
			return;// NPERR_NO_ERROR;
		if( current_prompt!=NULL )
			return;// NPERR_GENERIC_ERROR;

		sz = 250+strlen(cur->desc);
		if( buf!=NULL ) bta_free(buf);
		buf = (char *)bta_malloc(sz);
		if( !buf ) return;// NPERR_GENERIC_ERROR;
		
		sprintf(buf, "I will pay $%3.2f%s for the following:\n\n%s",
				    cur->price, cur->type==1?"/mo":"", cur->desc);

	  current_prompt=cur;
		bta_sys_prompt(instance, buf);
}

////////////////////////////////////////////////////////////
// BTA API functions
////////////////////////////////////////////////////////////

void _bta_api_count_site(NPP inst, const char *tag) {
	static int logged=0;
	char *ptr, last;
	int pos=0, count=1;
	fprintf(stderr, "bta_api_count_site\n");
	
	if( strlen(tag)!=19 ) return;
	
	ptr=strstr(bta_pv_buf, tag);
	if( ptr==NULL ) {
		sprintf(bta_pv_buf+bta_pv_len, "&%19s=%05d", tag, 1);
		bta_pv_len=strlen(bta_pv_buf);
	}	else {
		pos=ptr-bta_pv_buf+20;
		last=bta_pv_buf[pos+5];
		sscanf(bta_pv_buf+pos, "%05d", &count);
		sprintf(bta_pv_buf+pos, "%05d", count+1);
		bta_pv_buf[pos+5]=last;
	}
	
	if( ((logged++)%BTA_PAGEVIEWS)==0 ) {
		bta_post_data(inst, BTA_API_PAGEVIEWS, bta_pv_buf);
	}
}

void _bta_api_set_user(NPP inst, const char *user_token) {
	FILE *bta_fp;

	if( strcmp(user_token, bta_user)==0 ) return;

	bta_fp = fopen(BTA_DATAFILE, "w");
	if( !bta_fp ) {
		logmsg("error opening datafile\n");
		return;
	}
	fprintf(bta_fp, "user=%19s", user_token);
	fclose(bta_fp);
	strcpy(bta_user, user_token);
	sprintf(bta_pv_buf, "user=%19s", user_token);
	bta_pv_len=strlen(bta_pv_buf);
}

/////////////////////////////////////////////////////////////

void _bta_api_close_instance(NPP instance) {
	bta_info *prv = (bta_info *)NULL;
	bta_info *cur = current_clickables;
		
	while( cur!=NULL ) {
		if( cur->instance==instance ) {
			if( prv==NULL ) current_clickables=cur->next;
			else prv->next=cur->next;

			bta_free(cur);
			return;// NPERR_NO_ERROR;
		}
		prv=cur;
		cur=cur->next;
	}
}

static char __api_buf[ BTA_BUFFER_SIZE ];
void _bta_api_got_pin(NPP inst, const char *pin) {
	fprintf(stderr, "got pin: '%s'\n", pin);

	if( pin[0] != 'x' ) {
		__api_buf[0]=0;
		//if( bta_payment_posturl!=NULL ) bta_free(bta_payment_posturl);
		//bta_payment_posturl=(char*)bta_malloc(strlen(current_prompt->posturl));
		strcpy(bta_payment_posturl, current_prompt->posturl);
		sprintf(__api_buf, "site=%19s&user=%19s&pin=%s&price=%3.2f&check=%32s&type=%s&description=%s",
				//current_prompt->site, bta_user, pin, current_prompt->price, current_prompt->check, current_prompt->type==1?"subscription":"payment", "abcdefg");
				current_prompt->site, bta_user, pin, current_prompt->price, current_prompt->check, current_prompt->type==1?"subscription":"payment", current_prompt->desc);

		bta_post_data(inst, BTA_API_PAYMENT, __api_buf);
	}
	current_prompt=NULL;
}

void _bta_api_payment_instance(NPP inst, const char *site, float price, NPBool recurring, const char *posturl, const char *desc, const char *check) {
		// put it all in one block
		int sz = sizeof(bta_info)+strlen(posturl)+strlen(desc)+2;
		char *ptr = (char *)bta_malloc( sz );
		bta_info *nbta = (bta_info *)ptr;
		ptr+=sizeof(bta_info);

		nbta->instance=inst;
		nbta->type=recurring?1:2;
		nbta->price=price;
		strcpy(nbta->site, site);
		strcpy(nbta->check, check);
		nbta->posturl=ptr;
		  strcpy(nbta->posturl, posturl);
			ptr+=1+strlen(posturl);
		nbta->desc=ptr;
		  strcpy(nbta->desc, desc);
			ptr+=1+strlen(desc);

		nbta->next=current_clickables;
		current_clickables=nbta;
}
