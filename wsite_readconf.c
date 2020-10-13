#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include <netdb.h>

#include "wsite_readconf.h"

/* All the content of this file must be rewritten in order to be 
 * less obfuscating.
 */
extern char listen_port[6];
extern char in_hostname[NI_MAXHOST];
extern char redirect_hostname[MAX_REDIRECT_DOMAIN_NAME][NI_MAXHOST]; 

int read_wsite_conf(const char *config_filename);
int read_port(char buffer[1024]);
int read_hostname(char buffer[1024]);
int read_rdn(FILE *fp);

int read_port(char buffer[1024]) {
	char cname[23];
	char *pbuf, *pcna, *pport;

	memset(listen_port, 0, 6);
	memset(cname, 0, 23);

	for (pbuf = buffer, pcna = cname; *pbuf != ':'; *pcna++ = *pbuf++)
		if (*pbuf == '\0')
			return -1;

	if (strncmp(cname, "listen_port",
			   sizeof("listen_port") - 1) == 0) {
		for (pbuf += 2, pport = listen_port;
		     *pbuf && isdigit(*pbuf) != 0;
		     *pport++ = *pbuf++)
			if ((pport - listen_port) >= 6)
				return -1; 
	} else
		return -1;
	

	return 1;
}
 
int read_hostname(char buffer[1024]) {
	char cname[23];
	char *pbuf, *pcna, *phn;

	memset(in_hostname, 0, NI_MAXHOST);
	memset(cname, 0, 23);

	for (pbuf = buffer, pcna = cname; *pbuf != ':'; *pcna++ = *pbuf++)
		if (*pbuf == '\0')
			return -1;

	if (strncmp(cname, "hostname",
			   sizeof("hostname") - 1) == 0) {
		for (pbuf += 2, phn = in_hostname;
		     *pbuf && (*pbuf == '-' || *pbuf == '.' || isalnum(*pbuf) != 0);
		     *phn++ = *pbuf++)
			if ((phn - in_hostname) >= (NI_MAXHOST - 1))
				return -1;
	} else
		return -1;
	
	return 1;
}

int read_rdn(FILE *fp) {
	char cname[23];
	char *pbuf, *pcna, *prdn;
	char buffer[1024];
	uint8_t rdncnt;

 	memset(cname, 0, 23);	
	memset(redirect_hostname, 0, MAX_REDIRECT_DOMAIN_NAME * NI_MAXHOST); 

	rdncnt = 0;

	do {
		memset(buffer, 0, 1024);
		if (fgets(buffer, 1023, fp) == NULL)
			break;

		for (pbuf = buffer, pcna = cname;
		     pbuf && *pbuf && *pbuf != ':';
		     *pcna++ = *pbuf++)
			if ((pbuf - buffer) > 1023)
				return -3;

		if (strncmp(cname, "redirect_hostname",
				   sizeof("redirect_hostname") - 1) == 0) {
			for (pbuf += 2, prdn = redirect_hostname[rdncnt++];
			     *pbuf != '\n';
			     *prdn++ = *pbuf++);
		} else
			return -2;
	} while(rdncnt < MAX_REDIRECT_DOMAIN_NAME && !feof(fp));

	return 1;
}

int read_wsite_conf(const char *config_filename) {
	FILE *fp;
	char buffer[1024];
	int ret = 1;
	int err = 0;

	fp = fopen(config_filename, "r");
	if (fp == NULL) {
		fprintf(stderr, "Error in file %s at line %d with fopen (%s)\n",
				__FILE__,
				__LINE__,
				strerror(errno));
		return -1;
	} 

	memset(buffer, 0, 1024);
	if (fgets(buffer, 1023, fp) != NULL) {
		if (buffer[strlen(buffer) - 1] == '\n') {
			if (read_port(buffer) < 0) {
				err = -3;
			}
		} else {
			err = -2;
		}
	} else {
		ret = -1;
	}

	if (ret < 0) {
		fprintf(stderr, "Error in file %s at line %d with read_port() err: %i\n",
			__FILE__, __LINE__, err);
		fclose(fp);
		return -1;
	}

	memset(buffer, 0, 1024);
	if (fgets(buffer, 1023, fp) != NULL) {
		if (buffer[strlen(buffer) - 1] == '\n') {
			if (read_hostname(buffer) < 0) {
				ret = -3;
			}
		} else {
			ret = - 2;
		}
	} else {
		ret = -1;
	}

	if (ret < 0) {
		fprintf(stderr, "Error in file %s at line %d with read_hn() err: %i\n",
			__FILE__, __LINE__, err);
			fclose(fp);
			return -1;
	}
			

	err = read_rdn(fp);

	if (err < 0)
		fprintf(stderr, "Error in file %s at line %d with read_rdn() err: %i\n",
			__FILE__, __LINE__, err);
			
	fclose(fp);

	return ret;
}
