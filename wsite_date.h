#include <time.h>

#define HTTP_STRING_DATE_SIZE 32

extern unsigned char http_date_now[HTTP_STRING_DATE_SIZE];

int httpdate_now(void);
int time_to_httpdate(char *http_date, time_t *intime);
void set_expires(char *value);
