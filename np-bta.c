#include <stdio.h>
#include <string.h>

#if defined(_WINDOWS)
  //typedef int bool;
  //#define true 1
  //#define false 0
#elif defined(WEBKIT_DARWIN_SDK)

#else
	// need XEvent and ButtonRelease
  #include <X11/X.h>
  #include <X11/Xlib.h>
#endif

#if defined(XULRUNNER_SDK)
#include <npapi.h>
#include <npupp.h>
#include <npruntime.h>
#elif defined(WEBKIT_DARWIN_SDK)
#include <Webkit/npapi.h>
#include <WebKit/npfunctions.h>
#include <WebKit/npruntime.h>
#define OSCALL
#elif defined(WEBKIT_WINMOBILE_SDK) /* WebKit SDK on Windows */
#ifndef PLATFORM
#define PLATFORM(x) defined(x)
#endif
#include <npfunctions.h>
#ifndef OSCALL
#define OSCALL WINAPI
#endif
#endif

#include "bta.h"

///////////////////////////////////////////

typedef struct _bta_info {
	NPP instance;
	int type;   // 1=subscription, 2=payment
	char *site;
	char *check;
	char *posturl;
	char *desc;
	float price;

	struct _bta_info *next;
} bta_info;

bta_info *current_clickables = NULL;
bta_info *current_prompt = NULL;
char *buf=NULL;
char current_pin[32];
bool bta_ready=false;
NPP a_inst=0;

static NPNetscapeFuncs *npnfuncs = NULL;
static char url_resp[ BTA_BUFFER_SIZE ];

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

// posts the urlencoded-string data to url asynchronously
void _bta_post_data(NPP inst, const char *url, const char *data, const char *target) {
	static char bta_post[ BTA_BUFFER_SIZE ];
	int len = data?strlen(data):0;
	bta_post[0]=0;
	if( len>(BTA_BUFFER_SIZE-80) ) 
		logmsg("post data too large\n");
	else if( data )
		sprintf(bta_post, "Content-Type: application/x-www-form-urlencoded\nContent-Length: %d\n\n%s", len, data);

	if( !url ) return;
	fprintf(stderr, "URL: %s\nPOST: %s\n", url, bta_post);

	npnfuncs->posturlnotify(
			inst, // instance
			url,
			target, // NULL=send response back to plugin
			strlen(bta_post),    // length of data to send
		  bta_post,  // data to send
			false,  // bta_post is a buffer, not a file
			NULL
		);
}

void bta_post_data(NPP inst, const char *url, const char *data) {
  _bta_post_data(inst, url, data, NULL);
}

// new stream started
static NPError bta_newStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype) {
	logmsg("np-bta: NPP_NewStream\n");

	return NPERR_NO_ERROR;
}

// closing stream
static NPError bta_destroyStream(NPP instance, NPStream* stream, NPReason reason) {
	logmsg("np-bta: NPP_DestroyStream\n");

	logmsg("\n---- SOF ----\n");
  logmsg(url_resp);
	logmsg("\n---- EOF ----\n");
	return NPERR_NO_ERROR;
}

// how much data can we take?
static int32 bta_writeReady(NPP instance, NPStream* stream) {
	return BTA_BUFFER_SIZE;
}

// take some data from the stream
int32 bta_write(NPP instance, NPStream* stream, int32 offset, int32 len, void* dbuf) {
	// yup, it overwrites. none of the api calls should return very much anyway.
	if( len>=BTA_BUFFER_SIZE ) len=BTA_BUFFER_SIZE;
	memcpy(url_resp, ((char *)dbuf)+offset, len);
	url_resp[len]=0;
	return len;
}

// url notifications
void bta_urlNotify(NPP instance, const char* url,	NPReason reason, void* notifyData) {
	logmsg("np-bta: NPP_URLNotify\n");
	bta_gotURL(instance, url, url_resp);
}

