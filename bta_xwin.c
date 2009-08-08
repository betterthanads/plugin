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
// BetterThanAds NPAPI Plugin - X Windows-specific code
//

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

// cancel, ok buttons
const char *buttontext[2] = { "Cancel", "OK" };
XRectangle buttons[2] = { {10,DIALOG_HEIGHT-35, 150, 25}, {DIALOG_WIDTH-160, DIALOG_HEIGHT-35, 150, 25} };
XRectangle pinbox = {260,67, 394-260,86-67};

struct _bta_prompt_info {
	Display *dpy;
	Window parent;
	Window win;
	Colormap colormap;

	GC gc;
	XFontStruct *xfont;
	XColor clr[5];
	Pixmap banner;
	Cursor cursor[2];
	Atom wm_delete;

	NPP instance;
	char *pin;
	char error[1];
};

//////////////////
// helper funcs

void centeredtext(struct _bta_prompt_info *p, int cx, int cy, const char *str) {
	int len=strlen(str);
  int x=cx,y=cy;

	y -= (p->xfont->ascent+p->xfont->descent)/2;
	y += p->xfont->ascent+1;
	x -= XTextWidth(p->xfont, str, len)/2;

	XDrawString(p->dpy, p->win, p->gc, x,y, str, len);
}

void descriptiontext(struct _bta_prompt_info *p, int tx, int ty, int wx, const char *str) {
	int len=strlen(str);
	int start=0;
	int end=len, endline=0;
	int y=ty, h = p->xfont->ascent+p->xfont->descent;
	int w = XTextWidth(p->xfont, str+start, end-start);

	y -= h/2;
	y += p->xfont->ascent+1;

	while( start<len ) {
		endline=start;
		while( str[endline]!='\n' && endline<end ) endline++;
		if( str[endline]=='\n' ) end=endline-1;
		else end=len;
		w = XTextWidth(p->xfont, str+start, end-start);

		while( w>=wx ) {
			while( str[end]!=' ' && start<end ) end--;
			w = XTextWidth(p->xfont, str+start, end-start);
			end--;
		}
		end++;
		if(end>len) end=len;
		XDrawString(p->dpy, p->win, p->gc, tx, y, str+start, end-start);
		start=end+1;
		end=len;
		w = XTextWidth(p->xfont, str+start, end-start);

		y += h;
	}
}

/////////////////////////////////////////////////////////////////////////////////////

// these are only used in the prompt window
//  firefox seems to drop key events in separate threads
Display *_bta_sys_dpy = NULL;
Colormap _bta_sys_colormap;

int bta_sys_init() {
	_bta_sys_dpy = XOpenDisplay(NULL);
	if( _bta_sys_dpy==NULL ) {
		logmsg("unable to open display!");
		return 1;
	}
	int screen = DefaultScreen(_bta_sys_dpy);
	_bta_sys_colormap = DefaultColormap(_bta_sys_dpy, screen);
	return 0;
}

void bta_sys_close() {
	XCloseDisplay(_bta_sys_dpy);
}

////////////////////////////////////////////////////////////
// BTA button drawing code
////////////////////////////////////////////////////////////

