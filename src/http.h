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

  http.h
*/
#ifndef _HTTP_H_
#define _HTTP_H_

#include "http_impl.h"

int http_respond(int http_code, char *body, uint64_t body_len);
void http_set_respcode(int);
void *lsn_run(void *lsn);
int lsn_init(lsn_t *lsn, route_handler_cb_t cb);
void http_add_header(http_cli_t *cli, char *name, char *value);
char *http_get_method_str(void);
char *http_get_querystring(char *str);
uint64_t http_get_body_len(void);
char *http_get_body(void);
char *http_get_path(void);
int http_sendfile(int http_code, int fd);
#endif
