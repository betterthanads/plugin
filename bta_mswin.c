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
// BetterThanAds NPAPI Plugin - MS Windows-specific code
//
// Known Bugs:
//   prompt windows use global info pointers, so having more than one open at a
//     time will probably not work correctly

#include <stdio.h>
#include <string.h>

#include <windows.h>
#include <tchar.h>
#include <strsafe.h>

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

struct _bta_prompt_info {
	HWND parent;
	HWND win;
	HFONT hfont;
	HCURSOR cursor[2];
	HANDLE banner;

	NPP instance;
	char *pin;
	char error[1];
};

// cancel, ok buttons
const char *buttontext[2] = { "Cancel", "OK" };

// RECTs are +1 on bottom and right because MS doesn't draw the last pixel
RECT buttons[2] = {
	{10,DIALOG_HEIGHT-35, 161, DIALOG_HEIGHT-9}, 
	{DIALOG_WIDTH-160, DIALOG_HEIGHT-35, DIALOG_WIDTH-9, DIALOG_HEIGHT-9}
};
RECT pinbox = {260,67, 395,87};

// seemed easier than dealing with resources and BMPs
HBITMAP XPMLoadBitmap(HDC hdc, const char **xpm) {
	NPLOGPALETTE pal;
	int i=0, j=0, k=0, off=0, clr=0, possible_colors=0;
	int width, height, ncolors, bytesperpixel;
	unsigned char *pixel_data;

	if( sscanf(xpm[0], "%d %d %d %d", &width, &height, &ncolors, &bytesperpixel)!=4 )
		return NULL;

	pal = (NPLOGPALETTE)LocalAlloc(LPTR, sizeof(LOGPALETTE)+(ncolors-1)*sizeof(PALETTEENTRY));
	if( !pal ) return NULL;
	pal->palVersion=0x300;
	pal->palNumEntries=ncolors;

	// load colors
	i=bytesperpixel;
	possible_colors=95;  // (chars 32-126)
	while( --i>0 ) {
		possible_colors*=95;
	}

	int *colorLookup = (int *)LocalAlloc(LPTR, possible_colors*sizeof(int));
	if( !colorLookup ) return NULL;
	for(i=0; i<possible_colors; i++)
		colorLookup[i]=0;

	int curColor=0;
	for(i=0; i<ncolors; i++) {
		const char *cidx=xpm[1+i];
		int red,green,blue;
		if( sscanf(xpm[1+i]+bytesperpixel, " c #%02X%02X%02X", &red,&green,&blue)!=3 ) 
			continue;

		clr=0;
		for(j=0; j<bytesperpixel; j++) {
			clr*=95;
			clr+=cidx[j]-32;
		}
		pal->palPalEntry[curColor].peRed=red;
		pal->palPalEntry[curColor].peGreen=green;
		pal->palPalEntry[curColor].peBlue=blue;
		pal->palPalEntry[curColor].peFlags=0;
		colorLookup[clr]=curColor++;
	}

	// load palette into dc
	HPALETTE palobj = CreatePalette(pal);
	SelectObject(hdc, palobj);
	RealizePalette(hdc);
	DeleteObject(palobj);

	pixel_data = (unsigned char *)LocalAlloc(LPTR, width*height*3);
	if( !pixel_data ) return NULL;

	for(i=0; i<height; i++) {
		const char *line = xpm[i+ncolors+1];
		for(j=0; j<width; j++) {
			clr=0;
			for(k=0; k<bytesperpixel; k++) {
				clr*=95;
				clr+=line[j*bytesperpixel + k]-32;
			}

			pixel_data[((i*width+j)*3)+2]=pal->palPalEntry[ colorLookup[clr] ].peRed;
			pixel_data[((i*width+j)*3)+1]=pal->palPalEntry[ colorLookup[clr] ].peGreen;
			pixel_data[((i*width+j)*3)+0]=pal->palPalEntry[ colorLookup[clr] ].peBlue;
		}
	}

	LocalFree(colorLookup);
	LocalFree(pal); 

	/////////////////////////////////
	BITMAPINFO bmi;
	BITMAPINFOHEADER bmh;
	bmh.biHeight = -height;
	bmh.biWidth = width;
	bmh.biPlanes=1;
	bmh.biBitCount=24;
	bmh.biCompression=BI_RGB;
	bmh.biSizeImage=0;
	bmh.biYPelsPerMeter=72;
	bmh.biXPelsPerMeter=72;
	bmh.biClrImportant=ncolors;
	bmh.biClrUsed=0;
	bmh.biSize=sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader=bmh;

	HBITMAP bmp = CreateDIBitmap(hdc, &bmh, CBM_INIT, pixel_data, &bmi, 0);
	LocalFree(pixel_data);
	return bmp;
}

