#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>

#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <regex.h>

#include "wsite_config.h"

extern struct entry entries[];

#include "wsite_date.h"
#include "wsite_nv.h"
#include "wsite_request.h"
#include "wsite_response.h"
#include "wsite_handler.h"
#include "wsite_log.h"
#include "wsite_send.h"
#include "wsite_readconf.h"
#include "wsite_util.h"
#include "wsite_main.h"

extern FILE    *fp_log;
char		in_hostname[NI_MAXHOST];
char		listen_port[6];
char		log_filename[20];
char	        redirect_hostname[MAX_REDIRECT_DOMAIN_NAME][NI_MAXHOST];

struct hostserv {
	char		hostname[NI_MAXHOST];
	char		servname[NI_MAXSERV];
	uint8_t		canonname:1;
	uint8_t		pad:7;
};

ssize_t		dn2sinaddr(struct sockaddr_in sinaddr[6], char *dn);
int		bind_socket_server(int *s, struct sockaddr_in *sinaddr);
ssize_t		bind_server(int *s);
int		client_hostserv(struct hostserv *hs, struct sockaddr_in *usrinaddr);
int		create_socket_server(int *s);
void		usage(char *progname);
int		accept_connection(int s, struct sockaddr_in *saddrin_client);

ssize_t
bind_server(int *s)
{
	struct sockaddr_in server_sinaddr[6];
	ssize_t		nb_serveraddr = 0;
	char		server_address_s[INET_ADDRSTRLEN];
	uint16_t	server_port_us;

	if ((nb_serveraddr = dn2sinaddr(server_sinaddr,
					in_hostname)) < 0)
		exit(0);

	while (--nb_serveraddr > -1) {
		memset(server_address_s, 0, INET_ADDRSTRLEN);

		inet_ntoa_r(server_sinaddr[nb_serveraddr].sin_addr,
			    server_address_s,
			    sizeof(struct sockaddr_in));
		fprintf(stdout, "%s has address %s\n",
			in_hostname,
			server_address_s);

		server_port_us = (uint16_t) atoi(listen_port);
		server_sinaddr[nb_serveraddr].sin_port =
			htons(server_port_us);

		server_sinaddr[nb_serveraddr].sin_len =
			sizeof(struct sockaddr_in);
		server_sinaddr[nb_serveraddr].sin_family = AF_INET;

		if (bind_socket_server(s,
				       &server_sinaddr[nb_serveraddr]) > 0)
			break;
	}

	if (nb_serveraddr == -1) {
		printf("Failed to find a server address to bind the socket.\n");
		exit(0);
	}
	strcpy(log_filename, server_address_s);

	return nb_serveraddr;
}

int
accept_connection(int s, struct sockaddr_in *saddrin_client)
{
	int		infdusr;
	socklen_t	addrlen = sizeof(struct sockaddr_in);
	const struct timeval tmvrcv = {2, 40000};

	memset(saddrin_client, 0, sizeof(struct sockaddr_in));

	infdusr = accept4(s, (struct sockaddr *)saddrin_client,
			  &addrlen,
			  SOCK_CLOEXEC);
	if (infdusr < 0) {
		fprintf(stderr, "Error in file %s at line %d with accept (%s).\n",
			__FILE__,
			__LINE__,
			strerror(errno));
		return -1;
	}
	setsockopt(infdusr, SOL_SOCKET, SO_RCVTIMEO, &tmvrcv, sizeof(struct timeval));

	return infdusr;
}

int
bind_socket_server(int *s, struct sockaddr_in *sinaddr)
{
	int		ret_ecode;

	ret_ecode = bind(*s, (struct sockaddr *)sinaddr,
			 sizeof(struct sockaddr_in));
	if (ret_ecode < 0) {
		fprintf(stderr, "Can't bind server socket (%s).\n", strerror(errno));
		return -1;
	}
	return 1;
}

