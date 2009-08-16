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
// BetterThanAds NPAPI Plugin - Mac OS X Window code
//

#include <unistd.h>
#include <stdio.h>
#include <string.h>

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

// cancel, ok buttons
const char *buttontext[2] = { "Cancel", "OK" };
//XRectangle buttons[2] = { {10,DIALOG_HEIGHT-35, 150, 25}, {DIALOG_WIDTH-160, DIALOG_HEIGHT-35, 150, 25} };
//XRectangle pinbox = {260,67, 394-260,86-67};

struct _bta_prompt_info {
	CGrafPtr savePort;
	RGBColor savedFront, savedBack;

	CGrafPtr port;

	NPP instance;
	char *pin;
	char error[1];
};

/////////////////////////////////////////////////////////////////////////////////////

int bta_sys_init() {
	return 0;
}

void bta_sys_close() {
}

////////////////////////////////////////////////////////////
// BTA button drawing code
////////////////////////////////////////////////////////////

void bta_sys_windowhook(NPP instance, NPWindow *npwin_new) {
	logmsg("bta_sys_windowhook()\n");
	if( instance->pdata ) {
		bta_info *bta = (bta_info*)instance->pdata;

		// if closing or changing, remove old callback
		if( npwin_new==NULL && bta->window!=0 ) {
		}

		// install new callback
		if( npwin_new != NULL ) {
		}
	}
}

////////////////////////////////////////////////////////////
// BTA prompt window code
////////////////////////////////////////////////////////////

// creates a prompt window, shows it, and starts a pthread to process events
void bta_sys_prompt(NPP instance, const char *error) {
	logmsg("bta_sys_prompt()\n");

}
