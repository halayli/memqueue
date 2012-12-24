/*
  memqueue
  (C) 2011  Hasan Alayli <halayli@gmail.com>
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  http_parser.c
*/
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "log.h"
#include "http_impl.h"

int
http_parse_req_line(http_cli_t *c)
{
    typedef enum {st_host = 0,
        st_uri,
        st_proto,
        st_port} st_state;
    int uri_start = 0;
    int port_start = 0;
    int host_start = 0;
    int proto_start    = 0;
    char *hdr = c->req.hdr.hdr;
    int len = c->req.hdr.hdr_len;
    st_state state = st_host;
    int p = 0;
    c->req.port = HTTP_DEFAULT_SERVER_PORT;

    if (len < 7)
        return HTTP_ERR_INV_REQ_LINE;

    if (strncasecmp(hdr, "GET", 3) == 0) {
        c->req.method = HTTP_GET;
        c->req.method_str = "GET";
        p += 3;
    } else if (strncasecmp(hdr, "PUT", 3) == 0) {
        c->req.method = HTTP_PUT;
        c->req.method_str = "PUT";
        p += 3;
    } else if (strncasecmp(hdr, "DELETE", 6) == 0) {
        c->req.method = HTTP_DELETE;
        c->req.method_str = "DELETE";
        p += 6;
    } else if (strncasecmp(hdr, "POST", 4) == 0) {
        c->req.method = HTTP_POST;
        c->req.method_str = "POST";
        p += 4;
    } else if (strncasecmp(hdr, "HEAD", 4) == 0) {
        c->req.method = HTTP_HEAD;
        c->req.method_str = "HEAD";
        p += 4;
    } else  {
        return HTTP_ERR_INV_METHOD;
    }

    while (p < len && isspace(hdr[p]))
        p++;
    if (p == len)
        return HTTP_ERR_INV_REQ_LINE;

    state = st_uri;

    while (p < len) {
        switch (state) {
        case st_host:
            if (HTTP_IS_HOST_TERMINATOR((unsigned char)hdr[p])) {
                if (host_start == p)
                    return HTTP_ERR_INV_HOST;
                if ((p - host_start) > HTTP_MAX_HOST_LEN)
                    return HTTP_ERR_INV_HOST;
                if ((c->req.host = \
                    strndup(&hdr[host_start], p - host_start)) == NULL) {
                    LOG_ERROR(NOMEM "not enough memory to copy host");
                    return HTTP_FAIL;
                }
                if (hdr[p] == ':') {
                    state = st_port;
                    p++;
                    goto st_port;
                } else {
                    state = st_uri;
                    goto st_uri;
                }
            }

            if (!HTTP_IS_HOST_TOKEN(hdr[p]))
                return HTTP_ERR_INV_HOST;
            p++;
            break;
        case st_port:
        st_port:
            port_start = p;
            while (p < len && HTTP_IS_DIGIT(hdr[p]))
                p++;

            if (port_start == p)
                return HTTP_ERR_INV_PORT;
            c->req.port = http_strtol(&hdr[port_start], p - port_start, 10);
            if (c->req.port > 65534 || c->req.port < 1)
                return HTTP_ERR_INV_PORT;
            state = st_uri;
            goto st_uri;
        case st_uri:
        st_uri:
            uri_start = p;

            while (p < len && !isspace(hdr[p]))
                p++;

            if (p == len)
                return HTTP_ERR_INV_REQ_LINE;
            if ((c->req.uri = strndup(&hdr[uri_start], p - uri_start)) == NULL)
                abort();
            while (p < len && isspace(hdr[p])) {
                p++;
            }
            state = st_proto;
            goto st_proto;
        case st_proto:
        st_proto:
            proto_start = p;

            while (p < len && !HTTP_IS_CR_OR_LF(hdr[p]))
            p++;

            if (p == len){
                return HTTP_ERR_INV_REQ_LINE;
            }

            if ((p - proto_start) != 8)
                return HTTP_ERR_INV_PROTO;

            if (strncasecmp(&hdr[proto_start], "HTTP/1.1", 8) == 0) {
                c->req.hdr.http11 = 1;
            } else if (
                strncasecmp(&hdr[proto_start], "HTTP/1.0", 8) == 0) {
                c->req.hdr.http11 = 0;
            } else {
                return HTTP_ERR_INV_PROTO;
            }

            while (p < len && hdr[p] == '\0')
                p++;

            c->req.hdr.hdr[p - 1] = '\0';
            c->req.hdr.hdr_start = p;

            return 0;
        }
    }

    return HTTP_ERR_INV_REQ_LINE;
}

int
http_parse_req_hdr(http_cli_t *c)
{
    int ret = 0;
    char *tmp = NULL, *p = NULL;
    int i = 0;

    for (i = 0; i < c->req.hdr.hdr_len; i++)
        if (c->req.hdr.hdr[i] == '\0')
            return HTTP_ERR_INV_HDR;

    if ((ret = http_parse_req_line(c)) != 0)
        return ret;
    if ((ret = http_parse_hdr(c)) != 0)
        return ret;

    tmp = (char *)h_get(c->req.hdr.hdrs, "content-length");
    if (tmp)
        c->req.hdr.cnt_len = strtol(tmp, NULL, 10);
    tmp = (char *)h_get(c->req.hdr.hdrs, "transfer-encoding");
    if (tmp)
        c->req.hdr.chunked = 1;

    
    tmp = (char *)h_get(c->req.hdr.hdrs, "host");
    if (tmp) {
        if ((p = strchr(tmp, ':')) != NULL) {
            if ((p - tmp) > HTTP_MAX_HOST_LEN)
                return HTTP_ERR_INV_HOST;
            if (c->req.host)
                free(c->req.host);
            if ((c->req.host = strndup(tmp, p - tmp)) == NULL) {
                LOG_ERROR(NOMEM "not enough memory to copy host");
                return HTTP_FAIL;
            }
            c->req.port = strtol(p, NULL, 10);
        } else {
            if (c->req.host)
                free(c->req.host);
            if ((c->req.host = strndup(tmp, p - tmp)) == NULL) {
                LOG_ERROR(NOMEM "not enough memory to copy host");
                return HTTP_FAIL;
            }
        }
    }



    return 0;
}

int
http_parse_hdr(http_cli_t *cli)
{
    int i = 0;
    char *p = NULL, *q = NULL;
    int start = cli->req.hdr.hdr_start;

    for (i = cli->req.hdr.hdr_start; i < cli->req.hdr.hdr_len; i++) {
        if (cli->req.hdr.hdr[i] == '\n') {
            /* check if hdr is continuing on new line */
            if ((cli->req.hdr.hdr_len - i) && !isspace(cli->req.hdr.hdr[i + 1]))
                cli->req.hdr.hdr[i] = '\0';
            else
                continue;

            /* terminate at \r if it exist */
            if (i && cli->req.hdr.hdr[i - 1] == '\r')
                cli->req.hdr.hdr[i - 1] = '\0';

            p = strchr(&cli->req.hdr.hdr[start], ':');
            if (p == NULL) {
                start = i + 1;
                continue;
            }

            *p = '\0';
            for (q = &cli->req.hdr.hdr[start]; *q; q++)
                *q = tolower(*q);
            p++;
            while (*p && !isspace(*p))
                p++;
            h_insert(cli->req.hdr.hdrs, &cli->req.hdr.hdr[start], p + 1);
            start = i + 1;
        }
    }

    return 0;
}
