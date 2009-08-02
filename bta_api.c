#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(XULRUNNER_SDK)
#include <npapi.h>
#elif defined(WEBKIT_DARWIN_SDK)
#include <Webkit/npapi.h>
#endif

#include "bta.h"

static char *BTA_DATAFILE;
static char *bta_payment_posturl=NULL;
static char bta_user[ BTA_ID_LENGTH ];
static char *bta_pv_buf;
static unsigned int bta_pv_len=0;

NPBool bta_init() {
  FILE *bta_fp = NULL;
#ifdef _WINDOWS
	const char *filename = "betterthanads.dat";
	char *dir = getenv("APPDATA");
#else
	const char *filename = ".betterthanads";
	char *dir = getenv("HOME");
#endif
	int len = strlen(dir)+strlen(filename)+10;
	BTA_DATAFILE = (char *)bta_malloc(len);
	if( BTA_DATAFILE==NULL ) return FALSE;
	bta_pv_buf = (char *)bta_malloc( (BTA_ID_LENGTH+1+5)*BTA_PAGEVIEWS );
	if( bta_pv_buf==NULL ) {
		bta_free(BTA_DATAFILE);
		return FALSE;
	}

	sprintf(BTA_DATAFILE, "%s/%s", dir, filename);
	//fprintf(stderr, "data stored in '%s'\n", BTA_DATAFILE);

	bta_fp = fopen(BTA_DATAFILE, "r+");
	if( !bta_fp ) {
		bta_free(bta_pv_buf);
		bta_free(BTA_DATAFILE);
		return FALSE;
	}

	if( fscanf(bta_fp, "user=%19s", bta_user) != 1 ) {
		bta_free(bta_pv_buf);
		bta_free(BTA_DATAFILE);
		fclose(bta_fp);
		return FALSE;
	}
	sprintf(bta_pv_buf, "user=%19s", bta_user);
	bta_pv_len=strlen(bta_pv_buf);
	while( !feof(bta_fp) && bta_pv_len<BTA_BUFFER_SIZE ) {
		bta_pv_len+=fread(bta_pv_buf+bta_pv_len, 1, BTA_BUFFER_SIZE-bta_pv_len, bta_fp);
	}
	bta_pv_buf[bta_pv_len]=0;
	fclose(bta_fp);
	return TRUE;
}

void bta_close() {
	FILE *bta_fp = fopen(BTA_DATAFILE, "r+");
	fseek(bta_fp, 0, SEEK_SET);
	fputs(bta_pv_buf, bta_fp);
	fclose(bta_fp);

	bta_free(bta_pv_buf);
	bta_free(BTA_DATAFILE);
	if( bta_payment_posturl ) bta_free(bta_payment_posturl);
}

//////////////////////////////
// url notifications
//////////////////////////////

// gets notification that url completed
void bta_gotURL(NPP inst, const char *url, char *resp) {
	logmsg("np-bta: NP_URLNotify: ");

	if( !url ) return;
	logmsg(url);
	logmsg("\n");

	if( strcmp(url, BTA_API_PAGEVIEWS)==0 ) {
		if( strcmp(resp, "OK")==0 ) {
			// truncate file and cache
			FILE *bta_fp = fopen(BTA_DATAFILE, "w+");
			fprintf(bta_fp, "user=%19s", bta_user);
			fclose(bta_fp);
			bta_pv_buf[25]=0;
			bta_pv_len=strlen(bta_pv_buf);
			fprintf(stderr, "BetterThanAds: Posted pageviews successfully\n");
		} else {
			fprintf(stderr, "BetterThanAds: Remote Error posting pageviews: \n%s\n", resp);
		}

	} else if( strcmp(url, BTA_API_PAYMENT)==0 ) {
		if( strncmp(resp, "bta_token=", 9)==0 ) {
			// post token to postback url
			_bta_post_data(inst, bta_payment_posturl, resp, "_parent");
		} else {
			//TODO: error dialog
			bta_prompt_error();
		}
	}
}

////////////////////////////////////////////////////////////

void bta_count_site(NPP inst, const char *tag) {
	static int logged=0;
	char *ptr, last;
	int pos=0, count=1;
	
	if( strlen(tag)!=19 ) return;
	
	ptr=strstr(bta_pv_buf, tag);
	if( ptr==NULL ) {
		sprintf(bta_pv_buf+bta_pv_len, "&%19s=%05d", tag, 1);
		bta_pv_len=strlen(bta_pv_buf);
	}	else {
		pos=ptr-bta_pv_buf+20;
		last=bta_pv_buf[pos+5];
		sscanf(bta_pv_buf+pos, "%05d", &count);
		sprintf(bta_pv_buf+pos, "%05d", count+1);
		bta_pv_buf[pos+5]=last;
	}
	
	if( ((logged++)%BTA_PAGEVIEWS)==0 ) {
		bta_post_data(inst, BTA_API_PAGEVIEWS, bta_pv_buf);
	}
}

static char __api_buf[ BTA_BUFFER_SIZE ];
void bta_do_payment(NPP inst, const char *site, const char *pin, float price, NPBool recurring, const char *posturl, const char *description, const char *check) {

	__api_buf[0]=0;
	if( bta_payment_posturl ) bta_free(bta_payment_posturl);
	bta_payment_posturl=(char*)bta_malloc(strlen(posturl));
	strcpy(bta_payment_posturl, posturl);
	sprintf(__api_buf, "site=%19s&user=%19s&pin=%s&price=%3.2f&check=%32s&type=%s&description=%s", site, bta_user, pin, price, check, recurring?"subscription":"payment", description);
	bta_post_data(inst, BTA_API_PAYMENT, __api_buf);
}

void bta_set_user(NPP inst, const char *user_token) {
	FILE *bta_fp;

	if( strcmp(user_token, bta_user)==0 ) return;

	bta_fp = fopen(BTA_DATAFILE, "w");
	if( !bta_fp ) {
		logmsg("error opening datafile\n");
		return;
	}
	fprintf(bta_fp, "user=%19s", user_token);
	fclose(bta_fp);
	strcpy(bta_user, user_token);
	sprintf(bta_pv_buf, "user=%19s", user_token);
	bta_pv_len=strlen(bta_pv_buf);
}
