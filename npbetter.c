// BetterThanAds Plugin - site tracking, microsubscription and payment plugin
// Copyright (C) 2009 Jeremy Jay <jeremy@betterthanads.com>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
/////////////
//
// BetterThanAds NPAPI Plugin
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bta.h"

static NPNetscapeFuncs *npnfuncs = NULL;

void logmsg(const char *msg) {
#ifdef DEBUG
#ifndef _WINDOWS
	fputs(msg, stderr);
	fflush(stderr);
#else
	FILE *out = fopen("\\np-bta.log", "a");
	fputs(msg, out);
	fclose(out);
#endif
#endif
}

////////////////////////////////////////////////////////////
// url processing
////////////////////////////////////////////////////////////
static char url_resp[ BTA_BUFFER_SIZE ];

struct _post_struct {
	NPP inst;
	int len;
	char *buf;
	char *url;
	char *target;
  void (*callback)(NPP inst, const char *url,const char *data );
};

// posts the urlencoded-string data to url asynchronously
void __bta_post_data(void *ptr) {
	struct _post_struct *x = (struct _post_struct *)ptr;
	int r=npnfuncs->posturlnotify(x->inst, x->url, x->target, x->len, x->buf, false, ptr);
	//fprintf(stderr, "posturlnotify(%x,'%s','%s',%d,'%s',false,null) error: %d\n", x->inst, x->url, x->target, x->len, x->buf, r);
}

void bta_post_data(NPP inst, const char *url, const char *data, const char *target, 	
  void (*callback)(NPP inst, const char *url,const char *data), bool async ) {

	int len = 0;
	struct _post_struct *x;
  if( data==NULL || url==NULL ) return;

	x = (struct _post_struct *)bta_malloc(sizeof(struct _post_struct)+strlen(data)+strlen(url)+128);
  if( x==NULL ) return;
	x->inst=inst;
	x->buf = (char *)x+sizeof(struct _post_struct);
	if( x->buf==NULL ) return;
	sprintf(x->buf, "Content-Type: application/x-www-form-urlencoded\nContent-Length: %d\n\n%s", strlen(data), data);
	x->len=strlen(x->buf);
	x->url = x->buf+strlen(x->buf)+1;
	if( x->url==NULL ) return;
	strcpy(x->url, url);
	if( target==NULL ) {
		x->target=NULL;
	} else {
		x->target= x->url+strlen(x->url)+1;
		if( x->target==NULL ) return;
		strcpy(x->target, target);
	}
	x->callback=callback;

	if( async ) {
		// NPN_PostURLNotify must be called from main API thread
		//npnfuncs->pluginthreadasynccall(inst, __bta_post_data, x);
		fprintf(stderr, "-- async posting not implemented.\n");
	} else {
		npnfuncs->posturlnotify(x->inst, x->url, x->target, x->len, x->buf, false, x);
	  //fprintf(stderr, "posturlnotify(%x,'%s','%s',%d,'%s',false,null) error: %d\n", x->inst, x->url, x->target, x->len, x->buf, r);
	}
}

// new stream started
static NPError new_stream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype) {
	return NPERR_NO_ERROR;
}

// closing stream
static NPError destroy_stream(NPP instance, NPStream* stream, NPReason reason) {
	return NPERR_NO_ERROR;
}

// how much data can we take?
static int32_t write_ready(NPP instance, NPStream* stream) {
	return BTA_BUFFER_SIZE;
}

// take some data from the stream
int32_t write_data(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* dbuf) {
	// yup, it overwrites. none of the api calls should return more than BTA_BUFFER_SIZE bytes anyway.
	if( len>=BTA_BUFFER_SIZE ) len=BTA_BUFFER_SIZE;
	memcpy(url_resp, ((char *)dbuf)+offset, len);
	url_resp[len]=0;
	return len;
}

// url notifications
void url_notify(NPP instance, const char* url,	NPReason reason, void *v ) {
	struct _post_struct *x = (struct _post_struct *)v;
	if( v!=NULL && x->callback!=NULL ) 
		x->callback(instance, url, url_resp);
	bta_free(x);
}

////////////////////////////////////////////////////////////

