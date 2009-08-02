#include <stdio.h>
#include <string.h>

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

#define BTA_BUFFER_SIZE 2048

// TODO: MOVE TO https://
#define BTA_API_USER_TOKEN   "http://betterthanads.com/api/user_token/"
#define BTA_API_PAGEVIEWS    "http://betterthanads.com/api/pageviews/"
#define BTA_API_PAYMENT      "http://betterthanads.com/api/payment/"

static char BTA_DATAFILE[2048] = "betterthanads";
static NPNetscapeFuncs *bta_npnfuncs=NULL;
static const char *bta_payment_posturl;

void bta_init(NPNetscapeFuncs *npn) {
	bta_npnfuncs=npn;

#ifdef _WINDOWS
	sprintf(BTA_DATAFILE, "%s/betterthanads.dat", getenv("APPDATA"));
#else
	sprintf(BTA_DATAFILE, "%s/.betterthanads", getenv("HOME"));
#endif
	fprintf(stderr, "data stored in '%s'\n", BTA_DATAFILE);
}

static void logmsg(const char *msg) {
#ifdef DEBUG
#ifndef _WINDOWS
	fputs(msg, stderr);
	fflush(stderr);
#else
	FILE *out = fopen("\\np-bta.log", "a");
	fputs(msg, out);
	fclose(out);
#endif
#endif
}

//////////////////////////////
// url notifications
//////////////////////////////

// posts the urlencoded-string data to url asynchronously
void _bta_post_data(NPP inst, const char *url, const char *data, const char *target) {
	static char bta_post[ BTA_BUFFER_SIZE ];
	int len = data?strlen(data):0;
	bta_post[0]=0;
	if( len>(BTA_BUFFER_SIZE-80) ) 
		logmsg("post data too large\n");
	else if( data )
		sprintf(bta_post, "Content-Type: application/x-www-form-urlencoded\nContent-Length: %d\n\n%s", len, data);

	if( !url ) return;

	bta_npnfuncs->posturlnotify(
			inst, // instance
			url,
			target, // NULL=send response back to plugin
			strlen(bta_post),    // length of data to send
		  bta_post,  // data to send
			false,  // buf is a buffer, not a file
			NULL
		);
}
void bta_post_data(NPP inst, const char *url, const char *data) {
  _bta_post_data(inst, url, data, NULL);
}

// gets notification that url completed
void bta_gotURL(NPP inst, const char *url, char *resp) {
	char buf[1024];
	logmsg("np-bta: NP_URLNotify: ");

	if( !url ) return;
	logmsg(url);

	if( strcmp(url, BTA_API_USER_TOKEN)==0 ) {
		FILE *fp = fopen(BTA_DATAFILE, "r+");
		if( !fp ) {
			logmsg("error opening datafile\n");
			return;
		}
		fprintf(fp, "user=%32s", resp);
		fclose(fp);


	} else if( strcmp(url, BTA_API_PAGEVIEWS)==0 ) {
		if( strcmp(resp, "OK")==0 ) {
			FILE *fp = fopen(BTA_DATAFILE, "r+");
			if( !fp ) {
				logmsg("error opening datafile\n");
				return;
			}
			fscanf(fp, "user=%32s", buf);
			fclose(fp);

			// truncate file
			fp = fopen(BTA_DATAFILE, "w+");
			fprintf(fp, "user=%32s", buf);
			fclose(fp);
			fprintf(stderr, "Posted pageviews successfully\n");
		} else {
			fprintf(stderr, "Remote Error posting pageviews: \n%s\n", resp);
		}

	} else if( strcmp(url, BTA_API_PAYMENT)==0 ) {
		if( strncmp(resp, "bta_token=", 9)==0 ) {
			// post token to postback url
			_bta_post_data(inst, bta_payment_posturl, resp, "_parent");
			bta_payment_posturl=NULL;
		} else {
			//TODO: error dialog
		}
	}
}

////////////////////////////////////////////////////////////

void bta_count_site(NPP inst, const char *tag) {
	static char buf[ BTA_BUFFER_SIZE ];
	static int logged=0;
	char ttag[129], *ptr;
	FILE *fp = fopen(BTA_DATAFILE, "r+");
	int len=0, pos=0, count=1;

	if( strlen(tag)!=32 ) return;
	buf[0]=0;

	if( !fp ) {
		fp = fopen(BTA_DATAFILE, "w+");
		if( !fp ) {
			logmsg("error opening datafile\n");
			return;
		}
		fprintf(fp, "user=BEEFBEEFBEEFBEEFBEEFBEEFBEEFBEEF");
		fclose(fp);
		
		bta_post_data(inst, BTA_API_USER_TOKEN, buf);
	  
		fp = fopen(BTA_DATAFILE, "r+");
	}
	
	buf[0]=0;
	while( !feof(fp) && len<BTA_BUFFER_SIZE ) {
		len=fread(buf+len, 1, BTA_BUFFER_SIZE-len, fp);
	}
	buf[len]=0;

	ptr=strstr(buf, tag);
	if( ptr==NULL ) pos=-1;
	else pos=ptr-buf;

	if( pos>0 ) {
		sscanf(buf+pos, "%32s=%5d", ttag, &count);
		fseek(fp, pos+33, SEEK_SET);
		fprintf(fp, "%05d", count+1);
		sprintf(buf+pos, "%05d", count+1);
	} else {
		fprintf(fp, "&%32s=%05d", tag, count);
		sprintf(buf+strlen(buf), "&%32s=%05d", tag, count);
	}

	fclose(fp);
	
	if( ((logged++)%100)==0 || strlen(buf) > (BTA_BUFFER_SIZE-128) ) {
		bta_post_data(inst, BTA_API_PAGEVIEWS, buf);
	}
}

void bta_do_payment(NPP inst, const char *site, const char *pin, float price, bool recurring, const char *posturl) {
	static char buf[ BTA_BUFFER_SIZE ];
	char user_token[128];
	FILE *fp = fopen(BTA_DATAFILE, "r+");
	if( !fp ) {
		logmsg("error opening datafile\n");
		return;
	}
	fscanf(fp, "user=%32s", user_token);
	fclose(fp);

	buf[0]=0;
	sprintf(buf, "site=%32s&user=%32s&pin=%s&price=%3.2f&type=%s", site, user_token, pin, price, recurring?"subscription":"payment");
	bta_payment_posturl=posturl;
	bta_post_data(inst, BTA_API_PAYMENT, buf);
}