// completely self-contained button drawer
//   we could probably cache the colors and fonts...
void bta_sys_draw(NPP instance) {
  bta_info *bta = (bta_info*)instance->pdata;
	Display *dpy = NULL;
	Colormap cmap;
	XRectangle r;
	Window win;
	XColor clr, white;
	XEvent ev;
	GC gc;
	XFontStruct *xfont;
	char str[64];
	int len = 5;
	int x = bta->width/2, y = bta->height/2;
	sprintf(str, "BTA: $%0.2f%s", bta->price, bta->type==1?"/mo":"");
	len = strlen(str);

	dpy=bta->dpy;
	cmap=bta->cmap;

	xfont = XLoadQueryFont(dpy, XFONT_BUTTON_STR);
	y -= (xfont->ascent+xfont->descent)/2;
	y += xfont->ascent+1;
	x -= XTextWidth(xfont, str, len)/2;

	win=bta->window;
	gc = XCreateGC(dpy, win, 0, NULL);
	XAllocNamedColor(dpy, cmap, "#000055", &clr, &clr);
	XAllocNamedColor(dpy, cmap, "#ffffff", &white, &white);

	r.x = 0;  r.width  = bta->width;
	r.y = 0;	r.height = bta->height;
	
	XSetFont(dpy, gc, xfont->fid);

	XSetForeground(dpy, gc, clr.pixel);
	XFillRectangles(dpy, win, gc, &r, 1);

	XSetForeground(dpy, gc, white.pixel);
	XDrawString(dpy, win, gc, x,y, str, len);

	XFreeColors(dpy, cmap, &clr.pixel, 1, 0);
	XFreeColors(dpy, cmap, &white.pixel, 1, 0);
	XFreeGC(dpy, gc);
	XFreeFont(dpy, xfont);
}

void _bta_sys_xt_callback( Widget w, void *x, XEvent* ev, char* cont )  {
	if( ev->type==Expose ) {
		bta_sys_draw( (NPP)x );
		*cont = 0;
	} else if( ev->type==ButtonPress ) {
		bta_sys_prompt( (NPP)x, "");
		*cont = 0;
	} else if( ev->type==ClientMessage && strncmp(ev->xclient.data.b,"PIN",3)==0 ) {
		bta_api_do_payment( (NPP)x );
		*cont = 0;
	}
}

void bta_sys_windowhook(NPP instance, NPWindow *npwin_new) {
	if( instance->pdata ) {
		bta_info *bta = (bta_info*)instance->pdata;

		// if closing or changing, remove old callback
		if( npwin_new==NULL && bta->window!=0 ) {
			Widget w = XtWindowToWidget(bta->dpy, bta->window);

			XtRemoveEventHandler( w, ExposureMask|ButtonPressMask, 1, _bta_sys_xt_callback, instance );
			
			bta->dpy    = NULL;
			bta->cmap   = (Colormap)0;
			bta->window = (Window)0;
		}

		// install new callback
		if( npwin_new != NULL ) {
			bta->dpy    = ((NPSetWindowCallbackStruct *)(npwin_new->ws_info))->display;
			bta->cmap   = ((NPSetWindowCallbackStruct *)(npwin_new->ws_info))->colormap;
			bta->window = (Window)npwin_new->window;
			bta->width  = npwin_new->width;
			bta->height = npwin_new->height;
			Widget w = XtWindowToWidget(bta->dpy, bta->window);
			
			XtAddEventHandler( w, ExposureMask|ButtonPressMask, 1, _bta_sys_xt_callback, instance );
		}
	}
}

////////////////////////////////////////////////////////////
// BTA prompt window code
////////////////////////////////////////////////////////////