/////////////////////////////////////////////////////////////////////////////////////

struct _bta_prompt_info *_bta_sys_info=NULL;

LRESULT CALLBACK _bta_sys_prompt_callback( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	struct _bta_prompt_info *p = _bta_sys_info; //TODO: get from starting thread ...
	POINTS m;
	int x=0,y=0,z=0, lastover=OVER_OTHER;
	HDC dc;
	static int pinlen=0;
	static int done=0, redraw=1;
	static int over=OVER_OTHER;
	static HBRUSH clr[5];
	static HFONT hFont;

	switch( uMsg ) {
		case WM_CREATE:
			pinlen=0;
			done=0;
			redraw=0;
			over=OVER_OTHER;
			p->pin[0]=0;

			clr[0] = CreateSolidBrush(RGB(0,0,0x55));
			clr[1] = CreateSolidBrush(RGB(0x55,0x55,0x99));
			clr[2] = CreateSolidBrush(RGB(0xdd,0xdd,0xff));
			clr[3] = CreateSolidBrush(RGB(0xff,0xff,0xff));
			clr[4] = CreateSolidBrush(RGB(0xff,0xcc,0xcc));
			hFont = (HFONT)GetStockObject(ANSI_VAR_FONT); 

			// load banner image
			dc = GetDC(hwnd);
			p->banner = XPMLoadBitmap(dc, dialog_xpm);
			ReleaseDC(hwnd, dc);

			return DefWindowProc(hwnd, uMsg, wParam, lParam);
			break;

		case WM_MOVE:
			InvalidateRect(hwnd, NULL, FALSE);
			UpdateWindow(hwnd);
			break;

		case WM_PAINT: 
			redraw=1;
			break;

		case WM_CLOSE:
			done=-1;
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WM_MOUSEMOVE:
			m = MAKEPOINTS(lParam);
			lastover=over;
			x=m.x; y=m.y;

			if( x>=pinbox.left && x<=pinbox.right && y>=pinbox.top && y<=pinbox.bottom ) {
				if( over!=OVER_PINBOX ) {
					over=OVER_PINBOX;
					SetCursor(p->cursor[1]);
				}
			} else {
				if( over==OVER_PINBOX )
					SetCursor(p->cursor[0]);
				over=OVER_OTHER;
			}

			if( y>=buttons[0].top && y<=buttons[0].bottom ) {
				if( x>=buttons[0].left && x<=buttons[0].right ) {
					over=OVER_CANCEL;
				} else if( x>=buttons[1].left && x<=buttons[1].right ) {
					over=OVER_OK;
				}
			}
			if( over!=lastover ) { InvalidateRect(hwnd, NULL, FALSE); UpdateWindow(hwnd); }
			break;

		case WM_SETCURSOR:
			if( over==OVER_PINBOX )
				SetCursor(p->cursor[1]);
			return 0;

		case WM_LBUTTONUP: 
			switch( over) {
				case OVER_OK: done=1; break;
				case OVER_CANCEL: done=-1; break;
			}
			break;

		case WM_CHAR: 
			switch( wParam ) {
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
										 p->pin[pinlen++]=(char)wParam;
									 p->pin[pinlen]=0;
									 break;
			}

			InvalidateRect(hwnd, NULL, FALSE);
			UpdateWindow(hwnd);
			break;
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);

	}
	// can't be done if no pin
	if( done==1 && p->pin[0]==0 ) done=0;

	if( redraw ) {
		redraw=0;

		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);

		// draw banner
		HDC hdcBanner = CreateCompatibleDC(ps.hdc);
		SelectObject(hdcBanner, p->banner);
		BitBlt(ps.hdc,0,0,DIALOG_WIDTH, DIALOG_BANNER_HEIGHT, hdcBanner,0,0,SRCCOPY);

		// draw pinbox
		if( over==OVER_PINBOX ) 
			FillRect(ps.hdc, &pinbox, clr[2]);
		else
			FillRect(ps.hdc, &pinbox, clr[3]);

		RECT dot = {262,74, 268,80};
		for(z=0; z<=pinlen; z++) {
			dot.left=262+z*10;
			dot.right=dot.left+6;
			if( z!=pinlen )
				FillRect(ps.hdc, &dot, clr[0]);
			else {// cursor
				MoveToEx(ps.hdc, dot.left, 71, NULL);
				LineTo(ps.hdc, dot.left, 83);
			}
		}

		// draw description text
		RECT descRect;	
		SetTextColor(ps.hdc, RGB(0,0,0));
		SetBkColor(ps.hdc, RGB(0xFF,0xFF,0xFF));
		SetRect(&descRect, 5,110, DIALOG_WIDTH-5, DIALOG_HEIGHT-40);
		DrawTextA(ps.hdc, ((bta_info *)p->instance->pdata)->desc, -1, &descRect, DT_LEFT|DT_TOP|DT_WORDBREAK);

		// draw error message
		if( p->error[0]!=0 ) {
			RECT err;
			SetRect(&err, 10, DIALOG_HEIGHT-70, DIALOG_WIDTH-10, DIALOG_HEIGHT-45);
			FillRect(ps.hdc, &err, clr[4]);
			SetBkColor(ps.hdc, RGB(0xFF,0xCC,0xCC));
			DrawTextA(ps.hdc, p->error, -1, &err, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
		}

		// draw buttons
		SelectObject(ps.hdc, hFont);
		SetTextColor(ps.hdc, RGB(0xFF,0xFF,0xFF));
		if( over==OVER_CANCEL ) SetBkColor(ps.hdc, RGB(0x55,0x55,0x99));
		else SetBkColor(ps.hdc, RGB(0,0,0x55));
		FillRect(ps.hdc, &buttons[0], over==OVER_CANCEL?clr[1]:clr[0]);
		DrawText(ps.hdc, L"Cancel", -1, &buttons[0], DT_CENTER|DT_VCENTER|DT_SINGLELINE);
		if( over==OVER_OK ) SetBkColor(ps.hdc, RGB(0x55,0x55,0x99));
		else SetBkColor(ps.hdc, RGB(0,0,0x55));
		FillRect(ps.hdc, &buttons[1], over==OVER_OK?clr[1]:clr[0]);
		DrawText(ps.hdc, L"OK", -1, &buttons[1], DT_CENTER|DT_VCENTER|DT_SINGLELINE);

		EndPaint(hwnd, &ps);
		return 0;
	}

	if( done!=0 ) {
		// tell main window we have a PIN
		if( done>0 )
			SendMessage(p->parent, WM_USER, 42, 42);

		DeleteObject( clr[0] );
		DeleteObject( clr[1] );
		DeleteObject( clr[2] );
		DeleteObject( clr[3] );
		DeleteObject( clr[4] );

		DeleteObject( p->banner );

		DestroyWindow(hwnd);
		return 1;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int bta_sys_init() {
	return 0;
}

void bta_sys_close() {
}

void _bta_sys_prompt(struct _bta_prompt_info *p) {
	RECT b;
	HINSTANCE hinstance = (HINSTANCE)GetWindowLong(p->parent, GWL_HINSTANCE);
	GetWindowRect(GetParent(GetParent(p->parent)), &b);

	int sh = b.bottom-b.top;
	int sw = b.right-b.left;
	int borderwidth = GetSystemMetrics(SM_CXBORDER);
	int captionheight = GetSystemMetrics(SM_CYCAPTION);
	int w=DIALOG_WIDTH+borderwidth*2;
	int h=DIALOG_HEIGHT+borderwidth+captionheight;
	int x=b.left+((sw/2)-(w/2));
	int y=b.top+((sh/2)-(h/2));

	p->win = CreateWindow(L"BetterThanAdsPrompt", L"BetterThanAds - Confirm payment",
			WS_OVERLAPPED|WS_CAPTION, x,y, w,h,
			p->parent, (HMENU)NULL, hinstance, NULL);

	if( !p->win ) {
		logmsg("error creating window\n");
		return;
	}

	ShowWindow(p->win, SW_SHOW);
	UpdateWindow(p->win);

	MSG msg;
	while( GetMessage( &msg, NULL, 0, 0 ) > 0)
	{ 
		TranslateMessage(&msg);
		DispatchMessage(&msg); 
	}
	bta_free(p);
}

void bta_sys_prompt(NPP instance, const char *error) {
	struct _bta_prompt_info *p = (struct _bta_prompt_info *)bta_malloc(sizeof(struct _bta_prompt_info)+strlen(error)+1);

	_bta_sys_info = p;
	p->pin = ((bta_info *)instance->pdata)->pin;
	p->parent= ((bta_info*)instance->pdata)->window;
	strcpy_s(p->error, strlen(error)+1, error);
	p->instance=instance;

	HINSTANCE hinstance = (HINSTANCE)GetWindowLong(p->parent, GWL_HINSTANCE);

	p->hfont = (HFONT)GetStockObject(ANSI_VAR_FONT); 
	p->cursor[0]=LoadCursor(NULL, IDC_ARROW);
	p->cursor[1]=LoadCursor(NULL, IDC_IBEAM);

	WNDCLASS wndclass;
	if( GetClassInfo(hinstance, L"BetterThanAdsPrompt", &wndclass)==0 ) {
		wndclass.lpszClassName=L"BetterThanAdsPrompt";
		wndclass.lpszMenuName=NULL;
		wndclass.cbClsExtra=0;
		wndclass.cbWndExtra=0;
		wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION); 
		wndclass.hCursor=p->cursor[0];
		wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
		wndclass.lpfnWndProc=_bta_sys_prompt_callback;
		wndclass.style=CS_HREDRAW|CS_VREDRAW;

		wndclass.hInstance=hinstance;

		if( !RegisterClass(&wndclass) ) {
			char ibuf[20];
			_itoa(GetLastError(),ibuf,10);
			logmsg("ERROR registering window class!\n   Error code: ");
			logmsg(ibuf);
			logmsg("\n");
			return;
		}
	}

	DWORD threadid;
	HANDLE promptThreadHandle = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)_bta_sys_prompt, p, 0, &threadid);
}
///////////////////////////////////////////////////////////////////////////////