static NPError new_instance(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char *argn[], char *argv[], NPSavedData *saved) {
	const char *emptystr="";
	const char *domain=NULL;
	const char *site_token=NULL;
	const char *desc=emptystr;
	const char *posturl=NULL;
	const char *check=NULL;
	float price=0.0;
	int i=0, action=0;

	for(i=0; i<argc; i++) {
			if( strcmp(argn[i], "user")==0 ) {
				site_token=argv[i];
				bta_api_set_user(instance, site_token);
				
				// force plugin reload to ensure all instances get new userid
				//npnfuncs->reloadplugins(true);
				return NPERR_NO_ERROR;
			}
			
			if( strcmp(argn[i], "domain")==0 ) // TODO: remove me sometime
				domain=argv[i];

			if( strcmp(argn[i], "site")==0 )
				site_token=argv[i];
			if( strcmp(argn[i], "price")==0 )
				price=(float)atof( argv[i] );
			if( strcmp(argn[i], "check")==0 )
				check=argv[i];
			if( strcmp(argn[i], "posturl")==0 )
				posturl=argv[i];
			if( strcmp(argn[i], "description")==0 )
				desc=argv[i];

			if( strcmp(argn[i], "ptype")==0 ) {
				if( strcmp(argv[i], "subscription")==0 ) action=1;
				if( strcmp(argv[i], "payment")==0 ) action=2;
			}

	}

	if( site_token==NULL )
		return NPERR_GENERIC_ERROR;

	// TODO: remove me sometime
	if( domain!=NULL ) {
		char urlbuf[4096];
    sprintf(urlbuf, "http://api.betterthanads.com/adddomain/%s/%s", domain, site_token);
		npnfuncs->geturl(instance, urlbuf, NULL);
	}

	if( action==0 ) { 
		NPBool privateMode=0;

		if( npnfuncs->getvalue(instance, (NPNVariable)18 /*NPNVprivateModeBool*/, &privateMode) == NPERR_NO_ERROR ) {
			if( privateMode ) {
				fprintf(stderr, "In private mode, no tracking\n");
				return NPERR_GENERIC_ERROR;
			}
		}
		
		bta_api_count_site(instance, site_token);

	} else if( (action==1 || action==2) && price>0.0 && posturl!=NULL && check!=NULL ) { 
		
		bta_api_payment_instance(instance, site_token, price, action==1, posturl, desc, check);
	
	} else {
		return NPERR_GENERIC_ERROR;
	}
  
	return NPERR_NO_ERROR;
}

static NPError destroy_instance(NPP instance, NPSavedData **save) {
	bta_api_close_instance(instance);
	return NPERR_NO_ERROR;
}

static NPError set_window(NPP instance, NPWindow* npwin) {
  bta_api_window(instance, npwin);
	return NPERR_NO_ERROR;
}

static NPError handle_event(NPP instance, void *ev) {
	// TODO: mac callback here
	logmsg("handle_event()\n");
	return false;
}

static NPError get_value(NPP instance, NPPVariable variable, void *value) {
	switch(variable) {
	default:
		return NPERR_GENERIC_ERROR;
	case NPPVpluginNameString:
		*((char **)value) = "BetterThanAdsPlugin";
		break;
	case NPPVpluginDescriptionString:
		*((char **)value) = "<a href=\"http://betterthanads.com/\">BetterThanAds.com</a> site-supporting and micropayment plugin.";
		break;
	case NPPVpluginWindowBool:
		*((bool *)value) = true;
		break;
#if defined(XULRUNNER_SDK)
	case NPPVpluginNeedsXEmbed:
		*((bool *)value) = false;
		break;
#endif
	}

	return NPERR_NO_ERROR;
}

////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

NPError OSCALL NP_GetEntryPoints(NPPluginFuncs *nppfuncs) {
	nppfuncs->version       = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
	nppfuncs->newp          = new_instance;
	nppfuncs->destroy       = destroy_instance;
	nppfuncs->getvalue      = get_value;

	nppfuncs->event         = handle_event;
	nppfuncs->setwindow     = set_window;

	nppfuncs->urlnotify     = url_notify;
	nppfuncs->newstream     = new_stream;
	nppfuncs->destroystream = destroy_stream;
	nppfuncs->writeready    = write_ready;
	nppfuncs->write         = write_data;

	return NPERR_NO_ERROR;
}

#if !defined(_WINDOWS) && !defined(WEBKIT_DARWIN_SDK)

