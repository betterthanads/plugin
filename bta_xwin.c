// X Windows prompt dialog
// and posix threads

#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/xpm.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <semaphore.h>

#include "bta.h"

////////////////////////////

#define DIALOG_WIDTH 444
#define DIALOG_BANNER_HEIGHT 100
#define DIALOG_HEIGHT 300

#define OVER_OK     1
#define OVER_CANCEL 2
#define OVER_PINBOX 3
#define OVER_OTHER  4

////////////////////////////

#include "dialog.xpm"
#define XFONT_STR "-*-courier-medium-r-*-*-18-*-*-*-*-*-*-*"

struct _btap {
	pthread_t pt;
	XFontStruct *xfont;
	Display *dpy;
	Window win;
	GC gc;
	XColor clr[5];
	Pixmap banner;
	Cursor cursor[2];
	char pin[256];
	char *message;
	NPP instance;
} btap;

// cancel, ok buttons
const char *buttontext[2] = { "Cancel", "OK" };
XRectangle buttons[2] = { {10,DIALOG_HEIGHT-35, 150, 25}, {DIALOG_WIDTH-160, DIALOG_HEIGHT-35, 150, 25} };
XRectangle pinbox = {260,67, 394-260,86-67};

// TODO: should probably be a semaphore
int prompt_running=0;

//////////////////
// helper funcs

void centeredtext(int cx, int cy, const char *str) {
	int len=strlen(str);
  int x=cx,y=cy;

	y -= (btap.xfont->ascent+btap.xfont->descent)/2;
	y += btap.xfont->ascent+1;
	x -= XTextWidth(btap.xfont, str, len)/2;

	XDrawString(btap.dpy, btap.win, btap.gc, x,y, str, len);
}

void descriptiontext(int tx, int ty, int wx, const char *str) {
	int len=strlen(str);
	int start=0;
	int end=len, endline=0;
	int y=ty, h = btap.xfont->ascent+btap.xfont->descent;
	int w = XTextWidth(btap.xfont, str+start, end-start);

	y -= h/2;
	y += btap.xfont->ascent+1;

	while( start<len ) {
		endline=start;
		while( str[endline]!='\n' && endline<end ) endline++;
		if( str[endline]=='\n' ) end=endline-1;
		else end=len;
		w = XTextWidth(btap.xfont, str+start, end-start);

		while( w>=wx ) {
			while( str[end]!=' ' && start<end ) end--;
			w = XTextWidth(btap.xfont, str+start, end-start);
			end--;
		}
		end++;
		if(end>len) end=len;
		XDrawString(btap.dpy, btap.win, btap.gc, tx, y, str+start, end-start);
		start=end+1;
		end=len;
		w = XTextWidth(btap.xfont, str+start, end-start);

		y += h;
	}
}

/////////////////////////////////////////////////////////////////////////////////////

