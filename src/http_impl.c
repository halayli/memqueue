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

  http_impl.c
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <err.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <assert.h>
#include <inttypes.h>

#include <arpa/inet.h>
#include <netdb.h>

#include "common/queue.h"
#include "http_str.h"
#include "http_impl.h"
#include "http.h"
#include "http_bd.h"
#include "sock_easy.h"
#include "log.h"
#include <lthread.h>
#include "common/time.h"
#include <netinet/tcp.h>

int total = 0;
/************************************************************************
 * http server prototypes                                               *
 ***********************************************************************/
static int  http_recv_hdr(http_cli_t *cli);
static int  http_recv_req_hdr(http_cli_t *c);

static int  http_recv_exact(http_cli_t *cli);
static int  http_recv_chunked(http_cli_t *cli);
static int  http_recvbody(http_cli_t *cli);

static int  http_handle_cli_rd(http_cli_t *cli);
static void http_lt_cli_rd(lthread_t *lt, http_cli_t *cli);
static void http_listener(lthread_t *lt, lsn_t *lsn);
static void http_cli_new(lsn_t *lsn, int fd, struct sockaddr_in *peer_addr);

static int  http_cli_init(http_cli_t *cli);
static void http_cli_reset(http_cli_t *cli);
static void http_cli_free(http_cli_t *cli);

static void http_log_req(http_cli_t *cli);

static int  http_cli_resp_hdr_create(http_cli_t *c, struct iovec **);
static void http_handle_cli_req_err(http_cli_t *cli, int err);
static void http_set_path(http_cli_t *cli);

#define HTTP_VERSION "1.0"

static int
http_cli_resp_hdr_create(http_cli_t *c, struct iovec **_iovecs)
{
    int total_iovecs = 0;
    h_item_t *item = NULL;
    char tmp[100];
    struct iovec *iovecs = {0};
    int i = 0;

    if (c->resp.state != HTTP_HDR_BEGIN)
        return 0;

    /* 4 for the response line */
    total_iovecs = 4;

    if (c->resp.body_len) {
        sprintf(tmp, "%"PRIu64, c->resp.body_len);
        http_add_header(c, "Content-Length", tmp);
        /* 2, one for content, and for new line afterwards */
        total_iovecs+=2;
    } else {
        http_set_respcode(204);
    }

    if (c->resp.respcode == 0)
        http_set_respcode(200);

    if (h_get(c->resp.hdrs, "Server") == NULL)
        http_add_header(c, "Server", "express HTTP Server: "HTTP_VERSION);

    while ((item = h_next(c->resp.hdrs)) != NULL)
        total_iovecs += 4;

    total_iovecs += 1; /* newline at the end of hdr */

    iovecs = malloc(sizeof(struct iovec) * total_iovecs);

    if (!iovecs) {
        app_log(c->lsn->debug_log, APP_ERR,
            NOMEM "not enough memory to create resp header.");
        return -1;
    }

    i = 0;

    iovecs[i].iov_base = c->req.hdr.http11 == 1 ? "HTTP/1.1 ":"HTTP/1.0 ";
    iovecs[i].iov_len = 9;
    i++;
    iovecs[i].iov_base = c->resp.respcode_int;
    iovecs[i].iov_len = 4;
    i++;
    iovecs[i].iov_base = c->resp.respcode_str;
    iovecs[i].iov_len = strlen(c->resp.respcode_str);
    i++;
    iovecs[i].iov_base = "\n";
    iovecs[i].iov_len = 1;
    i++;

    while ((item = h_next(c->resp.hdrs)) != NULL) {
        iovecs[i].iov_base = item->key;
        iovecs[i].iov_len = strlen(item->key);
        i++;
        iovecs[i].iov_base = ": ";
        iovecs[i].iov_len = 2;
        i++;
        iovecs[i].iov_base = item->value;
        iovecs[i].iov_len = strlen(item->value);
        i++;
        iovecs[i].iov_base = "\n";
        iovecs[i].iov_len = 1;
        i++;
    }

    iovecs[i].iov_base = "\n";
    iovecs[i].iov_len = 1;

    *_iovecs = iovecs;
    i++;

    return i;
}

