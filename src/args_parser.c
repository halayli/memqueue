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

  args_parser.c
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include "memqueue_impl.h"
#include "args_parser.h"

void
args_memqueue_poll_populate(poll_args_t *user_args, h_hash_t *args)
{

    char *total_queues_str = http_get_querystring("total_queues");
    int i = 0;
    int j = 0;
    int total_queues = 0;
    char *fieldvalue = NULL;
    char *fieldname = NULL;
    char *fields[] = {"q_id", "rev", "latest", "consumer_id",
        "include_consumers", "timeout"};

    memset(user_args, 0, sizeof (poll_args_t));

    if (total_queues_str && args == NULL)
        total_queues = atoi(total_queues_str);
    else if (args != NULL)
        total_queues = 1;
    else
        total_queues = 0;

    for (i = 0; i < total_queues; i++) {
        for (j = 0; j < sizeof(fields) / sizeof (fields[0]); j++) {
            if (args == NULL)
                asprintf(&fieldname, "%s-%d", fields[j], i);
            else
                asprintf(&fieldname, "%s", fields[j]);

            if (args != NULL && j == 0)
                fieldvalue = h_get(args, fields[i]);
            else
                fieldvalue = http_get_querystring(fieldname);

            if (fieldvalue == NULL) {
                free(fieldname);
                continue;
            }

            switch(j) {
                case 0:
                    user_args->queue_args[i].q_id = fieldvalue;
                    break;
                case 1:
                    user_args->queue_args[i].rev = atoi(fieldvalue);
                    break;
                case 2:
                    user_args->queue_args[i].latest = atoi(fieldvalue);
                    break;
                case 3:
                    user_args->queue_args[i].consumer_id = fieldvalue;
                    break;
                case 4:
                    user_args->queue_args[i].include_consumers = \
                        atoi(fieldvalue);
                    break;
            }
            free(fieldname);
        }
    }

    user_args->total = total_queues;
    fieldvalue = http_get_querystring("timeout");
    if (fieldvalue)
        user_args->timeout = atoi(fieldvalue);
    else
        user_args->timeout = -1;

    return;
}

void
args_memqueue_create_populate(memqueue_create_args_t *user_args, h_hash_t *args)
{

    int j = 0;
    char *fieldvalue = NULL;
    char *fieldname = NULL;
    char *fields[] = {"q_id", "max_size",
        "consumer_expiry", "expiry", "drop_from_head"};

    memset(user_args, 0, sizeof (memqueue_create_args_t));
    for (j = 0; j < sizeof(fields) / sizeof (fields[0]); j++) {
        fieldname = fields[j];

        if (args != NULL && j == 0)
            fieldvalue = h_get(args, fields[j]);
        else
            fieldvalue = http_get_querystring(fieldname);

        if (fieldvalue == NULL)
            continue;

        switch(j) {
            case 0:
                user_args->q_id = fieldvalue;
                break;
            case 1:
                user_args->max_size = atoi(fieldvalue);
                break;
            case 2:
                user_args->consumer_expiry = atoi(fieldvalue);
                break;
            case 3:
                user_args->expiry = atoi(fieldvalue);
                break;
            case 4:
                user_args->drop_from_head = atoi(fieldvalue);
                break;
        }
    }

    return;
}