void *bta_malloc(int size) {
  return npnfuncs->memalloc(size);
}
void bta_free(void *ptr) {
  npnfuncs->memfree(ptr);
}

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////
static char prompt_result[1024];
const char *bta_js_prompt(NPP instance, const char *message) {
	const char *ptr=NULL;
	char *ret=NULL;
	NPObject *win=NULL;
	NPVariant args[1];
	NPVariant result;
	NPIdentifier p = npnfuncs->getstringidentifier("prompt");

	// get window
  if( npnfuncs->getvalue(instance, NPNVWindowNPObject, &win) != NPERR_NO_ERROR )
		return NULL;
	
	// STRINGZ_TO_NPVARIANT(message, args[0]);
	args[0].type = NPVariantType_String;
	args[0].value.stringValue.utf8length = strlen(message);
	args[0].value.stringValue.utf8characters = message;

	// popup dialog prompt
	if( !npnfuncs->invoke(instance, win, p, args, 1, &result) ) {
		fprintf(stderr,"NPN_Invoke on window.prompt() failed...\n");
		npnfuncs->releaseobject(win);
		return NULL;
	}

	//copy pin out
	if( NPVARIANT_IS_STRING(result) ) {
		ptr = NPVARIANT_TO_STRING(result).utf8characters;
		strncpy(prompt_result, ptr, 1024);
		prompt_result[1023]=0; // make sure its null terminated
		ret=prompt_result;
	} else if( NPVARIANT_IS_INT32(result) ) {
		// cant possibly overflow 1024 bytes...
		sprintf(prompt_result, "%d", NPVARIANT_TO_INT32(result));
		ret=prompt_result;
	}
	npnfuncs->releasevariantvalue(&result);

	// release window
	npnfuncs->releaseobject(win);

	return ret;
}

///////////////////////

void _bta_prompt_gotpin_async(const char *pin) {
	if( pin!=NULL )
		bta_do_payment(current_prompt->instance, current_prompt->site, pin, current_prompt->price, current_prompt->type==1, current_prompt->posturl, current_prompt->desc, current_prompt->check);
	current_prompt=NULL;
}
void _bta_prompt_gotpin(const char *pin) {
	fprintf(stderr, "xxx\n");
	fprintf(stderr, "got pin: '%s'\n", pin);
  _bta_prompt_gotpin_async(pin);
	// npnfuncs->pluginthreadasynccall(current_prompt->instance, bta_prompt_gotpin_async, pin);
	fprintf(stderr, "yyy\n");
}

void bta_prompt_getpin(void *x) {
	const char *pin = bta_prompt_result;
	fprintf(stderr, "got pin: '%s'\n", pin);
	if( pin!=NULL )
		bta_do_payment(current_prompt->instance, current_prompt->site, pin, current_prompt->price, current_prompt->type==1, current_prompt->posturl, current_prompt->desc, current_prompt->check);
	current_prompt=NULL;
}
void bta_prompt_gotpin(const char *pin) {
	//npnfuncs->pluginthreadasynccall(current_prompt->instance, bta_prompt_getpin, NULL);
	bta_prompt_getpin(NULL);
}
void bta_prompt_error() {
	fprintf(stderr, "payment error\n");
}

////////////////////////

/* NPP */