// prompt event-handling callback
void *_bta_sys_prompt( void *x )  {
	struct _bta_prompt_info *p = (struct _bta_prompt_info *)x;

	XEvent ev;
	int pinlen=0;
	int done=0, redraw=1;
	int z=0;
	int over=OVER_OTHER;

	XMapRaised(p->dpy, p->win);
	p->pin[0]=0;
	while( done==0 ) {
		XSync(p->dpy, False);

		if( XNextEvent(p->dpy, &ev) )
			  break;
		
		XLockDisplay(p->dpy);
		if( ev.type==Expose ) {
			redraw=1;
		} else if( ev.type==ClientMessage && ev.xclient.data.l[0] == p->wm_delete ) {
			// close / clean up nicely
			done=-1;
		} else if( ev.type==MotionNotify ) {
			int lastover=over;
			int x=ev.xmotion.x, y=ev.xmotion.y;
			// tl=260,67  br=394,86 
			if( x>=260 && x<=394 && y>=67 && y<=86 ) {
				if( over!=OVER_PINBOX ) {
					over=OVER_PINBOX;
					XDefineCursor(p->dpy, p->win, p->cursor[1]);
				}
			} else {
				if( over==OVER_PINBOX )
					XDefineCursor(p->dpy, p->win, p->cursor[0]);
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
		} else if( ev.type==KeyPress ) {
			char key[5];
			XLookupString(&ev.xkey, key, 5, NULL,NULL);
			switch( key[0] ) {
				case 27: done=-1; break;
				case '\b': if( pinlen>0 ) p->pin[ --pinlen ]=0; break;
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
									 p->pin[pinlen++]=key[0];
									 p->pin[pinlen]=0;
									 break;
			}

			redraw=1;
		}

		// can't be done if no pin
		if( done==1 && p->pin[0]==0 ) done=0;

		if( done!=0 ) 
			XUnmapWindow(p->dpy, p->win);

		else if( redraw ) {
			// draw banner
			XCopyArea(p->dpy, p->banner, p->win, p->gc, 0,0, DIALOG_WIDTH, DIALOG_BANNER_HEIGHT, 0,0);

			// draw pinbox
			XSetForeground(p->dpy, p->gc, over==OVER_PINBOX?p->clr[2].pixel:p->clr[3].pixel);
			XFillRectangles(p->dpy, p->win, p->gc, &pinbox, 1);

			XSetForeground(p->dpy, p->gc, p->clr[0].pixel);
			XRectangle dot= {262,74, 5,5};
			for(z=0; z<=pinlen; z++) {
				dot.x=262+z*10;
				if( z!=pinlen )
					XFillRectangles(p->dpy, p->win, p->gc, &dot, 1);
				else // cursor
					XDrawLine(p->dpy, p->win, p->gc, dot.x, 71, dot.x, 83);
			}

			// draw description text
			XSetForeground(p->dpy, p->gc, p->clr[0].pixel);
			descriptiontext(p, 5, 110, 434, ((bta_info *)p->instance->pdata)->desc);

			// draw error message
			if( p->error[0]!=0 ) {
				XRectangle err = {10,DIALOG_HEIGHT-70, DIALOG_WIDTH-20, 25};
				XSetForeground(p->dpy, p->gc, p->clr[4].pixel);
				XFillRectangles(p->dpy, p->win, p->gc, &err, 1);
				XSetForeground(p->dpy, p->gc, p->clr[0].pixel);
				centeredtext(p, DIALOG_WIDTH/2, DIALOG_HEIGHT-58, p->error);
			}

			// draw buttons
			XSetForeground(p->dpy, p->gc, over==OVER_CANCEL?p->clr[1].pixel:p->clr[0].pixel);
			XFillRectangles(p->dpy, p->win, p->gc, buttons, 1);

			XSetForeground(p->dpy, p->gc, over==OVER_OK?p->clr[1].pixel:p->clr[0].pixel);
			XFillRectangles(p->dpy, p->win, p->gc, buttons+1, 1);

			// draw text on buttons
			XSetForeground(p->dpy, p->gc, p->clr[3].pixel);

			centeredtext(p, buttons[1].x+buttons[1].width/2, buttons[1].y+buttons[1].height/2, buttontext[1]);
			centeredtext(p, buttons[0].x+buttons[0].width/2, buttons[0].y+buttons[0].height/2, buttontext[0]);
			
			redraw=0;
		}
		XUnlockDisplay(p->dpy);
	}

	if( done>0 ) {
		// send ClientMessage to parent window with pin
		XEvent gotpin;

		gotpin.type=ClientMessage;
		gotpin.xclient.window=p->parent;
		gotpin.xclient.message_type=0;
		gotpin.xclient.format=8;
		strcpy(gotpin.xclient.data.b, "PIN");
		XSendEvent(p->dpy, p->parent, False, 0, &gotpin);
	}

	// free up stuff
	XFreeColors(p->dpy, p->colormap, &p->clr[0].pixel, 1, 0);
	XFreeColors(p->dpy, p->colormap, &p->clr[1].pixel, 1, 0);
	XFreeColors(p->dpy, p->colormap, &p->clr[2].pixel, 1, 0);
	XFreeColors(p->dpy, p->colormap, &p->clr[3].pixel, 1, 0);
	XFreeColors(p->dpy, p->colormap, &p->clr[4].pixel, 1, 0);

	XFreeCursor(p->dpy, p->cursor[1]);
	XFreeCursor(p->dpy, p->cursor[0]);

	XFreePixmap(p->dpy, p->banner);
	XFreeFont(p->dpy, p->xfont);
	XFreeGC(p->dpy, p->gc);

	XDestroyWindow(p->dpy, p->win);
	XFlush(p->dpy);

	bta_free(p);
	return NULL;
}

// creates a prompt window, shows it, and starts a pthread to process events
void bta_sys_prompt(NPP instance, const char *error) {
	int screen, sw, sh;
	struct _bta_prompt_info *p = (struct _bta_prompt_info *)bta_malloc(sizeof(struct _bta_prompt_info)+strlen(error));
	Window root;
	XSetWindowAttributes wa;

	p->dpy = _bta_sys_dpy;
	p->colormap = _bta_sys_colormap;

	screen = DefaultScreen(p->dpy);
	p->parent = ((bta_info*)instance->pdata)->window;
	root = RootWindow(p->dpy, screen);
	sw = DisplayWidth(p->dpy, screen);
	sh = DisplayHeight(p->dpy, screen);
	
	p->cursor[0] = XCreateFontCursor(p->dpy, XC_left_ptr);
	p->cursor[1] = XCreateFontCursor(p->dpy, XC_xterm);
	
	XAllocNamedColor(p->dpy, p->colormap, "#000055", &p->clr[0], &p->clr[0]);
	XAllocNamedColor(p->dpy, p->colormap, "#555599", &p->clr[1], &p->clr[1]);
	XAllocNamedColor(p->dpy, p->colormap, "#ddddff", &p->clr[2], &p->clr[2]);
	XAllocNamedColor(p->dpy, p->colormap, "#ffffff", &p->clr[3], &p->clr[3]);
	XAllocNamedColor(p->dpy, p->colormap, "#ffcccc", &p->clr[4], &p->clr[4]);
	
	// create window
  p->win = XCreateSimpleWindow(p->dpy, root, (sw/2)-(DIALOG_WIDTH/2), (sh/2)-(DIALOG_HEIGHT/2), DIALOG_WIDTH, DIALOG_HEIGHT, 0, 0, p->clr[3].pixel);
	wa.event_mask= KeyReleaseMask|KeyPressMask|ButtonPressMask|ButtonReleaseMask|PointerMotionMask|ExposureMask;
	XChangeWindowAttributes(p->dpy, p->win, CWEventMask, &wa);

	// banner image
	if( XpmCreatePixmapFromData(p->dpy, p->win, (char **)dialog_xpm, &p->banner, NULL, NULL) ) {
		logmsg("pixmap load failed!\n");
		return;
	}

	if( !(p->xfont = XLoadQueryFont(p->dpy, XFONT_STR)) &&
			!(p->xfont = XLoadQueryFont(p->dpy, "fixed"))) {
		logmsg("no fonts!\n");
		return;
	}

	// create transient popup window
	p->gc = XCreateGC(p->dpy, p->win, 0, NULL);
	XStoreName(p->dpy, p->win, "BetterThanAds - Confirm payment");
	XSetTransientForHint(p->dpy, p->win, p->parent);
	XDefineCursor(p->dpy, p->win, p->cursor[0]);
	XSetFont(p->dpy, p->gc, p->xfont->fid);

	// catch close messages
	p->wm_delete = XInternAtom(p->dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(p->dpy, p->win, &p->wm_delete, 1);

  p->instance = instance;
	p->pin = ((bta_info *)instance->pdata)->pin;
	strcpy(p->error, error);

	// start a pthread for the window event-loop
	pthread_t pt;
	pthread_create(&pt, NULL, _bta_sys_prompt, p);
}
