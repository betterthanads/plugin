// X Windows prompt dialog
// and posix threads

#include <X11/cursorfont.h>
#include <X11/Intrinsic.h>
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
#define XFONT_BUTTON_STR "-*-helvetica-*-r-*-*-12-*-*-*-*-*-*-*"

struct _bta_sys {
	Display*      display;
	Colormap      colormap;
	Window browser_win;

	pthread_t pt;
	XFontStruct *xfont;
	Window win;
	GC gc;
	Atom wm_delete;
	XColor clr[5];
	Pixmap banner;
	Cursor cursor[2];

	char pin[256];
	char *message;
	NPP instance;
} bta_sys;

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

	y -= (bta_sys.xfont->ascent+bta_sys.xfont->descent)/2;
	y += bta_sys.xfont->ascent+1;
	x -= XTextWidth(bta_sys.xfont, str, len)/2;

	XDrawString(bta_sys.display, bta_sys.win, bta_sys.gc, x,y, str, len);
}

void descriptiontext(int tx, int ty, int wx, const char *str) {
	int len=strlen(str);
	int start=0;
	int end=len, endline=0;
	int y=ty, h = bta_sys.xfont->ascent+bta_sys.xfont->descent;
	int w = XTextWidth(bta_sys.xfont, str+start, end-start);

	y -= h/2;
	y += bta_sys.xfont->ascent+1;

	while( start<len ) {
		endline=start;
		while( str[endline]!='\n' && endline<end ) endline++;
		if( str[endline]=='\n' ) end=endline-1;
		else end=len;
		w = XTextWidth(bta_sys.xfont, str+start, end-start);

		while( w>=wx ) {
			while( str[end]!=' ' && start<end ) end--;
			w = XTextWidth(bta_sys.xfont, str+start, end-start);
			end--;
		}
		end++;
		if(end>len) end=len;
		XDrawString(bta_sys.display, bta_sys.win, bta_sys.gc, tx, y, str+start, end-start);
		start=end+1;
		end=len;
		w = XTextWidth(bta_sys.xfont, str+start, end-start);

		y += h;
	}
}

/////////////////////////////////////////////////////////////////////////////////////

