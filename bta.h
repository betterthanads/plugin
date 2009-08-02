// BetterThanAds NPAPI Plugin Header file

#define BTA_BUFFER_SIZE 102400

#define BTA_ID_LENGTH 19
#define BTA_PAGEVIEWS 100

// TODO: MOVE TO https://
#define BTA_API_PAGEVIEWS    "http://api.betterthanads.com/pageviews/"
#define BTA_API_PAYMENT      "http://api.betterthanads.com/payment/"
//#define BTA_API_PAGEVIEWS    "http://127.0.0.1:18080/api/pageviews/"
//#define BTA_API_PAYMENT      "http://127.0.0.1:18080/api/payment/"

extern const char *bta_prompt_result;

// np-bta.c
void logmsg(const char *msg);
void bta_prompt_gotpin(const char *pin);
void bta_prompt_error();
void _bta_post_data(NPP inst, const char *url, const char *data, const char *target);
void bta_post_data(NPP inst, const char *url, const char *data);

void *bta_malloc(int size);
void  bta_free(void *ptr);

// bta_api.c
NPBool bta_init();
void bta_close();
void bta_gotURL(NPP inst, const char *url, char *resp);
void bta_count_site(NPP inst, const char *tag);
void bta_do_payment(NPP inst, const char *site, const char *pin, float price, NPBool recurring, const char *posturl, const char *description, const char *check);
void bta_set_user(NPP inst, const char *user_token);

// bta_xwin.c, bta_osx.c, bta_win.c
int bta_prompt(const char *message, void *parent_win);
