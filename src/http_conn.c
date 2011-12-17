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

  http_conn.c
*/
#include <unistd.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <err.h>

#include "http_impl.h"

int
http_send(http_cli_t *cli, char *buf, uint64_t len)
{
    return lthread_send(cli->conn.fd, buf, len, 0);
}

int
http_recv(http_cli_t *cli, char *buf, uint64_t len)
{
    return lthread_recv(cli->conn.fd, buf, len, 0, 10000);
}
