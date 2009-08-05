// BetterThanAds NPAPI Plugin Header file

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

#ifdef XP_UNIX

#include <X11/X.h>
#include <X11/Xlib.h>
#define BTA_SYS_WINDOW Window

#elif defined(_WINDOWS)

#define BTA_SYS_WINDOW HWND

#elif defined(WEBKIT_DARWIN_SDK)

#endif



#define BTA_BUFFER_SIZE 102400

#define BTA_ID_LENGTH 19
#define BTA_PAGEVIEWS 100

// TODO: MOVE TO https://
#define BTA_API_PAGEVIEWS    "http://api.betterthanads.com/pageviews/"
#define BTA_API_PAYMENT      "http://api.betterthanads.com/payment/"
//#define BTA_API_PAGEVIEWS    "http://127.0.0.1:18080/api/pageviews/"
//#define BTA_API_PAYMENT      "http://127.0.0.1:18080/api/payment/"

void logmsg(const char *str);

typedef struct _bta_info {
	int type;   // 1=subscription, 2=payment
	float price;
	char site[ BTA_ID_LENGTH+1 ];
	char check[33];
	char *posturl;
	char *desc;
	char pin[13];

	// embeded instance info
	int width, height;
#ifdef XP_UNIX
	Display *dpy;
	Colormap cmap;
	Window window;
#elif defined(_WINDOWS)
    HWND window;
#else

#endif

	char buf[2]; // container for posturl and desc
} bta_info;

/////////////////////////////////////////////////
// bta_api.c
int  bta_api_init(NPNetscapeFuncs *npnf);
void bta_api_shutdown();
void bta_api_window(NPP inst, NPWindow *npwin);
void bta_api_count_site(NPP inst, const char *tag);
void bta_api_payment_instance(NPP inst, const char *site, float price, NPBool recurring, const char *posturl, const char *description, const char *check);
void bta_api_set_user(NPP inst, const char *user_token);
void bta_api_close_instance(NPP inst);
 
void bta_api_do_payment(NPP inst);
 
void *bta_malloc(int size);
void  bta_free(void *ptr);
 
 /////////////////////////////////////////////////
 // bta_xwin.c, bta_osx.c, bta_win.c
int  bta_sys_init();
void bta_sys_close();

void bta_sys_windowhook(NPP instance, NPWindow *npwin);
void bta_sys_prompt(NPP instance, const char *error);
