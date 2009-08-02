// MS Windows prompt dialog
// and threads

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

struct _bta_sys {
	HWND browser_win;

	HANDLE thread;
	HWND win;

	HFONT hfont;
	HCURSOR cursor[2];
	HANDLE banner;

	char pin[256];
	char *message;
	NPP instance;
} bta_sys;

// cancel, ok buttons
const char *buttontext[2] = { "Cancel", "OK" };
// RECTs are +1 on bottom and right because MS doesn't draw the last pixel
RECT buttons[2] = {
  {10,DIALOG_HEIGHT-35, 161, DIALOG_HEIGHT-9}, 
  {DIALOG_WIDTH-160, DIALOG_HEIGHT-35, DIALOG_WIDTH-9, DIALOG_HEIGHT-9}
};
RECT pinbox = {260,67, 395,87};

// TODO: should probably be a semaphore
int prompt_running=0;

void _win_error(const char *msg) {
	DWORD err=GetLastError();
	char ibuf[20];
	if( err==0 ) return;
	_itoa(err,ibuf,10);

	logmsg("ERROR code ");
	logmsg(ibuf);
	logmsg(" when  ");
	logmsg(msg);
	logmsg("\n");
}

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
	SelectObject(hdc, CreatePalette(pal));
	RealizePalette(hdc);

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

LRESULT CALLBACK bta_sys_thread( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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
			bta_sys.pin[0]=0;

			clr[0] = CreateSolidBrush(RGB(0,0,0x55));
			clr[1] = CreateSolidBrush(RGB(0x55,0x55,0x99));
			clr[2] = CreateSolidBrush(RGB(0xdd,0xdd,0xff));
			clr[3] = CreateSolidBrush(RGB(0xff,0xff,0xff));
			clr[4] = CreateSolidBrush(RGB(0xff,0xcc,0xcc));
			hFont = (HFONT)GetStockObject(ANSI_VAR_FONT); 

			// load banner image
			dc = GetDC(hwnd);
			bta_sys.banner = XPMLoadBitmap(dc, dialog_xpm);
			ReleaseDC(hwnd, dc);			

			return DefWindowProc(hwnd, uMsg, wParam, lParam);
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
					SetCursor(bta_sys.cursor[1]);
				}
			} else {
				if( over==OVER_PINBOX )
					SetCursor(bta_sys.cursor[0]);
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
				SetCursor(bta_sys.cursor[1]);
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
						 bta_sys.pin[pinlen++]=(char)wParam;
						 bta_sys.pin[pinlen]=0;
						 break;
			}

			InvalidateRect(hwnd, NULL, FALSE);
			UpdateWindow(hwnd);
			break;
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);

	}
    // can't be done if no pin
	if( done==1 && bta_sys.pin[0]==0 ) done=0;

	if( redraw ) {
		redraw=0;

		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);

		// draw banner
		HDC hdcBanner = CreateCompatibleDC(ps.hdc);
		SelectObject(hdcBanner, bta_sys.banner);
		BitBlt(ps.hdc,0,0,DIALOG_WIDTH, DIALOG_BANNER_HEIGHT, hdcBanner,0,0,SRCCOPY);

		// draw pinbox
		if( prompt_running==2 )
			FillRect(ps.hdc, &pinbox, clr[4]);
		else if( over==OVER_PINBOX ) 
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

		RECT descRect;	
		// TODO: get from instance
		LPCWSTR message = L"I agree to pay some money to some place. This is another sentence just to make the text wrap.";
		SetTextColor(ps.hdc, RGB(0,0,0));
		SetBkColor(ps.hdc, RGB(0xFF,0xFF,0xFF));
		SetRect(&descRect, 5,110, DIALOG_WIDTH-5, DIALOG_HEIGHT-40);
		DrawText(ps.hdc, message, -1, &descRect, DT_LEFT|DT_TOP|DT_WORDBREAK);

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
		if( done==1 ) {
			bta_api_got_pin(bta_sys.instance, bta_sys.pin);
		} else {
			bta_api_got_pin(bta_sys.instance, "x");
		}
		prompt_running=0;
		DestroyWindow(hwnd);
		return 1;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int bta_sys_init(BTA_SYS_WINDOW pwin) {
	if( ((int)pwin)==0 || bta_sys.browser_win!=NULL ) return 0;
	logmsg("bta_sys_init()\n");

	bta_sys.browser_win=GetParent(GetParent(pwin));
	bta_sys.hfont = (HFONT)GetStockObject(ANSI_VAR_FONT); 
	bta_sys.cursor[0]=LoadCursor(NULL, IDC_ARROW);
	bta_sys.cursor[1]=LoadCursor(NULL, IDC_IBEAM);

	// create a transient dialog class, parented to pwin
	WNDCLASS wndclass;

	wndclass.lpszClassName=L"BetterThanAdsPrompt";
	wndclass.lpszMenuName=NULL;
	wndclass.cbClsExtra=0;
	wndclass.cbWndExtra=0;
	wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION); 
	wndclass.hCursor=bta_sys.cursor[0];
	wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndclass.lpfnWndProc=bta_sys_thread;
	wndclass.style=CS_HREDRAW|CS_VREDRAW;

	wndclass.hInstance=(HINSTANCE)GetWindowLong(bta_sys.browser_win, GWL_HINSTANCE);

	if( !RegisterClass(&wndclass) ) {
		char ibuf[20];
		_itoa(GetLastError(),ibuf,10);
		logmsg("ERROR registering window class!\n   Error code: ");
		logmsg(ibuf);
		logmsg("\n");
		return 1;
	}

	return 0;
}