int
client_hostserv(struct hostserv *hs, struct sockaddr_in *usrinaddr)
{
	char	       *phostname;
	int		ecode;

	memset(hs->hostname, 0, NI_MAXHOST);
	memset(hs->servname, 0, NI_MAXSERV);

	if ((ecode = getnameinfo((struct sockaddr *)usrinaddr,
				 usrinaddr->sin_len,
				 hs->hostname,
				 NI_MAXHOST,
				 hs->servname,
				 NI_MAXSERV,
				 NI_NUMERICSERV)) != 0) {
		fprintf(stderr, "Error in %s at %d with getnameinfo (%s).\n",
			__FILE__,
			__LINE__,
			gai_strerror(ecode));
		return -1;
	}
	phostname = hs->hostname;

	while (isdigit(*phostname) != 0 || *phostname == '.')
		if (*phostname == '\0')
			break;
		else
			++phostname;

	if (*phostname == '\0')
		hs->canonname = 0;
	else
		hs->canonname = 1;

	return 1;
}

ssize_t
dn2sinaddr(struct sockaddr_in sinaddr[6], char *dn)
{
	struct addrinfo *res;
	struct addrinfo	hints;
	int		gai_ecode;
	int		cnt;

	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_IP;

	gai_ecode = getaddrinfo(dn, NULL, &hints, &res);
	if (gai_ecode != 0) {
		fprintf(stderr, "Error in %s at %d with getaddrinfo (%s).\n",
			__FILE__,
			__LINE__,
			gai_strerror(gai_ecode));
		return -1;
	}
	cnt = 0;

	do {
		memset(&sinaddr[cnt], 0, sizeof(struct sockaddr_in));
		memcpy(&sinaddr[cnt], res->ai_addr, sizeof(struct sockaddr_in));
		res = res->ai_next;
		cnt++;
	} while (res != NULL);

	freeaddrinfo(res);

	return cnt--;
}

int
create_socket_server(int *s)
{
	*s = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
	if (*s < 0) {
		fprintf(stderr, "Can't create server socket (%s).\n", strerror(errno));
		return -1;
	}
	return 1;
}

void
usage(char *progname)
{
	fprintf(stdout, "Usage: %s \n", progname);
	fprintf(stdout, "       The server will listen\n");
	fprintf(stdout, "       on the first ip address of %s with port 80\n",
		in_hostname);
	return;
}


#define CLOSE_CONN(S) do { 							\
		      	  shutdown(S, SHUT_RDWR); 				\
			  close(S);						\
		      } while(0)