static int
http_recv_hdr(http_cli_t *cli)
{
    int recvd = 0;
    int hdr_len = 0;
    int ret = 0;
    char *hdr_end;
    char tmp = 0;
    int i = 0;
    cli->req.hdr.cnt_len = 0;

    while ((HTTP_MAX_HDR_LEN - recvd) > 0) {
        ret = http_recv(cli, &cli->req.hdr.hdr[recvd],
            HTTP_MAX_HDR_LEN - recvd);

        if (ret == -2)
            return HTTP_ERR_TIMEOUT_EXC;
        if (ret < 1)
            return HTTP_ERR_CLI_CLOSED;

        /* ignore whitespace before header */
        if (recvd == 0 && ret > 0) {
            i = 0;
            while (i <= ret && isspace(cli->req.hdr.hdr[i])) {
                i++;
                ret--;
            }
            if (i && ret > 0) {
                memmove(cli->req.hdr.hdr, &cli->req.hdr.hdr[i], ret);
            }
        }

        if (ret == 0)
            continue;

        recvd += ret;
        tmp = cli->req.hdr.hdr[recvd];
        cli->req.hdr.hdr[recvd] = '\0';
        if ((hdr_end = strstr(cli->req.hdr.hdr, "\r\n\r\n")) ||
           (hdr_end = strstr(cli->req.hdr.hdr, "\n\n"))) {

            cli->req.hdr.hdr[recvd] = tmp;
            hdr_len = hdr_end - cli->req.hdr.hdr;
            /* check which whitespace we matched and advance */
            hdr_len += (hdr_end[0] == '\r') ? 4 : 2;

            cli->req.hdr.hdr_len = hdr_len;
            cli->req.body_len = recvd - hdr_len;
            if ((cli->req.body = malloc(cli->req.body_len)) == NULL) {
                app_log(cli->lsn->debug_log, APP_ERR,
                    NOMEM "not enough memory to recv body.");
                return HTTP_FAIL;
            }
            memcpy(cli->req.body, &cli->req.hdr.hdr[cli->req.hdr.hdr_len],
                cli->req.body_len);

            return 0;
        }
        cli->req.hdr.hdr[recvd] = tmp;
    }

    return HTTP_ERR_MAX_HDR_EXC;
}

static int
http_recv_req_hdr(http_cli_t *c)
{
    int ret = 0;
    ret = http_recv_hdr(c);

    if (ret != 0)
        return ret;

    ret = http_parse_req_hdr(c);

    return ret;
}

static int
http_cli_init(http_cli_t *cli)
{
    cli->req.host = NULL;
    cli->req.uri = NULL;
    cli->req.body = NULL;
    cli->req.path = NULL;
    cli->req.query_string = NULL;
    cli->resp.body_len = 0;
    cli->resp.respcode = 0;

    if ((cli->req.hdr.hdrs = h_init(128)) == NULL)
        return -1;
    if ((cli->resp.hdrs = h_init(128)) == NULL)
        return -1;

    if ((cli->req.query_string_args = h_init(128)) == NULL)
        return -1;

    return 0;
}

static void
http_cli_free(http_cli_t *cli)
{
    total--;
    lthread_close(cli->conn.fd);
    http_cli_reset(cli);
    free(cli);
}

static void
http_cli_reset(http_cli_t *cli)
{
    h_item_t *item = NULL;

    cli->resp.state = HTTP_HDR_BEGIN;
    if (cli->req.hdr.hdrs)
        h_free(cli->req.hdr.hdrs);

    if (cli->resp.hdrs) {
        while ((item = h_next(cli->resp.hdrs)))
            free(item->value);
        h_free(cli->resp.hdrs);
    }

    if (cli->req.query_string_args) {
        while ((item = h_next(cli->req.query_string_args)))
            free(item->value);
        h_free(cli->req.query_string_args);
    }

    if (cli->req.body)
        free(cli->req.body);

    if (cli->req.host)
        free(cli->req.host);
    if (cli->req.uri)
        free(cli->req.uri);
    if (cli->req.path)
        free(cli->req.path);
    if (cli->req.query_string)
        free(cli->req.query_string);

    cli->req.path = NULL;
    cli->req.query_string_args = NULL;
    cli->req.query_string = NULL;
    cli->req.hdr.hdrs = NULL;
    cli->resp.hdrs = NULL;
    cli->req.host = NULL;
    cli->req.uri = NULL;
    cli->req.body = NULL;
    cli->resp.body_len = 0;
    cli->resp.respcode = 0;
}