void bta_sys_close() {
//	DestroyWindow(bta_sys.win);
}

void _bta_sys_prompt(NPP instance, char *message) {
	RECT b;
	GetWindowRect(bta_sys.browser_win, &b);

	int sh = b.bottom-b.top;
	int sw = b.right-b.left;
	int borderwidth = GetSystemMetrics(SM_CXBORDER);
	int captionheight = GetSystemMetrics(SM_CYCAPTION);
	int w=DIALOG_WIDTH+borderwidth*2;
	int h=DIALOG_HEIGHT+borderwidth+captionheight;
	int x=b.left+((sw/2)-(w/2));
	int y=b.top+((sh/2)-(h/2));

	HINSTANCE hinstance = (HINSTANCE)GetWindowLong(bta_sys.browser_win, GWL_HINSTANCE);

	bta_sys.win = CreateWindow(L"BetterThanAdsPrompt", L"BetterThanAds - Confirm payment",
		WS_OVERLAPPED|WS_CAPTION, x,y, w,h,
		bta_sys.browser_win, (HMENU)NULL, hinstance, NULL);

	if( !bta_sys.win ) {
		logmsg("error creating window\n");
		return;
	}

	ShowWindow(bta_sys.win, SW_SHOW);
	UpdateWindow(bta_sys.win);

	MSG msg;
	while( GetMessage( &msg, NULL, 0, 0 ) > 0)
    { 
       TranslateMessage(&msg);
       DispatchMessage(&msg); 
    }
	DestroyWindow(bta_sys.win);
}

void bta_sys_prompt(NPP instance, char *message) {
	DWORD threadid;

	logmsg("bta_sys_prompt()\n");
	if( prompt_running ) return;
	prompt_running=1;
	bta_sys.instance=instance;

	HANDLE promptThreadHandle = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)_bta_sys_prompt, NULL, 0, &threadid);
}

void bta_sys_error(NPP instance, char *message) {
	DWORD threadid;

	logmsg("bta_sys_error()\n");
	if( prompt_running ) {
		prompt_running=2;
		return;
	}

	prompt_running=2;
	bta_sys.instance=instance;

	HANDLE promptThreadHandle = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)_bta_sys_prompt, NULL, 0, &threadid);
}

void _bta_sys_draw(HWND hwnd) {
	PAINTSTRUCT ps;
	RECT winsize;
	HBRUSH clr;
	HFONT hFont;
	LPCWSTR str = L"BTA: 25c/mo"; // TODO: get from instance...
	int width=0, height=0;
	int x = 0, y = 0;

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
	DrawText(ps.hdc, str, -1, &winsize, DT_CENTER|DT_VCENTER|DT_SINGLELINE);

	EndPaint(hwnd, &ps);
}

LRESULT CALLBACK _bta_sys_draw_callback( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) 
    { 
        case WM_PAINT: 
  		    _bta_sys_draw(hwnd);
					return 0;
				case WM_LBUTTONDOWN:
					bta_api_clicked(bta_sys.instance);
					return 0;
    } 
    return DefWindowProc(hwnd, uMsg, wParam, lParam); 
}

void bta_sys_draw(NPP instance, NPWindow *npwin) {
	bta_sys.instance=instance;

	bta_sys_init((HWND)npwin->window);

	// add callback to window (ignore old callback)
    SetWindowLongPtr((HWND)npwin->window, GWLP_WNDPROC, (LONG)_bta_sys_draw_callback);
}
/////////////////////////////////////////////////////////////////////////////////////

HANDLE bta_sys_threadHandle;
HANDLE bta_sys_dataready, bta_sys_dataload;
int __is_running=0;

void bta_sys_start_apithread() {
	DWORD threadid;

	__is_running=1;
	bta_sys_dataready = CreateSemaphore(NULL, 0, 1, NULL);
	bta_sys_dataload  = CreateSemaphore(NULL, 1, 1, NULL);

	bta_sys_threadHandle = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)bta_api_thread, NULL, 0, &threadid);
}
void bta_sys_stop_apithread() {
	__is_running=0;
	ReleaseSemaphore(bta_sys_dataready, 1, NULL);
	WaitForSingleObject(bta_sys_threadHandle, INFINITE);
	CloseHandle(bta_sys_dataload);
	CloseHandle(bta_sys_dataready);
}

int  bta_sys_is_running() {
	return __is_running;
}

void bta_sys_wait_dataready() {
	WaitForSingleObject(bta_sys_dataready, INFINITE);
}
void bta_sys_post_dataready() {
	ReleaseSemaphore(bta_sys_dataready, 1, NULL);
}

void bta_sys_lock_dataload() {
	WaitForSingleObject(bta_sys_dataload, INFINITE);
}
void bta_sys_unlock_dataload() {
	ReleaseSemaphore(bta_sys_dataload, 1, NULL);
}
