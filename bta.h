// BetterThanAds NPAPI Plugin Header file

#if defined(XULRUNNER_SDK)
#include <npapi.h>
#include <npfunctions.h>
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

/////////////////////////////////////////////////
// bta_api.c
int  bta_api_init(NPNetscapeFuncs *npnf);
void *bta_api_thread(void *x);
void bta_api_close();
void bta_api_gotURL(NPP inst, const char *url, char *resp, void *notifyData);
void bta_api_count_site(NPP inst, const char *tag);
void bta_api_payment_instance(NPP inst, const char *site, float price, NPBool recurring, const char *posturl, const char *description, const char *check);
void bta_api_set_user(NPP inst, const char *user_token);
void bta_api_close_instance(NPP inst);

void bta_api_clicked(NPP inst);
void bta_api_got_pin(NPP inst, const char *pin);
void bta_api_error(NPP inst, const char *message);

void *bta_malloc(int size);
void  bta_free(void **ptr);

/////////////////////////////////////////////////
// bta_xwin.c, bta_osx.c, bta_win.c
int  bta_sys_init(BTA_SYS_WINDOW parentwin);
void bta_sys_start_apithread();
void bta_sys_stop_apithread();
void bta_sys_close();

void bta_sys_draw(NPP instance, NPWindow *npwin);
void bta_sys_prompt(NPP instance, char *message);
void bta_sys_error(NPP instance, char *message);

int  bta_sys_is_running();
int  bta_sys_wait_dataready();
void bta_sys_post_dataready();
void bta_sys_lock_dataload();
void bta_sys_unlock_dataload();
