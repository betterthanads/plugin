#include <stdio.h>
#include <string.h>

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

#if defined(_WINDOWS)
  typedef bool int;
  #define true 1
  #define false 0
#elif defined(WEBKIT_DARWIN_SDK)

#else
	// need XEvent and ButtonRelease
  #include <X11/X.h>
  #include <X11/Xlib.h>
#endif

#define BUFFER_SIZE 102400
static NPNetscapeFuncs *npnfuncs = NULL;
static char url_resp[102400];

// defined in bta_api.c
void bta_init(NPNetscapeFuncs *npn);
void bta_gotURL(NPP inst, const char *url, char *resp);
void bta_count_site(NPP inst, const char *tag);
void bta_do_payment(NPP inst, const char *site, const char *pin, float price, bool recurring, const char *posturl);

typedef struct _bta_info {
	NPP instance;
	int type;   // 1=subscription, 2=payment
	char *site;
	char *posturl;
	char *desc;
	float price;

	struct _bta_info *next;
} bta_info;

bta_info *current_clickables = NULL;

/* NPN */

static void logmsg(const char *msg) {
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
	return BUFFER_SIZE;
}

// take some data from the stream
int32 bta_write(NPP instance, NPStream* stream, int32 offset, int32 len, void* buf) {
	// yup, it overwrites. none of the api calls should return very much anyway.
	if( len>=BUFFER_SIZE ) len=BUFFER_SIZE;
	memcpy(url_resp, ((char *)buf)+offset, len);
	url_resp[len]=0;
	return len;
}

// url notifications
void bta_urlNotify(NPP instance, const char* url,	NPReason reason, void* notifyData) {
	bta_gotURL(instance, url, url_resp);
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
	
	STRINGZ_TO_NPVARIANT(message, args[0]);

	// popup dialog prompt
	if( !npnfuncs->invoke(instance, win, p, args, 1, &result) ) {
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

/* NPP */

static NPError
nevv(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char *argn[], char *argv[], NPSavedData *saved) {
	const char *emptystr="";
	const char *site_token=NULL;
	const char *desc=emptystr;
	float price=0.0;
	const char *posturl=NULL;
	int i=0, action=0;
	logmsg("np-bta: new\n");

	// make windowless (which defaults to transparent too)
	npnfuncs->setvalue(instance, NPPVpluginWindowBool, 0);

	for(i=0; i<argc; i++) {
		//fprintf(stderr, "np-bta:   arg[%d]: '%s' => '%s'\n", i,argn[i],argv[i]);
		
		if( strcmp(argn[i], "site")==0 )
			site_token=argv[i];
		if( strcmp(argn[i], "price")==0 )
			price=atof( argv[i] );
		if( strcmp(argn[i], "posturl")==0 )
			posturl=argv[i];
		if( strcmp(argn[i], "description")==0 )
			desc=argv[i];

		if( strcmp(argn[i], "ptype")==0 ) {
			if( strcmp(argv[i], "subscription")==0 ) action=1;
			if( strcmp(argv[i], "payment")==0 ) action=2;
		}

	}

	if( action==0 ) { 
		bta_count_site(instance, site_token);

	} else if( (action==1 || action==2) && price>0.0 && posturl!=NULL ) { 
		// put it all in one block
		int sz = sizeof(bta_info)+strlen(posturl)+strlen(desc)+128;
		char *ptr = npnfuncs->memalloc( sz );
		bta_info *nbta = (bta_info *)ptr;
		ptr+=sizeof(bta_info);

		nbta->instance=instance;
		nbta->type=action;
		nbta->price=price;
		nbta->site=ptr;
		  strcpy(nbta->site, site_token);
			ptr+=1+strlen(site_token);
		nbta->posturl=ptr;
		  strcpy(nbta->posturl, posturl);
			ptr+=1+strlen(posturl);
		nbta->desc=ptr;
		  strcpy(nbta->desc, desc);
			ptr+=1+strlen(desc);

		nbta->next=current_clickables;
		current_clickables=nbta;

	}
  
	return NPERR_NO_ERROR;
}

static NPError
destroy(NPP instance, NPSavedData **save) {
	logmsg("np-bta: destroy\n");
		
	bta_info *prv = NULL;
	bta_info *cur = current_clickables;
	while( cur!=NULL ) {
		if( cur->instance==instance ) {
			if( prv==NULL ) current_clickables=cur->next;
			else prv->next=cur->next;

			npnfuncs->memfree(cur);
			return NPERR_NO_ERROR;
		}
		prv=cur;
		cur=cur->next;
	}
	
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
		*((char **)value) = "<a href=\"http://www.betterthanads.com/\">BetterThanAds.com</a> site-supporting and micropayment plugin.";
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
	const char *pin=NULL;
	char *buf=NULL;
	int sz=0;
	logmsg("np-bta: handleEvent\n");

#if defined(_WINDOWS)
	if( ((NPEvent *)ev)->event==WM_LBUTTONUP  )
#elif defined(WEBKIT_DARWIN_SDK)
	if( ((NPEvent *)ev)->what==2 ) // mouseUp
#else
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

		sz = 250+strlen(cur->desc);
		buf = npnfuncs->memalloc(sz);
		if( !buf ) return NPERR_GENERIC_ERROR;
		
		sprintf(buf, "Please provide your BetterThanAds PIN\nto confirm your payment of $%3.2f%s for the following:\n   %s\n",
				    cur->price, cur->type==1?"/mo":"", cur->desc);
		
		// add description, type, and price
		pin = bta_js_prompt(instance, buf);
		npnfuncs->memfree(buf);
		if( pin )
			bta_do_payment(instance, cur->site, pin, cur->price, cur->type==1, cur->posturl);

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

	bta_init(npnfuncs);
	return NPERR_NO_ERROR;
}

NPError
OSCALL NP_Shutdown() {
	logmsg("np-bta: NP_Shutdown\n");
	return NPERR_NO_ERROR;
}

char *
NP_GetMIMEDescription(void) {
	logmsg("np-bta: NP_GetMIMEDescription\n");
	return "application/x-vnd-betterthanads-bta:.bta:BetterThanAds.com site-support, micropayments and microsubscriptions";
}

NPError OSCALL /* needs to be present for WebKit based browsers */
NP_GetValue(void *npp, NPPVariable variable, void *value) {
	return getValue((NPP)npp, variable, value);
}

#ifdef __cplusplus
}
#endif
