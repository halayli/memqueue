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

  http_impl.h
*/
#ifndef _HTTP_IMPL_H_
#define _HTTP_IMPL_H_

#include <sys/queue.h>
#include <lthread.h>
#include "http_str.h"
#include "log.h"
#include "hash.h"

#define TCP_BUF_SIZE        (32*1024)
#define HTTP_MAX_HDR_LEN    (8192)
#define HTTP_MAX_HDRS        (64)
#define HTTP_MAX_HOST_LEN    (255)
#define HTTP_MAX_REQ_LINE_LEN   (4096)
#define HTTP_DEFAULT_SERVER_PORT (80)
#define NOMEM "Memory Error: "

#define NELEMENTS(x) (sizeof (x) / sizeof x[0])
#define IS_SET(x, y) ((x) & (1<<(y)))
#define SET_BIT(x, y) ((x) |= (1<<(y)))

typedef enum {
    HTTP_GET = 1,
    HTTP_POST,
    HTTP_PUT,
    HTTP_HEAD,
    HTTP_DELETE,
} http_method_t;

typedef struct lsn              lsn_t;
typedef struct http_cli         http_cli_t;
typedef struct http_hdr         http_hdr_t;
typedef struct http_req         http_req_t;
typedef struct http_resp        http_resp_t;
typedef struct http_conn        http_conn_t;

typedef enum  {
    HTTP_FAIL = -1,
    HTTP_ERR_UNKNOWN_HOST,  /* no host in url or host hdr   */
    HTTP_ERR_NO_CNT_LEN,    /* no content length            */
    HTTP_ERR_MAX_HDR_EXC,   /* max hdr size exceeded        */
    HTTP_ERR_CLI_CLOSED,    /* cli closed connection        */
    HTTP_ERR_INV_REQ_LINE,  /* invalid request line         */
    HTTP_ERR_INV_METHOD,    /* invalid method               */
    HTTP_ERR_INV_HOST,      /* invalid host format          */
    HTTP_ERR_INV_PORT,      /* port out of range            */
    HTTP_ERR_INV_PROTO,     /* invalid protocol             */
    HTTP_ERR_TIMEOUT_EXC,   /* read/write timeout exceeded */
    HTTP_ERR_INV_HDR,       /* hdr contains null or not complaint */
} http_req_err_t;

struct http_hdr {
    char        hdr[HTTP_MAX_HDR_LEN];
    int         hdr_start;
    int         hdr_len;
    uint64_t    cnt_len;   /* content-length or chunk size */
    unsigned    chunked:1;
    unsigned    nolen:1;
    unsigned    keepalive:1;
    unsigned    http11:1;
    unsigned    expect100:1;
    h_hash_t    *hdrs;
};

/*
 * Request Structure
 */
struct http_req {
    char            *host;
    http_method_t   method;
    char            *method_str;
    char            *req_line;
    char            *uri;
    unsigned short  port; /* port if any (default 80) */
    http_hdr_t      hdr;
    char            *body;
    uint64_t        body_len;
    char            *path;
    char            *query_string;
    h_hash_t        *query_string_args;
};

/*
 * Response Structure
 */

typedef enum {
    HTTP_HDR_BEGIN,
    HTTP_HDR_SENT,
    HTTP_BODY_SENT
} resp_state_t;

struct http_resp {
    int         respcode;
    char        *respcode_int;
    char        *respcode_str;
    h_hash_t    *hdrs;
    char        *body;
    uint64_t    body_len;
    resp_state_t state;
};

/*
 * Connection Structure
 */
struct http_conn {
    int fd;
    struct sockaddr_in peer_addr;
};

/*
 * Client Structure
 */
struct http_cli {
    http_req_t  req;
    http_resp_t resp;
    lsn_t       *lsn;
    lthread_t   *lt;
    http_conn_t conn;
};

/*
 * listener Structure
 */
typedef int (*route_handler_cb_t)(void);
struct lsn {
    unsigned short  lsn_port;
    struct in_addr  lsn_addr;
    log_t           *debug_log;
    log_t           *access_log;
    uint64_t        birth;
    route_handler_cb_t   router_cb;
    char            *app_name;
    char            *log_path;
};

/*
 * from http_conn.c
 */
int http_send(http_cli_t *cli, char *buf, uint64_t len);
int http_recv(http_cli_t *cli, char *buf, uint64_t len);

/*
 * from http_parser.c
 */
int http_parse_req_line(http_cli_t *cli);
int http_parse_req_hdr(http_cli_t *cli);
int http_parse_hdr(http_cli_t *cli);
#endif