int bta_sys_init(BTA_SYS_WINDOW pwin) {
	int screen, sw, sh;
	Window root;
	Colormap cmap;
	XSetWindowAttributes wa;
 
	btap.dpy = XOpenDisplay(NULL);
	if( btap.dpy==NULL ) {
		fprintf(stderr, "unable to open display!");
		return 1;
	}
	screen = DefaultScreen(btap.dpy);
	root = RootWindow(btap.dpy, screen);
	sw = DisplayWidth(btap.dpy, screen);
	sh = DisplayHeight(btap.dpy, screen);
	btap.cursor[0] = XCreateFontCursor(btap.dpy, XC_left_ptr);
	btap.cursor[1] = XCreateFontCursor(btap.dpy, XC_xterm);
	cmap = DefaultColormap(btap.dpy, screen);
	XAllocNamedColor(btap.dpy, cmap, "#000055", &btap.clr[0], &btap.clr[0]);
	XAllocNamedColor(btap.dpy, cmap, "#555599", &btap.clr[1], &btap.clr[1]);
	XAllocNamedColor(btap.dpy, cmap, "#ddddff", &btap.clr[2], &btap.clr[2]);
	XAllocNamedColor(btap.dpy, cmap, "#ffffff", &btap.clr[3], &btap.clr[3]);

	btap.win = XCreateSimpleWindow(btap.dpy, root, (sw/2)-(DIALOG_WIDTH/2), (sh/2)-(DIALOG_HEIGHT/2), DIALOG_WIDTH, DIALOG_HEIGHT, 0, 0, btap.clr[3].pixel);
	btap.gc = XCreateGC(btap.dpy, btap.win, 0, NULL);

	XStoreName(btap.dpy, btap.win, "BetterThanAds - Confirm payment");
	XSetTransientForHint(btap.dpy, btap.win, pwin);
	XDefineCursor(btap.dpy, btap.win, btap.cursor[0]);

	// grab KeyPress, ButtonRelease, PointerMotion, ExposureMask
	wa.event_mask= KeyPressMask|ButtonPressMask|ButtonReleaseMask|PointerMotionMask|ExposureMask;
	XChangeWindowAttributes(btap.dpy, btap.win, CWEventMask, &wa);

	// banner image
	if( XpmCreatePixmapFromData(btap.dpy, btap.win, (char **)dialog_xpm, &btap.banner, NULL, NULL) ) {
		fprintf(stderr, "pixmap load failed!");
		return 1;
	}

	if( !(btap.xfont = XLoadQueryFont(btap.dpy, XFONT_STR)) && !(btap.xfont = XLoadQueryFont(btap.dpy, "fixed"))) {
		fprintf(stderr, "no fonts!");
		return 1;
	}
	XSetFont(btap.dpy, btap.gc, btap.xfont->fid);

	return 0;
}

void *bta_sys_thread(void *m) {
	const char *message =(const char *)m;
	XEvent ev;
	int pinlen=0;
	int done=0, redraw=1, unmapped=0;
	int z=0;
	int over=OVER_OTHER;

	fprintf(stderr, "win opened\n");
	XMapRaised(btap.dpy, btap.win);
	/////////////
	// get pin/button
	btap.pin[0]=0;
	while( !done && !unmapped ) {
		if( XNextEvent(btap.dpy, &ev) )
			break;

		if( ev.type==Expose ) {
			redraw=1;
		} else if( ev.type==UnmapNotify ) {
			unmapped=1;
		} else if( ev.type==MotionNotify ) {
			int lastover=over;
			int x=ev.xmotion.x, y=ev.xmotion.y;
			// tl=260,67  br=394,86 
			if( x>=260 && x<=394 && y>=67 && y<=86 ) {
				if( over!=OVER_PINBOX ) {
					over=OVER_PINBOX;
					XDefineCursor(btap.dpy, btap.win, btap.cursor[1]);
				}
			} else {
				if( over==OVER_PINBOX )
					XDefineCursor(btap.dpy, btap.win, btap.cursor[0]);
				over=OVER_OTHER;
			}

			if( y>=buttons[0].y && y<=buttons[0].y+buttons[0].height ) {
				if( x>=buttons[0].x && x<=buttons[0].x+buttons[0].width ) {
					over=OVER_CANCEL;
				} else if( x>=buttons[1].x && x<=buttons[1].x+buttons[1].width ) {
					over=OVER_OK;
				}
			}
			if( over!=lastover ) redraw=1;
		
		} else if( ev.type==ButtonRelease ) {
			switch( over) {
				case OVER_OK: done=1; break;
				case OVER_CANCEL: done=-1; break;
			}
		} else if( ev.type==KeyPress) {
			char key[5];
			XLookupString(&ev.xkey, key, 5, NULL,NULL);
			switch( key[0] ) {
				case 27: done=-1; break;
				case '\b': if( pinlen>0 ) btap.pin[ --pinlen ]=0; break;
				case '\r': done=1; break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':  if(pinlen==13) break; 
									 btap.pin[pinlen++]=key[0];
									 btap.pin[pinlen]=0;
									 break;
				default:
									 printf("got key '%s'\n", key);
			}

			redraw=1;
		}

		// can't be done if no pin
		if( done==1 && btap.pin[0]==0 ) done=0;

		if( done ) 
			XUnmapWindow(btap.dpy, btap.win);

		else if( redraw ) {
			// draw banner
			XCopyArea(btap.dpy, btap.banner, btap.win, btap.gc, 0,0, DIALOG_WIDTH, DIALOG_BANNER_HEIGHT, 0,0);

			// draw pinbox
			XSetForeground(btap.dpy, btap.gc, over==OVER_PINBOX?btap.clr[2].pixel:btap.clr[3].pixel);
			XFillRectangles(btap.dpy, btap.win, btap.gc, &pinbox, 1);

			XSetForeground(btap.dpy, btap.gc, btap.clr[0].pixel);
			XRectangle dot= {262,74, 5,5};
			for(z=0; z<=pinlen; z++) {
				dot.x=262+z*10;
				if( z!=pinlen )
					XFillRectangles(btap.dpy, btap.win, btap.gc, &dot, 1);
				else // cursor
					XDrawLine(btap.dpy, btap.win, btap.gc, dot.x, 71, dot.x, 83);
			}

			// draw description text
			XSetForeground(btap.dpy, btap.gc, btap.clr[0].pixel);
			descriptiontext(5, 110, 434, message);

			// draw buttons
			XSetForeground(btap.dpy, btap.gc, over==OVER_CANCEL?btap.clr[1].pixel:btap.clr[0].pixel);
			XFillRectangles(btap.dpy, btap.win, btap.gc, buttons, 1);

			XSetForeground(btap.dpy, btap.gc, over==OVER_OK?btap.clr[1].pixel:btap.clr[0].pixel);
			XFillRectangles(btap.dpy, btap.win, btap.gc, buttons+1, 1);

			// draw text on buttons
			XSetForeground(btap.dpy, btap.gc, btap.clr[3].pixel);

			centeredtext(buttons[1].x+buttons[1].width/2, buttons[1].y+buttons[1].height/2, "OK");
			centeredtext(buttons[0].x+buttons[0].width/2, buttons[0].y+buttons[0].height/2, "Cancel");
			
			redraw=0;
		}
	}
	XFlush(btap.dpy);

	if( done==1 ) {
		bta_api_got_pin(btap.instance, btap.pin);
	} else {
		bta_api_got_pin(btap.instance, "x");
	}

	fprintf(stderr, "win closed\n");
	prompt_running=0;
}

