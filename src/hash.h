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

  hash.h
*/
#ifndef _HASHLIB_H_
#define _HASHLIB_H_

/*        hash
 * ----------------------
 * |     entry          | ----> item
 * ----------------------
 * |     entry          | ----> item
 * ----------------------
 * |     entry          | ----> item
 * ----------------------
*/
#include "common/queue.h" 
#include <stdint.h>

uint32_t h_hash_func(const char *key, uint32_t len);

struct h_item {
    char *key;
    char *value;
    int order;
    LIST_ENTRY(h_item) items;
    TAILQ_ENTRY(h_item) ordered_items;
};
typedef struct h_item_t_l h_item_t_l;
LIST_HEAD(h_item_t_l, h_item);
TAILQ_HEAD(h_item_t_q, h_item);

struct  h_bucket {
    struct h_item_t_l head;
};

typedef struct h_hash    h_hash_t;
typedef struct h_bucket    h_bucket_t;
typedef struct h_item    h_item_t;

struct h_hash {
    h_bucket_t *buckets;
    struct h_item_t_q order_head;
    int size;
    h_item_t *order_next;
};

h_hash_t *h_init(int size);
int h_remove(h_hash_t *h, char *key);
int h_insert(h_hash_t *h, char *key, void *value);
void h_init_traverse(h_hash_t *h);
h_item_t *h_next(h_hash_t *h);
void h_free(h_hash_t *h);
void *h_get(h_hash_t *h, char *key);

#endif