int
main(int argc, char *argv[])
{
	struct sockaddr_in usrinaddr;
	struct header_nv hdr_nv[HEADER_NV_MAX_SIZE];
	uint8_t		usrfdcnt = 0;
	struct wsite_response wsresp;
	struct request_line rline;
	int32_t		infdusr;
	int32_t		socket_server[14];
	int_fast16_t	i;
	struct hostserv	hs;
	regex_t		reg1, reg2;
	int		ch;
	int		s;

	memset(log_filename, 0, 20);
	memset(&reg1, 0, sizeof(regex_t));
	memset(&reg2, 0, sizeof(regex_t));

	if (regcomp(&reg1, "(.org|.net|.it|.es|.eu|.com|.fr|.arpa|.us|.de|.uk|.se|.nl|.io)$",
		    REG_EXTENDED | REG_ICASE | REG_NOSUB) != 0) {
		printf("regcomp() error\n");
		return -1;
	}
	if (regcomp(&reg2, "(ipip.net|frontiernet.net|grapeshot.co.uk|hwclouds-dns.com|"
			   "rima-tde.net|tedata.net|ahrefs.com|bezeqint.net|"
		 	   "googleusercontent.com|semrush.com|sogou.com|yandex.com|"
			   "kyivstar.net)$",
		    REG_EXTENDED | REG_ICASE | REG_NOSUB) != 0) {
		printf("regcomp() error\n");
		return -1;
	}
	signal(SIGPIPE, SIG_IGN);

	while ((ch = getopt(argc, argv, "h")) != -1) {
		switch (ch) {
		case 'h':
			usage(argv[0]);
			return 0;
		default:
			break;
		}
	}

	if (read_wsite_conf("wsite.conf") < 0)
		return -1;

	for (s = 0; s < (int)sizeof(socket_server); s++)
		if (create_socket_server(&socket_server[s]) < 0)
			return -1;

	for (i = 0; bind_server(&socket_server[i]) > 0; ++i)
		if (fork() == 0)
			break;

	listen(socket_server[i], 10);

	strcat(log_filename, WSITE_LOG_FILENAME_EXT);

	if (fopen_log(log_filename) < 0)
		return -1;

	log_filename[strlen(log_filename) - 4] = '\0';

	log_listen(log_filename, 80);

	fprintf(stdout, "Listening on %s with port %d\n", log_filename, 80);

	log_filename[strlen(log_filename) - 4] = '.';

	s = socket_server[i];

	for (;;) {
		if (((usrfdcnt++ + 7) % 44) == 0) {
			system("clear");
			usrfdcnt = 0;
		}

		infdusr = accept_connection(s, &usrinaddr);
		if (infdusr < 0) {
			CLOSE_CONN(infdusr);
			continue;
		}

		memset(&hs, 0, sizeof(struct hostserv));
		client_hostserv(&hs, &usrinaddr);
		log_client_hostserv_name(hs.hostname);

		if (hs.canonname) {
			if (regexec(&reg1, hs.hostname, 0, NULL, 0) != 0 ||
			    regexec(&reg2, hs.hostname, 0, NULL, 0) == 0) {
				fprintf(fp_log, "blocked_request\n");
				CLOSE_CONN(infdusr);
				continue;
			}
		}

		printf("Oncoming connection with %s:%s.\n", hs.hostname, hs.servname);
		fflush(stdout);

		memset(&rline, 0, sizeof(struct request_line));
		if (recv_request_line(infdusr, &rline) < 0) {
			fprintf(fp_log, "bad_request\n");
			CLOSE_CONN(infdusr);
			continue;
		}
		if (http_version_ok(rline.version) < 0) {
			fprintf(fp_log, "bad_request\n");
			CLOSE_CONN(infdusr);
			continue;
		}
		if (absoluteURI_in(rline.resource) > 0)
			rline.absURIin = 1;
		
		if (facebook(rline.resource) > 0)
			rline.facebook = 1;

		if ((i = match_resource(rline.resource)) < 0) {
			fprintf(fp_log, "bad_request\n");
			CLOSE_CONN(infdusr);
			continue;
		}
		memset(&wsresp, 0, sizeof(struct wsite_response));
		wsresp.entry = &entries[i];

		if (strcmp(rline.method, HTTP_METHOD_GET) == 0) {
			wsresp.send_content = 1;
		} else if (strcmp(rline.method, HTTP_METHOD_HEAD) == 0) {
			wsresp.send_content = 0;
		} else {
			fprintf(fp_log, "bad_request\n");
			CLOSE_CONN(infdusr);
			continue;
		}

		memset(&hdr_nv, 0, sizeof(struct header_nv) * HEADER_NV_MAX_SIZE);
		if (recv_header_nv(infdusr, hdr_nv) < 0) {
			fprintf(fp_log, "bad_request\n");
			CLOSE_CONN(infdusr);
			continue;
		}
		if (wsite_handle(&wsresp, hdr_nv, &rline) > 0) {
			if (wsite_send(infdusr, &wsresp) > 0) {
				fprintf(fp_log, "%s:sent_ok\n", wsresp.entry->resource);
			} else {
				fprintf(fp_log, "bad_request\n");
			}
		} else {
			fprintf(fp_log, "bad_request\n");
		}

		fflush(fp_log);
		CLOSE_CONN(infdusr);
		
	}	

}