static void
http_handle_cli_req_err(http_cli_t *cli, int err)
{
    switch(err) {
    case HTTP_ERR_UNKNOWN_HOST:
    case HTTP_ERR_NO_CNT_LEN:
    case HTTP_ERR_INV_REQ_LINE:
    case HTTP_ERR_MAX_HDR_EXC:
    case HTTP_ERR_INV_HOST:
    case HTTP_ERR_INV_METHOD:
    case HTTP_ERR_INV_PORT:
    case HTTP_ERR_INV_PROTO:
        http_respond(400, NULL, 0);
        break;
    }

    switch(err) {
    case HTTP_ERR_UNKNOWN_HOST:
        app_log(cli->lsn->debug_log, APP_TRC, "Unknown host.");
        break;
    case HTTP_ERR_NO_CNT_LEN:
        app_log(cli->lsn->debug_log, APP_TRC, "no cnt len.");
        break;
    case HTTP_ERR_INV_REQ_LINE:
        app_log(cli->lsn->debug_log, APP_TRC, "invalid request line.");
        break;
    case HTTP_ERR_MAX_HDR_EXC:
        app_log(cli->lsn->debug_log, APP_TRC, "max hdr exceeded.");
        break;
    case HTTP_ERR_INV_HOST:
        app_log(cli->lsn->debug_log, APP_TRC, "invalid host.");
        break;
    case HTTP_ERR_INV_PORT:
        app_log(cli->lsn->debug_log, APP_TRC, "invalid port.");
        break;
    case HTTP_ERR_INV_PROTO:
        app_log(cli->lsn->debug_log, APP_TRC, "invalid protocol.");
        break;
    case HTTP_ERR_TIMEOUT_EXC:
        app_log(cli->lsn->debug_log, APP_TRC, "connection timed out.");
        break;
    }
}

static void
http_log_req(http_cli_t *cli)
{
    char ipstr[INET6_ADDRSTRLEN];

    inet_ntop(AF_INET, &cli->conn.peer_addr.sin_addr,
        ipstr, INET_ADDRSTRLEN);
    app_log(cli->lsn->access_log, APP_NOOP, "%s %s %s %s [%s]",
        ipstr,
        http_get_method_str(),
        cli->req.uri,
        cli->req.hdr.http11 ? "HTTP/1.1" : "HTTP/1.0 ",
        (char *)h_get(cli->req.hdr.hdrs, "user-agent")
    );
}

static int
http_recvbody(http_cli_t *cli)
{
    if (cli->req.hdr.chunked)
        return http_recv_chunked(cli);
     else
        return http_recv_exact(cli);
}

static int
http_recv_exact(http_cli_t *cli)
{
    uint64_t recvd = 0;
    uint64_t ret = 0;
    char *tmp = NULL;

    if (cli->req.hdr.nolen == 1)
        return 0;

    recvd = cli->req.body_len;
    if (recvd == cli->req.hdr.cnt_len)
        return 0;

    while (recvd < cli->req.hdr.cnt_len) {
        tmp = realloc(cli->req.body, recvd + TCP_BUF_SIZE);
        if (tmp == NULL) {
            app_log(cli->lsn->debug_log, APP_ERR,
                NOMEM "not enough memory to recv body.");
            free(cli->req.body);
            cli->req.body = NULL;
            return -1;
        }
        cli->req.body = tmp;

        ret = http_recv(cli, cli->req.body + recvd, TCP_BUF_SIZE);

        if (ret == -1) {
            free(cli->req.body);
            cli->req.body = NULL;
            return -1;
        }

        recvd += ret;
        cli->req.body_len = recvd;

        if (ret == 0)
            return 0;

    }

    return 0;
}

