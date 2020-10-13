
#define WSITE_LOG_FILENAME_EXT ".log"

extern char log_date_now[56];
extern FILE *fp_log;
extern struct tm *tmv;

int fopen_log(const char *filename);
void fclose_log(void);
void log_client_hostserv_name(char *hostserv_name);
int log_listen(const char *server_addr, const unsigned short server_port);