NPError OSCALL NP_Initialize(NPNetscapeFuncs *npnf, NPPluginFuncs *nppfuncs) {
	logmsg("npbetter (*nix) starting...\n");
	if( npnf != NULL ) {
		NP_GetEntryPoints(nppfuncs);

#else

NPError OSCALL NP_Initialize(NPNetscapeFuncs *npnf) {
	logmsg("npbetter (win) starting...\n");
	if( npnf != NULL ) {

#endif

		npnfuncs = npnf;
	} else {
		return NPERR_INVALID_FUNCTABLE_ERROR;
	}

	bta_api_init(npnfuncs);
	return NPERR_NO_ERROR;
}

NPError OSCALL NP_Shutdown() {
	bta_api_shutdown();
	return NPERR_NO_ERROR;
}

// webkit hack
NPError OSCALL NP_GetValue(void *npp, NPPVariable variable, void *value) {
	return get_value((NPP)npp, variable, value);
}

char *NP_GetMIMEDescription(void) {
	return "application/x-vnd-betterthanads-bta:.bta:BetterThanAds.com site-support tracking, micropayments and microsubscriptions";
}

#ifdef __cplusplus
}
#endif

////////////////////////////////////////////////////////////
// BTA API functions
////////////////////////////////////////////////////////////
static char *BTA_DATAFILE;               // pathname to user data file
static char bta_user[ BTA_ID_LENGTH+1 ]; // user id
static char *bta_pv_buf;                 // pageview buffer cache (need mutex?)
static unsigned int bta_pv_len=0;        // length of pageview buffer cache

int bta_api_init(NPNetscapeFuncs *npnf) {
  FILE *bta_fp = NULL;

	// get local user store filename
#ifdef _WINDOWS
	const char *filename = "betterthanads.dat";
	char *dir = getenv("APPDATA");
#else
	const char *filename = ".betterthanads";
	char *dir = getenv("HOME");
#endif
	int len = strlen(dir)+strlen(filename)+10;

	// malloc a buffer and space for the filename
	BTA_DATAFILE = (char *)bta_malloc(len);
	if( BTA_DATAFILE==NULL ) return 0;
	bta_pv_buf = (char *)bta_malloc( (BTA_ID_LENGTH+6)*BTA_PAGEVIEWS );
	if( bta_pv_buf==NULL ) {
		bta_free(BTA_DATAFILE);
		return 0;
	}

	sprintf(BTA_DATAFILE, "%s/%s", dir, filename);
	logmsg("data stored in: ");
	logmsg(BTA_DATAFILE);
	logmsg("\n");

	//try to open the datafile (dont want to create yet)
	bta_fp = fopen(BTA_DATAFILE, "r+");
	if( !bta_fp ) {
		bta_free(bta_pv_buf);
		bta_free(BTA_DATAFILE);
		return 0;
	}

	// read in the user id
	if( fscanf(bta_fp, "user=%19s", bta_user) != 1 ) {
		bta_free(bta_pv_buf);
		bta_free(BTA_DATAFILE);
		fclose(bta_fp);
		return 0;
	}

	// read and cache the rest of the datastore
	sprintf(bta_pv_buf, "user=%19s", bta_user);
	bta_pv_len=strlen(bta_pv_buf);
	while( !feof(bta_fp) && bta_pv_len<BTA_BUFFER_SIZE ) {
		int n=fread(bta_pv_buf+bta_pv_len, 1, BTA_BUFFER_SIZE-bta_pv_len, bta_fp);
		if( n<=0 ) break;
		bta_pv_len+=n;
	}
	bta_pv_buf[bta_pv_len]=0;
	fclose(bta_fp);

	bta_sys_init();
	return 1;
}

BTA_SYS_WINDOW bta_api_get_parent(NPP inst) {
	BTA_SYS_WINDOW browser_win;
	npnfuncs->getvalue(inst, NPNVnetscapeWindow, &browser_win);
	return browser_win;
}

void bta_api_shutdown() {
	bta_sys_close();

	// write the cached datastore back to disk
	FILE *bta_fp = fopen(BTA_DATAFILE, "r+");
	fseek(bta_fp, 0, SEEK_SET);
	fputs(bta_pv_buf, bta_fp);
	fclose(bta_fp);

	// release mem
	bta_free(bta_pv_buf);
	bta_free(BTA_DATAFILE);
}

void *bta_malloc(int size) {
  return npnfuncs->memalloc(size);
}
void bta_free(void *ptr) {
  npnfuncs->memfree(ptr);
}

void bta_api_count_rcvd(NPP inst, const char *url, const char *resp) {
	if( strcmp(resp, "OK")==0 ) {
		// truncate file and cache
		FILE *bta_fp = fopen(BTA_DATAFILE, "w+");
		fprintf(bta_fp, "user=%19s", bta_user);
		fclose(bta_fp);
		bta_pv_buf[25]=0;
		bta_pv_len=strlen(bta_pv_buf);
		logmsg("BetterThanAds: Posted pageviews successfully\n");
	} else {
		logmsg("BetterThanAds: Remote Error posting pageviews\n");
	}
}

void bta_api_count_site(NPP inst, const char *tag) {
	static int logged=0;
	char *ptr, last;
	int pos=0, count=1;
	
	if( strlen(tag)!=19 ) return;

	// find site tag in cache
	ptr=strstr(bta_pv_buf, tag);
	if( ptr==NULL ) {
		// not found, append to the end
		sprintf(bta_pv_buf+bta_pv_len, "&%19s=%05d", tag, 1);
		bta_pv_len=strlen(bta_pv_buf);
	}	else {
		// found, read and update count
		pos=ptr-bta_pv_buf+20;
		last=bta_pv_buf[pos+5];
		sscanf(bta_pv_buf+pos, "%05d", &count);
		sprintf(bta_pv_buf+pos, "%05d", count+1);
		bta_pv_buf[pos+5]=last;
	}

	// post to server every once in a while (0 is good cause then we do it when the browser is started)
	if( ((logged++)%BTA_PAGEVIEWS)==0 ) {
		bta_post_data(inst, BTA_API_PAGEVIEWS, bta_pv_buf, NULL, bta_api_count_rcvd, false);
	}
}

void bta_api_set_user(NPP inst, const char *user_token) {
	FILE *bta_fp;

	if( strcmp(user_token, bta_user)==0 ) return;

	// truncate/create data file
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

/////////

void bta_api_payment_instance(NPP inst, const char *site, float price, NPBool recurring, const char *posturl, const char *description, const char *check) {
	bta_info *nbta=NULL;
    nbta = (bta_info *)bta_malloc(sizeof(bta_info)+strlen(posturl)+strlen(description));
	if( !nbta ) return;

	nbta->type=recurring?1:2;
	nbta->price=price;
	strncpy(nbta->site, site, BTA_ID_LENGTH);
	  nbta->site[BTA_ID_LENGTH]=0;
	strncpy(nbta->check, check, 33);
	  nbta->check[32]=0;

	nbta->posturl=nbta->buf;
	  strcpy(nbta->posturl, posturl);

	nbta->desc=nbta->buf + strlen(posturl)+1;
		strcpy(nbta->desc, description);

	inst->pdata = nbta;
}

void bta_api_window(NPP instance, NPWindow* npwin) {
  if( instance->pdata ) {
		bta_info *nbta = (bta_info *)instance->pdata;

		bta_sys_windowhook(instance, npwin);
	}
}

void bta_api_close_instance(NPP instance) {
  if( instance->pdata ) {
		bta_info *nbta = (bta_info *)instance->pdata;
		bta_sys_windowhook(instance, NULL);
		bta_free(instance->pdata);
	}
}

char *__payment_postbuf=NULL;
void bta_api_do_payment_rcvd(NPP inst, const char *url, const char *resp) {
	bta_free(__payment_postbuf);

	if( strncmp(resp, "bta_token=", 10)!=0 ) {
		bta_sys_prompt(inst, resp);
	} else { // succeded
		bta_post_data(inst, ((bta_info *)inst->pdata)->posturl, resp, "_parent", NULL, false);
	}
}

void bta_api_do_payment(NPP inst) {
	bta_info *p = (bta_info *)inst->pdata;

	__payment_postbuf = (char *)bta_malloc(128+strlen(p->desc)+(BTA_ID_LENGTH*2));
	if( !__payment_postbuf ) return;
	sprintf(__payment_postbuf, "site=%19s&user=%19s&pin=%s&price=%3.2f&check=%32s&type=%s&description=%s",
			p->site, bta_user, p->pin, p->price, p->check, p->type==1?"subscription":"payment", p->desc);
	
	bta_post_data(inst, BTA_API_PAYMENT, __payment_postbuf, NULL, bta_api_do_payment_rcvd, false);
}
