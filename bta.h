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
// BetterThanAds NPAPI Plugin Header file
//

#if defined(XULRUNNER_SDK)
 #include <npapi.h>
 #include <nptypes.h>
#ifdef _WINDOWS
 #include <npupp.h>
#else
 #include <npfunctions.h>
#endif
 #include <npruntime.h>
#elif defined(WEBKIT_DARWIN_SDK)
 #include <Webkit/npapi.h>
 #include <WebKit/npfunctions.h>
 #include <WebKit/npruntime.h>
 #define OSCALL
#endif


#ifdef XP_UNIX

 #include <X11/X.h>
 #include <X11/Xlib.h>

 #define BTA_SYS_WINDOW Window

#elif defined(_WINDOWS)
typedef __int8            int8_t;
typedef __int16           int16_t;
typedef __int32           int32_t;
typedef __int64           int64_t;
typedef unsigned __int8   uint8_t;
typedef unsigned __int16  uint16_t;
typedef unsigned __int32  uint32_t;
typedef unsigned __int64  uint64_t;

 #define BTA_SYS_WINDOW HWND

#elif defined(WEBKIT_DARWIN_SDK)

 // TODO: mac code...

#endif


// bigints can be 19 digits at most
#define BTA_ID_LENGTH 19

// track up to 100 pageviews
#define BTA_PAGEVIEWS 100

// fixed buffer size
#define BTA_BUFFER_SIZE 102400

// TODO: MOVE TO https://
#define BTA_API_PAGEVIEWS    "http://api.betterthanads.com/pageviews/"
#define BTA_API_PAYMENT      "http://api.betterthanads.com/payment/"

void logmsg(const char *str);

typedef struct _bta_info {
	int type;   // 1=subscription, 2=payment
	float price;
	char site[ BTA_ID_LENGTH+1 ];
	char check[33];
	char *posturl;
	char *desc;
	char pin[13];

	// embedded instance info
	int width, height;
#ifdef XP_UNIX
	Display *dpy;
	Colormap cmap;
	Window window;
#elif defined(_WINDOWS)
  HWND window;
#elif defined(WEBKIT_DARWIN_SDK)
 // TODO: mac code...

#endif

	char buf[2]; // container for posturl and desc
} bta_info;

/////////////////////////////////////////////////
// npbetter.c

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
// bta_xwin.c, bta_osx.c, bta_mswin.c
int  bta_sys_init();
void bta_sys_close();

void bta_sys_windowhook(NPP instance, NPWindow *npwin);
void bta_sys_prompt(NPP instance, const char *error);