static int
http_recv_chunked(http_cli_t *cli)
{
    uint64_t recvd = 0;
    uint64_t len = 0;
    uint64_t more = 0;
    uint64_t ret = 0;
    char *tmp = NULL;

    /*
     * hex digits representing the chunk size are not part
     * of the chunk size, remove them.
     */
    recvd = cli->req.body_len - \
        (http_strcasechr(&cli->req.hdr.hdr[cli->req.hdr.hdr_len], LF,
        cli->req.body_len) - &cli->req.hdr.hdr[cli->req.hdr.hdr_len]) - 1;


    if (recvd == cli->req.hdr.cnt_len) {
        return 0;
    }

    len = 1;
    do {

        recvd += ret;

        more = recvd < cli->req.hdr.cnt_len ?-1: recvd - cli->req.hdr.cnt_len;
        /* If we received a single complete chunk in the header we'll
         * assume it is the only chunk. Not a proper assumption at all
         */
        if (more > 0 && ret == 0)
            break;

        while (more > 0) {
            /*
             * skip trailing spaces and don't count them as more
             * data.
             */
            while (more && (ret >= more) ) {
                if (isspace(cli->req.body[ret - more]))
                    more--;
                else
                    break;
            }
            /*
             * The new chunk size is not in this packet. we recvd
             * 0 bytes for the new chunk and we still don't know
             * it's full size.
             */
            if (more == 0) {
                recvd = 0;
                cli->req.hdr.cnt_len = 0;
                break;
            }
            /*
             * At this point ret - `more` points to the new chunk
             * size, parse it. but first, did we exceed the buffer.
             */
            if (ret < more)
                break;
            len = http_strtol(&cli->req.body[ret - more],
                ret - (ret - more), 16);
            /*
             * hex digits representing the chunk size are not part
             * of the chunk size, remove them.
             */
            recvd = -1 *(http_strcasechr(&cli->req.body[ret - more], LF,
                ret - (ret - more)) - &cli->req.body[ret - more]) - 1;
            cli->req.hdr.cnt_len = len;
            if (len == 0)
                break;
            /* how much data have we received for the new chunk? */
            recvd += ret - (ret - more);
            /*
             * if the chunk was small enough we could have recvd
             * it all in the same packet.
             */
            more = (recvd  < len) ? -1 : recvd - len;
        }

        if (len == 0)
            break;

        tmp = realloc(cli->req.body, recvd + TCP_BUF_SIZE);
        if (tmp == NULL) {
            free(cli->req.body);
            app_log(cli->lsn->debug_log, APP_ERR,
                NOMEM "not enough memory to recv body.");
            cli->req.body = NULL;
            return -1;
        }
        cli->req.body = tmp;

        ret = http_recv(cli, cli->req.body + cli->req.body_len, TCP_BUF_SIZE);

        if (ret == 0)
            return 0;

        if (ret == -1) {
            free(cli->req.body);
            cli->req.body = NULL;
            return -1;
        }

        if (more == 0) {
            recvd = 0;
            cli->req.hdr.cnt_len = 0;
        }

    } while (len > 0);

    return 0;
}

/* handles one cli request in each call */
static int
http_handle_cli_rd(http_cli_t *cli)
{
    int err = 0;

    /* recv client hdr after validating it. */
    if ((err = http_recv_req_hdr(cli)) != 0) {
        http_handle_cli_req_err(cli, err);
        return -1;
    }

    http_set_path(cli);
    /*if (cli->lsn->srv_log.log_level == SRV_DBG)
        http_print_exact(cli->req.hdr.hdr, cli->req.hdr.hdr_len);*/

    if (cli->req.method == HTTP_POST)
        http_recvbody(cli);

    return 0;
}

/* read data from client */
static void
http_lt_cli_rd(lthread_t *lt, http_cli_t *cli)
{
    int err = 0;
    DEFINE_LTHREAD;

    lthread_set_data(cli);
    while (1) {
        if (http_cli_init(cli) == -1)
            break;

        err = http_handle_cli_rd(cli);
        if (err == -1)
            break;
        if (cli->lsn->router_cb)
            err = cli->lsn->router_cb();

        http_respond(cli->resp.respcode, cli->resp.body, cli->resp.body_len);
        http_log_req(cli);
        if (err == -1)
            break;
        http_cli_reset(cli);
    }

    /* clean up client conn and return */
    //shutdown(cli->conn.fd, SHUT_RDWR);
    http_cli_free(cli);
}