int bta_sys_init(BTA_SYS_WINDOW pwin) {
	int screen, sw, sh;
	Window root;
	XSetWindowAttributes wa;
	logmsg("bta_sys_init()\n");

	bta_sys.display = XOpenDisplay(NULL);
	if( bta_sys.display==NULL ) {
		logmsg("unable to open display!");
		return 1;
	}
	bta_sys.browser_win=pwin;
	screen = DefaultScreen(bta_sys.display);
	bta_sys.colormap = DefaultColormap(bta_sys.display, screen);

	root = RootWindow(bta_sys.display, screen);
	sw = DisplayWidth(bta_sys.display, screen);
	sh = DisplayHeight(bta_sys.display, screen);
	bta_sys.cursor[0] = XCreateFontCursor(bta_sys.display, XC_left_ptr);
	bta_sys.cursor[1] = XCreateFontCursor(bta_sys.display, XC_xterm);
	XAllocNamedColor(bta_sys.display, bta_sys.colormap, "#000055", &bta_sys.clr[0], &bta_sys.clr[0]);
	XAllocNamedColor(bta_sys.display, bta_sys.colormap, "#555599", &bta_sys.clr[1], &bta_sys.clr[1]);
	XAllocNamedColor(bta_sys.display, bta_sys.colormap, "#ddddff", &bta_sys.clr[2], &bta_sys.clr[2]);
	XAllocNamedColor(bta_sys.display, bta_sys.colormap, "#ffffff", &bta_sys.clr[3], &bta_sys.clr[3]);
	XAllocNamedColor(bta_sys.display, bta_sys.colormap, "#ffcccc", &bta_sys.clr[4], &bta_sys.clr[4]);

	bta_sys.win = XCreateSimpleWindow(bta_sys.display, root, (sw/2)-(DIALOG_WIDTH/2), (sh/2)-(DIALOG_HEIGHT/2), DIALOG_WIDTH, DIALOG_HEIGHT, 0, 0, bta_sys.clr[3].pixel);
	bta_sys.gc = XCreateGC(bta_sys.display, bta_sys.win, 0, NULL);

	XStoreName(bta_sys.display, bta_sys.win, "BetterThanAds - Confirm payment");
	XSetTransientForHint(bta_sys.display, bta_sys.win, bta_sys.browser_win);
	XDefineCursor(bta_sys.display, bta_sys.win, bta_sys.cursor[0]);

	// grab KeyPress, ButtonRelease, PointerMotion, ExposureMask
	wa.event_mask= KeyPressMask|ButtonPressMask|ButtonReleaseMask|PointerMotionMask|ExposureMask;
	XChangeWindowAttributes(bta_sys.display, bta_sys.win, CWEventMask, &wa);

	// banner image
	if( XpmCreatePixmapFromData(bta_sys.display, bta_sys.win, (char **)dialog_xpm, &bta_sys.banner, NULL, NULL) ) {
		logmsg("pixmap load failed!\n");
		return 1;
	}

	if( !(bta_sys.xfont = XLoadQueryFont(bta_sys.display, XFONT_STR)) && !(bta_sys.xfont = XLoadQueryFont(bta_sys.display, "fixed"))) {
		logmsg("no fonts!\n");
		return 1;
	}
	XSetFont(bta_sys.display, bta_sys.gc, bta_sys.xfont->fid);

	// catch close messages
	bta_sys.wm_delete = XInternAtom(bta_sys.display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(bta_sys.display, bta_sys.win, &bta_sys.wm_delete, 1);

	return 0;
}

void *bta_sys_thread(void *m) {
	const char *message =(const char *)m;
	XEvent ev;
	int pinlen=0;
	int done=0, redraw=1, unmapped=0;
	int z=0;
	int over=OVER_OTHER;

	XMapRaised(bta_sys.display, bta_sys.win);
	XFlush(bta_sys.display);
	/////////////
	// get pin/button
	bta_sys.pin[0]=0;
	while( !done && !unmapped ) {
    //if( XWindowEvent(bta_sys.display, bta_sys.win, ButtonReleaseMask|PointerMotionMask|ExposureMask|KeyPressMask, &ev) )
    //if( XWindowEvent(bta_sys.display, bta_sys.win, ~0, &ev) )
		if( XNextEvent(bta_sys.display, &ev) )
			break;
		
		if( ev.type==Expose ) {
			redraw=1;
		} else if( ev.type==ClientMessage && ev.xclient.data.l[0] == bta_sys.wm_delete ) {
			// close / clean up nicely
			done=-1;
		} else if( ev.type==MotionNotify ) {
			int lastover=over;
			int x=ev.xmotion.x, y=ev.xmotion.y;
			// tl=260,67  br=394,86 
			if( x>=260 && x<=394 && y>=67 && y<=86 ) {
				if( over!=OVER_PINBOX ) {
					over=OVER_PINBOX;
					XDefineCursor(bta_sys.display, bta_sys.win, bta_sys.cursor[1]);
				}
			} else {
				if( over==OVER_PINBOX )
					XDefineCursor(bta_sys.display, bta_sys.win, bta_sys.cursor[0]);
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
				case '\b': if( pinlen>0 ) bta_sys.pin[ --pinlen ]=0; break;
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
									 bta_sys.pin[pinlen++]=key[0];
									 bta_sys.pin[pinlen]=0;
									 break;
			}

			redraw=1;
		}

		// can't be done if no pin
		if( done==1 && bta_sys.pin[0]==0 ) done=0;

		if( done ) 
			XUnmapWindow(bta_sys.display, bta_sys.win);

		else if( redraw ) {
			// draw banner
			XCopyArea(bta_sys.display, bta_sys.banner, bta_sys.win, bta_sys.gc, 0,0, DIALOG_WIDTH, DIALOG_BANNER_HEIGHT, 0,0);

			// draw pinbox
			if( prompt_running==2 )
				XSetForeground(bta_sys.display, bta_sys.gc, bta_sys.clr[4].pixel);
			else
				XSetForeground(bta_sys.display, bta_sys.gc, over==OVER_PINBOX?bta_sys.clr[2].pixel:bta_sys.clr[3].pixel);
			XFillRectangles(bta_sys.display, bta_sys.win, bta_sys.gc, &pinbox, 1);

			XSetForeground(bta_sys.display, bta_sys.gc, bta_sys.clr[0].pixel);
			XRectangle dot= {262,74, 5,5};
			for(z=0; z<=pinlen; z++) {
				dot.x=262+z*10;
				if( z!=pinlen )
					XFillRectangles(bta_sys.display, bta_sys.win, bta_sys.gc, &dot, 1);
				else // cursor
					XDrawLine(bta_sys.display, bta_sys.win, bta_sys.gc, dot.x, 71, dot.x, 83);
			}

			// draw description text
			XSetForeground(bta_sys.display, bta_sys.gc, bta_sys.clr[0].pixel);
			descriptiontext(5, 110, 434, message);

			// draw buttons
			XSetForeground(bta_sys.display, bta_sys.gc, over==OVER_CANCEL?bta_sys.clr[1].pixel:bta_sys.clr[0].pixel);
			XFillRectangles(bta_sys.display, bta_sys.win, bta_sys.gc, buttons, 1);

			XSetForeground(bta_sys.display, bta_sys.gc, over==OVER_OK?bta_sys.clr[1].pixel:bta_sys.clr[0].pixel);
			XFillRectangles(bta_sys.display, bta_sys.win, bta_sys.gc, buttons+1, 1);

			// draw text on buttons
			XSetForeground(bta_sys.display, bta_sys.gc, bta_sys.clr[3].pixel);

			centeredtext(buttons[1].x+buttons[1].width/2, buttons[1].y+buttons[1].height/2, "OK");
			centeredtext(buttons[0].x+buttons[0].width/2, buttons[0].y+buttons[0].height/2, "Cancel");
			
			redraw=0;
		}
	}
	XFlush(bta_sys.display);
	prompt_running=0;

	if( done==1 ) {
		bta_api_got_pin(bta_sys.instance, bta_sys.pin);
	} else {
		bta_api_got_pin(bta_sys.instance, "x");
	}
}

void bta_sys_close() {
	if( prompt_running )
		pthread_cancel(bta_sys.pt);

	XFreeColors(bta_sys.display, bta_sys.colormap, &bta_sys.clr[0].pixel, 1, 0);
	XFreeColors(bta_sys.display, bta_sys.colormap, &bta_sys.clr[1].pixel, 1, 0);
	XFreeColors(bta_sys.display, bta_sys.colormap, &bta_sys.clr[2].pixel, 1, 0);
	XFreeColors(bta_sys.display, bta_sys.colormap, &bta_sys.clr[3].pixel, 1, 0);
	XFreeColors(bta_sys.display, bta_sys.colormap, &bta_sys.clr[4].pixel, 1, 0);

	XFreePixmap(bta_sys.display, bta_sys.banner);
	XFreeFont(bta_sys.display, bta_sys.xfont);
	XFreeCursor(bta_sys.display, bta_sys.cursor[1]);
	XFreeCursor(bta_sys.display, bta_sys.cursor[0]);
	XFreeGC(bta_sys.display, bta_sys.gc);
	XDestroyWindow(bta_sys.display, bta_sys.win);

	XSync(bta_sys.display, True);
	XCloseDisplay(bta_sys.display);
}

void bta_sys_prompt(NPP instance, char *message) {
	if( prompt_running ) return;
	prompt_running=1;
	bta_sys.instance=instance;
	pthread_create(&bta_sys.pt, NULL, bta_sys_thread, message);
}
void bta_sys_error(NPP instance, char *message) {
	if( prompt_running ) {
		prompt_running=2;
		return;
	}
	prompt_running=2;
	bta_sys.instance=instance;
	pthread_create(&bta_sys.pt, NULL, bta_sys_thread, message);
}

void _bta_sys_draw(NPWindow *npwin) {
	Display *dpy = NULL;
	Colormap cmap;
	XRectangle r;
	Window win;
	XColor clr, white;
	XEvent ev;
	GC gc;
	XFontStruct *xfont;
	const char *str = "BTA: 25c/mo"; // TODO: get from instance...
	int len = strlen(str);
	int x = npwin->width/2, y = npwin->height/2;

	dpy = ((NPSetWindowCallbackStruct *)(npwin->ws_info))->display;
	cmap = ((NPSetWindowCallbackStruct *)(npwin->ws_info))->colormap;

	xfont = XLoadQueryFont(dpy, XFONT_BUTTON_STR);
	y -= (xfont->ascent+xfont->descent)/2;
	y += xfont->ascent+1;
	x -= XTextWidth(xfont, str, len)/2;

	win=(Window)npwin->window;
	gc = XCreateGC(dpy, win, 0, NULL);
	XAllocNamedColor(dpy, cmap, "#000055", &clr, &clr);
	XAllocNamedColor(dpy, cmap, "#ffffff", &white, &white);

	r.x      = 0; // npwin->x;
	r.y      = 0; // npwin->y;
	r.width  = npwin->width;
	r.height = npwin->height;
	
	XSetFont(dpy, gc, xfont->fid);

	XSetForeground(dpy, gc, clr.pixel);
	XFillRectangles(dpy, win, gc, &r, 1);

	XSetForeground(dpy, gc, white.pixel);
	XDrawString(dpy, win, gc, x,y, str, len);

	XFreeFont(dpy, xfont);
	XFreeColors(dpy, cmap, &clr.pixel, 1, 0);
	XFreeGC(dpy, gc);
}

void _bta_sys_xt_callback( Widget w, NPWindow *npwin, XEvent* ev, char* cont )  {
	if( ev->type==Expose ) {
		_bta_sys_draw(npwin);
		*cont = 0;
	} else if( ev->type==ButtonPress ) {
		bta_api_clicked(bta_sys.instance);
		*cont = 0;
	} else if( ev->type==DestroyNotify) {
		XtRemoveEventHandler( w, ExposureMask|ButtonPressMask, 1, _bta_sys_xt_callback, npwin );
		*cont = 0;
	}
}

void bta_sys_draw(NPP instance, NPWindow *npwin) {
	bta_sys.instance=instance;

	Display *dpy = ((NPSetWindowCallbackStruct *)(npwin->ws_info))->display;
	Widget w = XtWindowToWidget(dpy, (Window)(npwin->window));
	XtAddEventHandler( w, ExposureMask|ButtonPressMask, 1, _bta_sys_xt_callback, npwin );
}
/////////////////////////////////////////////////////////////////////////////////////

pthread_t bta_sys_pt;
sem_t bta_sys_dataready, bta_sys_dataload;
int __is_running=0;

void bta_sys_start_apithread() {
	__is_running=1;
	sem_init(&bta_sys_dataready, 0, 0);
	sem_init(&bta_sys_dataload, 0, 1);
	pthread_create(&bta_sys_pt, NULL, bta_api_thread, NULL);
}
void bta_sys_stop_apithread() {
	__is_running=0;
	sem_post(&bta_sys_dataready);
	pthread_join(bta_sys_pt,NULL);
	sem_destroy(&bta_sys_dataload);
	sem_destroy(&bta_sys_dataready);
}

int  bta_sys_is_running() {
	return __is_running;
}

int  bta_sys_wait_dataready() {
	sem_wait(&bta_sys_dataready);
}
void bta_sys_post_dataready() {
	sem_post(&bta_sys_dataready);
}

void bta_sys_lock_dataload() {
	sem_wait(&bta_sys_dataload);
}
void bta_sys_unlock_dataload() {
	sem_post(&bta_sys_dataload);
}
