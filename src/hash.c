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

  hash.c
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "common/queue.h" 
#include "hash.h"

h_hash_t *
h_init(int size)
{
    h_hash_t *h = NULL;
    int i = 0;

    if ((h = calloc(1, sizeof(h_hash_t))) == NULL)
        return NULL;
    if ((h->buckets = calloc(1, size * sizeof(h_bucket_t))) == NULL)
                return NULL;

    h->size = size;
    h->order_next = NULL;

    for (i = 0; i < size; i++)
        LIST_INIT(&h->buckets[i].head);
    TAILQ_INIT(&h->order_head);

    return h;
}

int
h_remove(h_hash_t *h, char *key)
{
    int hash = 0;
    h_item_t *item = NULL, *item_tmp = NULL;

    hash = h_hash_func(key, strlen(key)) % h->size;
    LIST_FOREACH_SAFE(item, &h->buckets[hash].head, items, item_tmp) {
        if (strcmp(key, item->key) == 0) {
            LIST_REMOVE(item, items);
            TAILQ_REMOVE(&h->order_head, item, ordered_items);
            free(item);
            return 0;
        }
    }

    return -1;    
}

int
h_insert(h_hash_t *h, char *key, void *value)
{
    h_item_t *item = NULL;
    h_item_t *curr_item = NULL;
    int hash = 0;

    hash = h_hash_func(key, strlen(key)) % h->size;

    LIST_FOREACH(curr_item, &h->buckets[hash].head, items) {
        if (strcmp(key, curr_item->key) == 0)
            return -1;
    }

    if ((item = calloc(1, sizeof(h_item_t))) == NULL)
        return -1;

    item->key = strdup(key);
    item->value = value;

    LIST_INSERT_HEAD(&h->buckets[hash].head, item, items);
    TAILQ_INSERT_TAIL(&h->order_head, item, ordered_items);

    return 0;
}

void
h_init_traverse(h_hash_t *h)
{
    h->order_next = NULL;
}

void *
h_get(h_hash_t *h, char *key)
{
    int hash = 0;
    h_item_t *item = NULL;

    hash = h_hash_func(key, strlen(key)) % h->size;
    LIST_FOREACH(item, &h->buckets[hash].head, items) {
        if (strcmp(key, item->key) == 0)
            return item->value;
    }

    return NULL;    
}

h_item_t *
h_next(h_hash_t *h)
{
    if (h->order_next == NULL)
        h->order_next = TAILQ_FIRST(&h->order_head);
    else
        h->order_next = TAILQ_NEXT(h->order_next, ordered_items);

    /*if (h->order_next)
    printf("order_next is %p: %s %s\n", h->order_next,
        h->order_next->key, h->order_next->value);*/
    return h->order_next;
}

void
h_free(h_hash_t *h)
{
    h_item_t *item = NULL;
    h_init_traverse(h);
    while (!TAILQ_EMPTY(&h->order_head)) {
            item = TAILQ_FIRST(&h->order_head);
            TAILQ_REMOVE(&h->order_head, item, ordered_items);
        free(item->key);
        free(item);
    }

    free(h->buckets);
    free(h);
}

uint32_t h_hash_func(const char *key, uint32_t len)
{
    int i, hash = 0;

    for (i = 0; i < len; i++)
        hash += key[i];

    return hash;
}