static void
http_listener(lthread_t *lt, lsn_t *lsn)
{
    int cli_fd = 0;
    int lsn_fd = 0;
    struct sockaddr_in peer_addr;
    socklen_t addrlen = sizeof(peer_addr);

    DEFINE_LTHREAD;

    lsn_fd = e_listener("127.0.0.1", lsn->lsn_port);
    if (lsn_fd == -1)
        return;

    printf("Starting listener on %d\n", lsn->lsn_port);

    while (1) {
        cli_fd = lthread_accept(lsn_fd, (struct sockaddr*)&peer_addr, &addrlen);
        if (total >= 2000)
            lthread_sleep(10);
        total++;

        http_cli_new(lsn, cli_fd, &peer_addr);
    }

    close(lsn_fd);
}

static void
http_cli_new(lsn_t *lsn, int cli_fd, struct sockaddr_in *peer_addr)
{
    lthread_t *lt_cli_rd = NULL;
    http_cli_t *cli = NULL;
    int ret = 0;
    int opt = 1;

    if (cli_fd <= 0) {
        perror("Cannot accept new connection");
        return;
    }

    if ((cli = calloc(1, sizeof(http_cli_t))) == NULL) {
        app_log(lsn->debug_log, APP_ERR,
            NOMEM "not enough memory for cli calloc.");
        return;
    }

    cli->conn.fd = cli_fd;
    cli->conn.peer_addr = *peer_addr;

    if (setsockopt(cli_fd, SOL_SOCKET, SO_REUSEADDR, &opt,sizeof(int)) == -1)
        perror("failed to set SOREUSEADDR on socket");

    ret = lthread_create(&lt_cli_rd, http_lt_cli_rd, cli);
    if (ret != 0) {
        app_log(lsn->debug_log, APP_WRN, "http_cli_new failed");
        goto err;
    }

    cli->lt = lt_cli_rd;
    cli->lsn = lsn;

    return;
err:

    if (lt_cli_rd)
        lthread_destroy(lt_cli_rd);

    shutdown(cli_fd, SHUT_RDWR);
    lthread_close(cli_fd);
}

int
lsn_init(lsn_t *lsn, route_handler_cb_t cb, char *log_path, char *app_name)
{
    if (lsn == NULL)
        return -1;

    char tmp[256];
    lsn->birth = rdtsc();
    lsn->log_path = log_path;
    lsn->app_name = app_name;
    sprintf(tmp, "%s_%s", app_name, "srv_dbg");
    lsn->debug_log = app_log_new(APP_TRC, log_path, tmp);
    sprintf(tmp, "%s_%s", app_name, "srv_access");
    lsn->access_log = app_log_new(APP_NOOP, log_path, tmp);
    lsn->router_cb = cb;

    return 0;
}

void
lsn_run(lsn_t *lsn)
{
    lthread_t *lt = NULL;
    lthread_create(&lt, http_listener, lsn);
    lthread_create(&lt, bd_lt_listener, (void*)0);
    lthread_join();
}

