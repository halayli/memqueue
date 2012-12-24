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

  router.c
*/
#include <stdio.h>
#include <string.h>
#include <pcre.h>
#include <sys/queue.h>

#include "router.h"
#include "log.h"

struct http_route {
    char *path;
    char *method;
    route_cb_t cb;
    pcre *re;
    LIST_ENTRY(http_route) next;
};
typedef struct http_route http_route_t;

LIST_HEAD(http_route_l, http_route);
struct http_route_l routes;

void
http_route_init(void)
{
    LIST_INIT(&routes);
}

int
http_route_on(char *method, char *path, route_cb_t cb)
{
    http_route_t *route = NULL;
    int erroffset = 0;
    const char *error = NULL;

    if ((route = malloc(sizeof(http_route_t))) == NULL)
        return -1;

    route->path = path;
    route->cb = cb;
    route->method = method;
    route->re = pcre_compile(path, 0, &error, &erroffset, NULL);
    if (route->re == NULL) {
        LOG_ERROR("PCRE compilation failed at offset %d: %s", erroffset, error);
        free(route);
        return -1;
    }
    LOG_INFO("Route %s added successfully", path);

    LIST_INSERT_HEAD(&routes, route, next);
    return 0;
}

int
http_route_handle_request(void)
{
    http_route_t *route = NULL;
    h_hash_t *h = NULL;
    int rc, i;
    int namecount;
    int ovector[30];
    char *name_table = NULL;
    int name_entry_size;
    int matched = 0;
    char *path = http_get_path();
    int ret = 0;

    LIST_FOREACH(route, &routes, next) {
        if (strcmp(route->method, http_get_method_str()) != 0)
            continue;
        rc = pcre_exec(route->re, NULL, path,
            strlen(path), 0, 0, ovector, 30);
        if (rc < 0) /* match failed. continue */
            continue;

        matched = 1;
        rc = pcre_fullinfo(route->re, NULL, PCRE_INFO_NAMECOUNT, &namecount);
        /* group subpatterns exist, hand them off to the caller */
        if (namecount > 0) {
            pcre_fullinfo(route->re, NULL, PCRE_INFO_NAMETABLE, &name_table);
            pcre_fullinfo(route->re, NULL, PCRE_INFO_NAMEENTRYSIZE,
                &name_entry_size);
            h = h_init(128);
            if (h != NULL)
                for (i = 0; i < namecount; i++) {
                    int n = (name_table[0] << 8) | name_table[1];
                    char *value = strndup(path + ovector[2*n],
                        ovector[2*n+1] - ovector[2*n]);
                    if (value == NULL) {
                        free(h);
                        continue;
                    }
                    h_insert(h, name_table + 2, value);
                    name_table += name_entry_size;
                }
        }

        ret = route->cb(h);
        if (h) {
            h_item_t *item = NULL;
            while ((item = h_next(h)))
                free(item->value);
            h_free(h);
        }

        if (ret == -2)
            continue;

        return ret;
    }

    if (!matched) {
        http_set_respcode(404);
        return -1;
    }

    return 0;
}
