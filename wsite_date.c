#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "wsite_date.h"

extern struct tm     *tmv;
unsigned char http_date_now[HTTP_STRING_DATE_SIZE];


/* Set http_date_now to now (HTTP date format) */

int
httpdate_now(void)
{

	memset(http_date_now, 0, HTTP_STRING_DATE_SIZE);
	if (strftime((char*)http_date_now, HTTP_STRING_DATE_SIZE,
					   "%a, %d %b %Y %H:%M:%S GMT", tmv) == 0)
		return -1;

	return 1;
}

/* Transforms a time_t value intime to HTTP date format */

int
time_to_httpdate(char *http_date, time_t *intime)
{
	struct tm *intmv;

	intmv = gmtime(intime);

	memset(http_date, 0, HTTP_STRING_DATE_SIZE);

	if (strftime((char*)http_date, HTTP_STRING_DATE_SIZE,
				       "%a, %d %b %Y %H:%M:%S GMT", intmv) == 0)
		return -1;

	return 1;
}

/* Create an Expires value. (This value is always now + one year) */

void
set_expires(char *value)
{
	++(tmv->tm_year);
	strftime(value, HTTP_STRING_DATE_SIZE, "%a, %d %b %Y %H:%M:%S GMT", tmv);
	--(tmv->tm_year);
	return;
}
