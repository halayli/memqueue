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

  args_parser.h
*/

#ifndef _ARGS_PARSER_H_
#define _ARGS_PARSER_H_

typedef struct poll_args poll_args_t;
typedef struct memqueue_create_args memqueue_create_args_t;
struct poll_args;
struct memqueue_create_args;

struct poll_args {
    struct {
        int rev;
        int latest;
        char *consumer_id;
        char *q_id;
        int include_consumers;
    } queue_args[32];
    int timeout;
    int total;
};

struct memqueue_create_args {
    uint32_t expiry;
    int max_size;
    uint32_t consumer_expiry;
    int drop_from_head;
    char *q_id;
};

void args_memqueue_poll_populate(poll_args_t *user_args, h_hash_t *args);
void args_memqueue_create_populate(memqueue_create_args_t *user_args,
    h_hash_t *args);
#endif