void
http_set_respcode(int respcode)
{
    http_cli_t *cli = (http_cli_t*)lthread_get_data();

    int dont_close = 0;
    if (cli->resp.respcode)
        return;
    cli->resp.respcode = respcode;
    switch(cli->resp.respcode) {
    case 200:
        cli->resp.respcode_str = "OK";
        cli->resp.respcode_int = "200 ";
        dont_close = 1;
        break;
    case 201:
        cli->resp.respcode_str = "Created";
        cli->resp.respcode_int = "201 ";
        dont_close = 1;
        break;
    case 202:
        cli->resp.respcode_str = "Accepted";
        cli->resp.respcode_int = "202 ";
        dont_close = 1;
        dont_close = 1;
        break;
    case 203:
        cli->resp.respcode_str = "Non-Authoritative Information";
        cli->resp.respcode_int = "203 ";
        break;
    case 204:
        cli->resp.respcode_str = "No Content";
        cli->resp.respcode_int = "204 ";
        dont_close = 1;
        break;
    case 205:
        cli->resp.respcode_str = "Reset Content";
        cli->resp.respcode_int = "205 ";
        break;
    case 206:
        cli->resp.respcode_str = "Partial Content";
        cli->resp.respcode_int = "206 ";
        dont_close = 1;
        break;
    case 300:
        cli->resp.respcode_str = "Multiple Choices";
        cli->resp.respcode_int = "300 ";
        break;
    case 301:
        dont_close = 1;
        cli->resp.respcode_str = "Moved Permanently";
        cli->resp.respcode_int = "301 ";
        break;
    case 302:
        dont_close = 1;
        cli->resp.respcode_str = "Found";
        cli->resp.respcode_int = "302 ";
        break;
    case 303:
        dont_close = 1;
        cli->resp.respcode_str = "See Other";
        cli->resp.respcode_int = "303 ";
        break;
    case 304:
        dont_close = 1;
        cli->resp.respcode_str = "Not Modified";
        cli->resp.respcode_int = "304 ";
        break;
    case 305:
        cli->resp.respcode_str = "Use Proxy";
        cli->resp.respcode_int = "305 ";
        break;
    case 307:
        cli->resp.respcode_str = "Temporary Redirect";
        cli->resp.respcode_int = "307 ";
        break;
    case 400:
        cli->resp.respcode_str = "Bad Request";
        cli->resp.respcode_int = "400 ";
        break;
    case 401:
        cli->resp.respcode_str = "Unauthorized";
        cli->resp.respcode_int = "401 ";
        break;
    case 404:
        cli->resp.respcode_str = "Not Found";
        cli->resp.respcode_int = "404 ";
        break;
    case 405:
        cli->resp.respcode_str = "Method Not Allowed";
        cli->resp.respcode_int = "405 ";
        break;
    case 407:
        cli->resp.respcode_str = "Proxy Authentication Required";
        cli->resp.respcode_int = "407 ";
        break;
    case 408:
        cli->resp.respcode_str = "Request Timeout";
        cli->resp.respcode_int = "408 ";
        break;
    case 409:
        cli->resp.respcode_str = "Conflict";
        cli->resp.respcode_int = "409 ";
        break;
    case 410:
        cli->resp.respcode_str = "Gone";
        cli->resp.respcode_int = "410 ";
        break;
    case 411:
        cli->resp.respcode_str = "Length Required";
        cli->resp.respcode_int = "411 ";
        break;
    case 412:
        cli->resp.respcode_str = "Precondition Failed";
        cli->resp.respcode_int = "412 ";
        break;
    case 413:
        cli->resp.respcode_str = "Request Entity Too Large";
        cli->resp.respcode_int = "413 ";
        break;
    case 414:
        cli->resp.respcode_str = "Request-URI Too Long";
        cli->resp.respcode_int = "414 ";
        break;
    case 415:
        cli->resp.respcode_str = "Unsupported Media Type";
        cli->resp.respcode_int = "415 ";
        break;
    case 416:
        cli->resp.respcode_str = "Requested Range Not Satisfiable";
        cli->resp.respcode_int = "416 ";
        break;
    case 417:
        cli->resp.respcode_str = "Expectation Failed";
        cli->resp.respcode_int = "417 ";
        break;
    case 500:
        cli->resp.respcode_str = "Internal Server Error";
        cli->resp.respcode_int = "500 ";
        break;
    case 501:
        cli->resp.respcode_str = "Not Implemented";
        cli->resp.respcode_int = "501 ";
        break;
    case 502:
        cli->resp.respcode_str = "Bad Gateway";
        cli->resp.respcode_int = "502 ";
        break;
    case 503:
        cli->resp.respcode_str = "Service Unavailable";
        cli->resp.respcode_int = "503 ";
        break;
    case 504:
        cli->resp.respcode_str = "Gateway Timeout";
        cli->resp.respcode_int = "504 ";
        break;
    case 505:
        cli->resp.respcode_str = "HTTP Version Not Supported";
        cli->resp.respcode_int = "505 ";
        break;
    default:
        cli->resp.respcode_str = "No Content";
        cli->resp.respcode = 204;
        cli->resp.respcode_int = "204 ";
        dont_close = 0;
    }

    if (dont_close == 0)
        http_add_header(cli, "Connection", "close");
    else
        http_add_header(cli, "Connection", "keep-alive");

}

void
http_add_header(http_cli_t *cli, char *name, char *value)
{
    h_insert(cli->resp.hdrs, name, strdup(value));
}

