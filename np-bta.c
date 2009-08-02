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

#include "bta.h"

///////////////////////////////////////////

static char url_resp[ BTA_BUFFER_SIZE ];
static NPNetscapeFuncs *npnfuncs = NULL;

void logmsg(const char *msg) {
//#ifdef DEBUG
#ifndef _WINDOWS
	fputs(msg, stderr);
	fflush(stderr);
#else
	FILE *out = fopen("\\np-bta.log", "a");
	fputs(msg, out);
	fclose(out);
#endif
//#endif
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
	logmsg("np-bta: NPP_WriteReady\n");
	return BTA_BUFFER_SIZE;
}

// take some data from the stream
int32 bta_write(NPP instance, NPStream* stream, int32 offset, int32 len, void* dbuf) {
	logmsg("np-bta: NPP_Write\n");
	// yup, it overwrites. none of the api calls should return very much anyway.
	if( len>=BTA_BUFFER_SIZE ) len=BTA_BUFFER_SIZE;
	memcpy(url_resp, ((char *)dbuf)+offset, len);
	url_resp[len]=0;
	return len;
}

// url notifications
void bta_urlNotify(NPP instance, const char* url,	NPReason reason, void* notifyData) {
	logmsg("np-bta: NPP_URLNotify\n");
	bta_api_gotURL(instance, url, url_resp, notifyData);
}

////////////////////////////////////////////////////////////

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

	for(i=0; i<argc; i++) {
			if( strcmp(argn[i], "user")==0 ) {
				site_token=argv[i];
				bta_api_set_user(instance, site_token);
				npnfuncs->reloadplugins(true);
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
				logmsg("In private mode, no tracking\n");
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

static NPError
destroy(NPP instance, NPSavedData **save) {
	logmsg("np-bta: destroy\n");
	bta_api_close_instance(instance);
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
	logmsg("np-bta: handleEvent\n");

#if defined(_WINDOWS)
	if( ((NPEvent *)ev)->event==WM_LBUTTONUP  )
#elif defined(WEBKIT_DARWIN_SDK)
	if( ((NPEvent *)ev)->what==2 ) // mouseUp
#else // X Windows
	if( ((XEvent*)ev)->type==ButtonRelease ) // dont really care which button...
#endif
	{
		bta_api_clicked(instance);
		return TRUE;
	}

	return FALSE;
}

static NPError /* expected by Opera */
setWindow(NPP instance, NPWindow* npwin) {
	logmsg("np-bta: setWindow\n");

	if( npwin!=NULL )	bta_sys_draw(instance, npwin);

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

	bta_api_init(npnfuncs);
	return NPERR_NO_ERROR;
}

NPError
OSCALL NP_Shutdown() {
	logmsg("np-bta: NP_Shutdown\n");
	bta_api_close();
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