static NPError
nevv(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char *argn[], char *argv[], NPSavedData *saved) {
	const char *emptystr="";
	const char *domain=NULL;
	const char *site_token=NULL;
	const char *desc=emptystr;
	const char *posturl=NULL;
	const char *check=NULL;
	float price=0.0;
	int i=0, action=0;
	logmsg("np-bta: new\n");

	// make windowless (which defaults to transparent too)
	npnfuncs->setvalue(instance, NPPVpluginWindowBool, 0);
	
	for(i=0; i<argc; i++) {
		//fprintf(stderr, "np-bta:   arg[%d]: '%s' => '%s'\n", i,argn[i],argv[i]);
	  if( !bta_ready ) {	
			if( strcmp(argn[i], "user")==0 ) {
				site_token=argv[i];
				bta_set_user(instance, site_token);
				a_inst=instance;
				npnfuncs->reloadplugins(true);
				return NPERR_NO_ERROR;
			}
		} else {
			if( strcmp(argn[i], "user")==0 )
				return NPERR_NO_ERROR;

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

	}
	if( !bta_ready ) {
		if( a_inst==0 ) {
			npnfuncs->geturl(instance, "http://betterthanads.com/107e4535fb8c1f1ac38be682088dd441/plugin/activate", "_blank");
			a_inst=instance;
		}
		return NPERR_NO_ERROR;
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
		
		bta_count_site(instance, site_token);

	} else if( (action==1 || action==2) && price>0.0 && posturl!=NULL && check!=NULL ) { 
		// put it all in one block
		int sz = sizeof(bta_info)+strlen(check)+strlen(posturl)+strlen(desc)+128;
		char *ptr = (char *)bta_malloc( sz );
		bta_info *nbta = (bta_info *)ptr;
		ptr+=sizeof(bta_info);

		nbta->instance=instance;
		nbta->type=action;
		nbta->price=price;
		nbta->site=ptr;
		  strcpy(nbta->site, site_token);
			ptr+=1+strlen(site_token);
		nbta->check=ptr;
		  strcpy(nbta->check, check);
			ptr+=1+strlen(check);
		nbta->posturl=ptr;
		  strcpy(nbta->posturl, posturl);
			ptr+=1+strlen(posturl);
		nbta->desc=ptr;
		  strcpy(nbta->desc, desc);
			ptr+=1+strlen(desc);

		nbta->next=current_clickables;
		current_clickables=nbta;

	} else {
		return NPERR_GENERIC_ERROR;
	}
  
	return NPERR_NO_ERROR;
}

static NPError
destroy(NPP instance, NPSavedData **save) {
	bta_info *prv = (bta_info *)NULL;
	bta_info *cur = current_clickables;
	logmsg("np-bta: destroy\n");
		
	while( cur!=NULL ) {
		if( cur->instance==instance ) {
			if( prv==NULL ) current_clickables=cur->next;
			else prv->next=cur->next;

			bta_free(cur);
			return NPERR_NO_ERROR;
		}
		prv=cur;
		cur=cur->next;
	}

	if( buf ) bta_free(buf);
	return NPERR_NO_ERROR;
}

static NPError
getValue(NPP instance, NPPVariable variable, void *value) {
	switch(variable) {
	default:
		logmsg("np-bta: getvalue - default\n");
		return NPERR_GENERIC_ERROR;
	case NPPVpluginNameString:
		logmsg("np-bta: getvalue - name string\n");
		*((char **)value) = "BetterThanAdsPlugin";
		break;
	case NPPVpluginDescriptionString:
		logmsg("np-bta: getvalue - description string\n");
		*((char **)value) = "<a href=\"http://betterthanads.com/\">BetterThanAds.com</a> site-supporting and micropayment plugin.";
		break;
	case NPPVpluginWindowBool:
		logmsg("np-bta: getvalue - windowed\n");
		*((PRBool *)value) = PR_TRUE;
		break;
#if defined(XULRUNNER_SDK)
	case NPPVpluginNeedsXEmbed:
		logmsg("np-bta: getvalue - xembed\n");
		*((PRBool *)value) = PR_FALSE;
		break;
#endif
	}

	return NPERR_NO_ERROR;
}

static NPError /* expected by Safari on Darwin */
handleEvent(NPP instance, void *ev) {
	int sz=0;
	//logmsg("np-bta: handleEvent\n");

#if defined(_WINDOWS)
	if( ((NPEvent *)ev)->event==WM_LBUTTONUP  )
#elif defined(WEBKIT_DARWIN_SDK)
	if( ((NPEvent *)ev)->what==2 ) // mouseUp
#else // X Windows
	Window ff_win;

	if( ((XEvent*)ev)->type==ButtonRelease ) // dont really care which button...
#endif
	{
		bta_info *cur = current_clickables;
		while( cur!=NULL ) {
			if( cur->instance==instance ) break;
			cur=cur->next;
		}
		if( cur==NULL )
			return NPERR_NO_ERROR;
		if( current_prompt!=NULL )
			return NPERR_GENERIC_ERROR;

		sz = 250+strlen(cur->desc);
		if( buf ) bta_free(buf);
		buf = (char *)bta_malloc(sz);
		if( !buf ) return NPERR_GENERIC_ERROR;
		
		sprintf(buf, "I will pay $%3.2f%s for the following:\n\n%s",
				    cur->price, cur->type==1?"/mo":"", cur->desc);

		npnfuncs->getvalue(instance, NPNVnetscapeWindow, &ff_win);

		current_prompt = cur;
		if( bta_prompt(buf, &ff_win)!=0 ) {
			current_prompt=NULL;
		}
		return 1;
	}

	return NPERR_NO_ERROR;
}

static NPError /* expected by Opera */
setWindow(NPP instance, NPWindow* pNPWindow) {
	logmsg("np-bta: setWindow\n");
	return NPERR_NO_ERROR;
}

////////////////////////////////////////////////////////////////

/* EXPORT */
#ifdef __cplusplus
extern "C" {
#endif

NPError OSCALL
NP_GetEntryPoints(NPPluginFuncs *nppfuncs) {
	logmsg("np-bta: NP_GetEntryPoints\n");
	nppfuncs->version       = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
	nppfuncs->newp          = nevv;
	nppfuncs->destroy       = destroy;
	nppfuncs->getvalue      = getValue;
	nppfuncs->event         = handleEvent;
	nppfuncs->setwindow     = setWindow;

	nppfuncs->urlnotify     = bta_urlNotify;
	nppfuncs->newstream     = bta_newStream;
	nppfuncs->destroystream = bta_destroyStream;
	nppfuncs->writeready    = bta_writeReady;
	nppfuncs->write         = bta_write;

	return NPERR_NO_ERROR;
}

#ifndef HIBYTE
#define HIBYTE(x) ((((uint32)(x)) & 0xff00) >> 8)
#endif

NPError OSCALL
NP_Initialize(NPNetscapeFuncs *npnf
#if !defined(_WINDOWS) && !defined(WEBKIT_DARWIN_SDK)
			, NPPluginFuncs *nppfuncs)
#else
			)
#endif
{
	logmsg("np-bta: NP_Initialize\n");
	if(npnf == NULL)
		return NPERR_INVALID_FUNCTABLE_ERROR;

	if(HIBYTE(npnf->version) > NP_VERSION_MAJOR)
		return NPERR_INCOMPATIBLE_VERSION_ERROR;
  
	npnfuncs = npnf;
#if !defined(_WINDOWS) && !defined(WEBKIT_DARWIN_SDK)
	NP_GetEntryPoints(nppfuncs);
#endif

	bta_ready = bta_init();
	return NPERR_NO_ERROR;
}

NPError
OSCALL NP_Shutdown() {
	logmsg("np-bta: NP_Shutdown\n");
	if(bta_ready) bta_close();
	return NPERR_NO_ERROR;
}

char *
NP_GetMIMEDescription(void) {
	logmsg("np-bta: NP_GetMIMEDescription\n");
	return "application/x-vnd-betterthanads-bta:.bta:BetterThanAds.com site-support tracking, micropayments and microsubscriptions";
}

NPError OSCALL /* needs to be present for WebKit based browsers */
NP_GetValue(void *npp, NPPVariable variable, void *value) {
	return getValue((NPP)npp, variable, value);
}

#ifdef __cplusplus
}
#endif
