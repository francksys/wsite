#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <stdio.h>

#include <netinet/in.h>
#include <netdb.h>

#include "wsite_config.h"
#include "wsite_nv.h"
#include "wsite_date.h"
#include "wsite_request.h"
#include "wsite_response.h"
#include "wsite_readconf.h"
#include "wsite_util.h"
#include "wsite_handler.h"

extern char	in_hostname[NI_MAXHOST];
extern char	redirect_hostname[MAX_REDIRECT_DOMAIN_NAME][NI_MAXHOST];


/* Handle if no errors occur the following HTTP responses:
 * 	HTTP_NOT_MODIFIED
 *      HTTP_MOVED_PERMANENTLY
 *      HTTP_PARTIAL_CONTENT
 *      HTTP_OK
 */
 
int
wsite_handle(struct wsite_response *wsresp,
	     struct header_nv hdr_nv[HEADER_NV_MAX_SIZE],
	     struct request_line *rline)
{
	int		iname;
	int		maxiname;
	char		cookie[254];
	char		expires[HTTP_STRING_DATE_SIZE];
	uint8_t		absURIin;
	uint8_t		facebook;

	absURIin = rline->absURIin;
	facebook = rline->facebook;

	if (create_wanted_resource(&wsresp->wanted_resource,
				   hdr_nv,
				   wsresp->entry->type) < 0)
		return -1;

	strcat(wsresp->wanted_resource.local_resource, wsresp->entry->resource);

	/* Client request with a If-Modifed-Since header field */

	if (wsresp->wanted_resource.modified_i > 0) {
		time_t		rlm;

		memset(&rlm, 0, sizeof(time_t));

		rlm = resource_lastmod(wsresp->wanted_resource.local_resource);

		memcpy(&wsresp->wanted_resource.lastmodified_i,
		       &rlm,
		       sizeof(time_t));

		wsresp->statusline.code_h = (rlm > wsresp->wanted_resource.modified_i) ?
			HTTP_CODE_STATUS_OK : HTTP_CODE_STATUS_NOT_MODIFIED;

	} else if (wsresp->wanted_resource.range == true) {
		wsresp->statusline.code_h = HTTP_CODE_STATUS_PARTIAL_CONTENT;
	}

	/* Client request with a proxy */

	if (absURIin == 1) {
		;
	} else if ((iname = nv_find_name_client(hdr_nv, "Host")) > -1) {

		memset(cookie, 0, 254);
		memset(expires, 0, HTTP_STRING_DATE_SIZE);

		/* Check Host header value */

		if (strcasecmp(hdr_nv[iname].value.v, in_hostname) != 0) {
			int		i;

			for (i = 0;
			     redirect_hostname[i][0] != '\0';
			     i++) {
				if (strncasecmp(redirect_hostname[i],
						hdr_nv[iname].value.v,
						DOMAIN_NAME_MAX_SIZE) == 0) {
					wsresp->statusline.code_h =
						HTTP_CODE_STATUS_MOVED_PERMANENTLY;
					break;
				}
			}

			if (redirect_hostname[i][0] == '\0')
				return -1;
		} else if (facebook > 0) {
			wsresp->statusline.code_h = HTTP_CODE_STATUS_MOVED_PERMANENTLY;
		} else if (strcmp(wsresp->entry->resource, "RGPD_Ok") == 0) {

			set_expires(expires);

			strcpy(cookie, "RGPD=1; ");
			strcat(cookie, "Expires=");
			strcat(cookie, expires);

			/* RGPD is specialy handled with a MOVED_PERMANENTLY response */

			wsresp->statusline.code_h = HTTP_CODE_STATUS_MOVED_PERMANENTLY;

		} else if (strcmp(wsresp->entry->resource, "RGPD_Refus") == 0) {

			set_expires(expires);

			strcpy(cookie, "RGPD=0; ");
			strcat(cookie, "Expires=");
			strcat(cookie, expires);

			/* RGPD is specialy handled with a MOVED_PERMANENTLY response */

			wsresp->statusline.code_h = HTTP_CODE_STATUS_MOVED_PERMANENTLY;
		}
	} else {
		return -1;
	}

	create_status_response(&wsresp->statusline);
	maxiname = create_header_nv_response(wsresp->hdr_nv,
				     wsresp->wanted_resource.lastmodified_i,
					     wsresp->wanted_resource.range,
					     wsresp->entry->type,
					     cookie);

	if (wsresp->statusline.code_h == HTTP_CODE_STATUS_MOVED_PERMANENTLY) {
		int		ireferer;

		wsresp->hdr_nv[maxiname].name.wsite = "Location";


		if (strncmp(wsresp->entry->resource, "RGPD", sizeof("RGPD") - 1) == 0) {

			/* RGPD handler, Whether user consent redirect to referer
			 * with 2 cases: 1) right referer                
			 *               2) wrong referer and redirect to Accueil
			 */

			ireferer = nv_find_name_client(hdr_nv, "Referer");

			if (ireferer > 0 &&
			    strncmp(hdr_nv[ireferer].value.v, "http://", sizeof("http://") - 1) == 0 &&
			    strncmp(hdr_nv[ireferer].value.v + 7, in_hostname,
								  strlen(in_hostname)) == 0 &&
			    strlen(hdr_nv[ireferer].value.v) > 7 + strlen(in_hostname) + 2) {
				strcpy(wsresp->hdr_nv[maxiname].value.v, hdr_nv[ireferer].value.v);
				sprintf(wsresp->redirect_hypertext_note, "http://%s/%s",
					 				 hdr_nv[ireferer].value.v,
									 in_hostname);
			} else {
				char buffer[254];

				memset(buffer, 0, 254);

				strcpy(buffer, "http://");
				strcat(buffer, in_hostname);
				strcat(buffer, "/Accueil");

				strcpy(wsresp->hdr_nv[maxiname].value.v, buffer);

				sprintf(wsresp->redirect_hypertext_note,
					"<a href=\"%s\">%s</i></a>",
					wsresp->hdr_nv[maxiname].value.v,
					in_hostname);
			}
		} else {
			sprintf(wsresp->hdr_nv[maxiname].value.v, "http://%s/%s",
				in_hostname,
				wsresp->entry->resource);

			sprintf(wsresp->redirect_hypertext_note,
				"<a href=\"%s\">%s</i></a>",
				wsresp->hdr_nv[maxiname].value.v,
				in_hostname);
		}

		wsresp->send_content = 0;
	}

	return 1;
}