void _bta_sys_draw(HWND hwnd, NPP instance) {
	bta_info *bta = (bta_info*)instance->pdata;
	PAINTSTRUCT ps;
	RECT winsize;
	HBRUSH clr;
	HFONT hFont;
	int width=0, height=0;
	int x = 0, y = 0;
	char str[128];
	sprintf_s(str, 128, "BTA: $%0.2f%s", bta->price, bta->type==1?"/mo":"");

	GetWindowRect(hwnd, &winsize);
	width=(winsize.right-winsize.left);
	height=(winsize.bottom-winsize.top);

	BeginPaint(hwnd, &ps);
	SetTextColor(ps.hdc, RGB(0xFF,0xFF,0xFF));
	SetBkColor(ps.hdc, RGB(0,0,0x55));
	clr = CreateSolidBrush(RGB(0,0,0x55));
	SelectObject(ps.hdc, clr);
	Rectangle(ps.hdc, 0,0, width, height);

	hFont = (HFONT)GetStockObject(ANSI_VAR_FONT); 
	SelectObject(ps.hdc, hFont);
	SetRect(&winsize, 0,0, width, height);
	DrawTextA(ps.hdc, str, -1, &winsize, DT_CENTER|DT_VCENTER|DT_SINGLELINE);

	EndPaint(hwnd, &ps);
}

NPP _bta_sys_npp;

LRESULT CALLBACK _bta_sys_button_callback( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) 
	{ 
		case WM_PAINT: 
			_bta_sys_draw(hwnd, _bta_sys_npp);
			return 0;
		case WM_LBUTTONDOWN:
			bta_sys_prompt(_bta_sys_npp, "");
			return 0;
		case WM_USER:
			if( wParam==42 && lParam==42 ) {
				bta_api_do_payment( _bta_sys_npp );
			}
			return 0;
	} 
	return DefWindowProc(hwnd, uMsg, wParam, lParam); 
}

void bta_sys_windowhook(NPP instance, NPWindow *npwin_new) {
	if( instance->pdata ) {
		bta_info *bta = (bta_info*)instance->pdata;

		// if closing or changing, remove old callback
		if( npwin_new==NULL || bta->window!=0 ) {
			bta->window = (HWND)0;
		}

		// install new callback
		if( npwin_new!=NULL ) {
			bta->window = (HWND)npwin_new->window;

			SetWindowLongPtr(bta->window, GWLP_WNDPROC, (LONG)_bta_sys_button_callback);
			// TODO: need to pass instance...
			_bta_sys_npp = instance;
		}
	}
}