static void
http_set_path(http_cli_t *cli)
{
    char *uri = cli->req.uri;
    char *tmp1 = NULL, tmp2 = 0, *sep1 = NULL, *sep2 = NULL, *value = NULL;

    //printf("uri is %s\n", cli->req.uri);
    if ((sep1 = strchr(uri, '?')) != NULL) {
        cli->req.path = strndup(uri, sep1 - uri);
        sep1++;
        cli->req.query_string = strdup(sep1);
        //printf("query_string is %s\n", cli->req.query_string);
        //printf("path is %s\n", cli->req.path);

        for(value = strtok_r(cli->req.query_string, "&", &tmp1);
                value;
                value = strtok_r(NULL, "&", &tmp1)) {
            if ((sep2 = strchr(value, '=')) != NULL) {
                tmp2 = value[sep2 - value];
                value[sep2 - value] = '\0';
                sep2++;
                //printf("adding arg key:%s value:%s\n", value, sep2);
                h_insert(cli->req.query_string_args, value, strdup(sep2));
                sep2--;
                value[sep2 - value] = tmp2;
            }
            if (value > cli->req.query_string)
            *(value-1) = '&';
        }
    } else {
        cli->req.path = strdup(uri);
    }
}

/*
 * public methods
 */

char *
http_get_querystring(char *str)
{
    http_cli_t *cli = (http_cli_t*)lthread_get_data();
    return h_get(cli->req.query_string_args, str);
}

char *
http_get_method_str(void)
{
    http_cli_t *cli = (http_cli_t*)lthread_get_data();

    switch(cli->req.method) {
    case HTTP_GET:
        return "GET";
    case HTTP_POST:
        return "POST";
    case HTTP_DELETE:
        return "DELETE";
    case HTTP_PUT:
        return "PUT";
    case HTTP_HEAD:
        return "HEAD";
    }
    return "Unknown Method";
}

char *
http_get_body(void)
{
    http_cli_t *cli = (http_cli_t*)lthread_get_data();

    return cli->req.body;
}

uint64_t
http_get_body_len(void)
{
    http_cli_t *cli = (http_cli_t*)lthread_get_data();

    return cli->req.body_len;
}

char *
http_get_path(void)
{
    http_cli_t *cli = (http_cli_t*)lthread_get_data();

    return cli->req.path;
}

int
http_respond(int http_code, char *body, uint64_t body_len)
{
    int ret = 0;
    http_cli_t *cli = (http_cli_t*)lthread_get_data();
    struct iovec *iovecs = NULL;
    int total_iovecs = 0;

    if (cli->resp.state != HTTP_HDR_BEGIN)
        return 0;

    http_set_respcode(http_code);
    if (body_len) {
        cli->resp.body = body;
        cli->resp.body_len = body_len;
    }

    total_iovecs = http_cli_resp_hdr_create(cli, &iovecs);
    if (total_iovecs < 1)
        return -1;

    if (cli->resp.body_len) {
        iovecs[total_iovecs].iov_base = cli->resp.body;
        iovecs[total_iovecs].iov_len = cli->resp.body_len;
        total_iovecs++;

    }

    ret = lthread_writev(cli->conn.fd, iovecs, total_iovecs);
    cli->resp.state = HTTP_BODY_SENT;
    free(iovecs);

    return 0;
}

#ifdef __FreeBSD__
int
http_sendfile(int http_code, int fd)
{
    http_cli_t *cli = (http_cli_t*)lthread_get_data();
    struct iovec *iovecs = NULL;
    int total_iovecs = 0;
    struct sf_hdtr hdr = {0};
    struct stat st = {0};

    fstat(fd, &st);

    cli->resp.body_len = st.st_size;

    http_set_respcode(http_code);
    total_iovecs = http_cli_resp_hdr_create(cli, &iovecs);
    if (total_iovecs < 1)
        return -1;

    hdr.headers = iovecs;
    hdr.hdr_cnt = total_iovecs;

    int ret = lthread_sendfile(fd, cli->conn.fd, 0, 0, &hdr);
    if (ret == -1)
        return -1;

    cli->resp.state = HTTP_BODY_SENT;
    free(iovecs);

    return 0;
}
#else
int
http_sendfile(int http_code, int fd)
{
    assert(0);
}
#endif
