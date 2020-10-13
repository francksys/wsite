#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "wsite_log.h"

struct tm *tmv;
FILE *fp_log;
char log_date_now[56];

static void set_log_date_now(void);
void log_client_hostserv_name(char *hostserv_name);
int log_listen(const char *server_addr, const unsigned short server_port);


/* Set log_date_now to now in a short log format */

static void
set_log_date_now(void)
{
	time_t now;

	memset(&now, 0, sizeof(time_t));
	time(&now);
	tmv = gmtime(&now);

	memset(log_date_now, 0, 56);
	strftime(log_date_now, 56, "%d.%b.%Y:%T", tmv);

	return;
}

/* Log client addr and request resource info */

void
log_client_hostserv_name(char *hostserv_name)
{
	set_log_date_now();
	fprintf(fp_log, "%s:oncoming:%s:", log_date_now, hostserv_name);
	fflush(fp_log);
	return;
}

/* Log the server is listening to */

int
log_listen(const char *server_addr, const unsigned short server_port)
{
	int retv;

	retv = fprintf(fp_log, "\nListening on address %s with port %d\n",
			       server_addr,
			       server_port);

	if (retv < 0) {
		fprintf(stderr, "Error in file %s at line %d with fprintf (%s)\n",
				__FILE__,
				__LINE__,
				strerror(errno));
		return -1;
	}

	fflush(fp_log);

	return 1;
}

/* fopen log file, return -1 if unable to */

int
fopen_log(const char *filename)
{
	fp_log = fopen(filename, "a+");
	if (fp_log == NULL) {
		fprintf(stderr, "Can't open log filename (%s) in %s at %d (%s).\n",
				 filename,
				 __FILE__,
				 __LINE__,
				strerror(errno));
		return -1;
	}

	return 1;
}

/* Flush and close log file */

void
fclose_log(void)
{
	fflush(fp_log);
	fclose(fp_log);
	return;
}