void bta_sys_close() {
	if( prompt_running )
		pthread_cancel(btap.pt);

	XFreePixmap(btap.dpy, btap.banner);
	XFreeCursor(btap.dpy, btap.cursor[1]);
	XFreeCursor(btap.dpy, btap.cursor[0]);
	XFreeGC(btap.dpy, btap.gc);
	XDestroyWindow(btap.dpy, btap.win);
	XSync(btap.dpy, True);
	XCloseDisplay(btap.dpy);
}

void bta_sys_prompt(NPP instance, char *message) {
	fprintf(stderr, "trying to open win\n");
	if( prompt_running ) return;
	prompt_running=1;
	btap.instance=instance;
	pthread_create(&btap.pt, NULL, bta_sys_thread, message);
}

/////////////////////////////////////////////////////////////////////////////////////

pthread_t bta_sys_pt;
sem_t bta_sys_running, bta_sys_dataready, bta_sys_dataload;

void bta_sys_start_apithread() {
	sem_init(&bta_sys_running, 0, 1);
	sem_init(&bta_sys_dataready, 0, 0);
	sem_init(&bta_sys_dataload, 0, 1);
	pthread_create(&bta_sys_pt, NULL, bta_api_thread, NULL);
}
void bta_sys_stop_apithread() {
	sem_wait(&bta_sys_running);
	sem_post(&bta_sys_dataready);
	pthread_join(bta_sys_pt,NULL);
	sem_destroy(&bta_sys_dataload);
	sem_destroy(&bta_sys_dataready);
	sem_destroy(&bta_sys_running);
}

int  bta_sys_wait_dataready() {
	sem_wait(&bta_sys_dataready);
}

void bta_sys_post_dataready() {
	sem_post(&bta_sys_dataready);
}

int  bta_sys_is_running() {
	int val=0;
	sem_getvalue(&bta_sys_running, &val);
	return val;
}

void bta_sys_lock_dataload() {
	sem_wait(&bta_sys_dataload);
}
void bta_sys_unlock_dataload() {
	sem_post(&bta_sys_dataload);
}
