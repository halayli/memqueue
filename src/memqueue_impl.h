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

  memqueue_impl.h
*/
#ifndef _MEMQUEUE_H_
#define _MEMQUEUE_H_

#include <ctype.h>
#include <unistd.h>

#include <sys/queue.h>
#include "http.h"
#include "hash.h"
#include "rbtree.h"
#include "time.h" 
#include "json/json.h"

struct expiry_node;
struct cli_binder;
struct msg;
struct memqueue;
struct pending_msg;
struct memqueue_ins;
struct consumer;

typedef struct expiry_node expiry_node_t;
typedef struct cli_binder cli_binder_t;
typedef struct msg msg_t;
typedef struct memqueue memqueue_t;
typedef struct memqueue_binder memqueue_binder_t;
typedef struct pending_msg pending_msg_t;
typedef struct memqueue_ins memqueue_ins_t;
typedef struct consumer consumer_t;

LIST_HEAD(cli_binder_l, cli_binder);
LIST_HEAD(consumer_l, consumer);
LIST_HEAD(msg_l, msg);
LIST_HEAD(memqueue_l, memqueue);
LIST_HEAD(memqueue_binder_l, memqueue_binder);
TAILQ_HEAD(msg_q, msg);
TAILQ_HEAD(pending_msg_q, pending_msg);

typedef struct expiry_obj_l expiry_obj_l_t;
typedef struct cli_binder_l cli_binder_l_t;
typedef struct msg_q msg_q_t;
typedef struct msg_l msg_l_t;
typedef struct consumer_l consumer_l_t;
typedef struct memqueue_l memqueue_l_t;
typedef struct memqueue_binder_l memqueue_binder_l_t;
typedef struct pending_msg_q pending_msg_q_t;

typedef enum {
    MEMQUEUE_NOT_FOUND = 0,
    MEMQUEUE_CREATED,
    MEMQUEUE_EXISTS,
    MEMQUEUE_MSG_ADDED,
    MEMQUEUE_MSGS,
    MEMQUEUE_TIMEOUT,
    MEMQUEUE_DELETED,
    MEMQUEUE_FULL,
    MEMQUEUE_OK,
} memqueue_ret_t;

typedef enum {
    MSG_TYPE,
    MEMQUEUE_TYPE,
    CONSUMER_TYPE,
} obj_type_t;

struct expiry_node {
    uint32_t msecs;
    struct rb_node node;
    memqueue_l_t memqueue_objs;
    consumer_l_t consumer_objs;
    msg_l_t msg_objs;
};

/*
 * memqueue_binder represents a bind to a queue.
 */
struct memqueue_binder {
    int rev;
    memqueue_t *memqueue;
    LIST_ENTRY(memqueue_binder) q_next; /* link list in memqeueue */
    LIST_ENTRY(memqueue_binder) b_next; /* link list in cli_binder */
    cli_binder_t *cli_binder;
    int include_consumers;
};

struct pending_msg {
    msg_t *msg;
    memqueue_binder_t memqueue_binder;
    TAILQ_ENTRY(pending_msg) next;
};

struct cli_binder {
    lthread_t *lt;
    uint32_t timeout;
    lthread_cond_t *cond;
    memqueue_binder_l_t memqueues;
    LIST_ENTRY(cli_binder) next;
    pending_msg_q_t pending_msgs;
};

struct msg {
    void *data;
    uint64_t data_len;
    int ref_count;
    int rev;
    int new;
    memqueue_t *memqueue;
    LIST_ENTRY(msg) expiry_next;
    TAILQ_ENTRY(msg) next;
    int32_t expiry;
    expiry_node_t *expiry_node;
    uint64_t msg_id;
};

struct memqueue {
    int rev;
    int ref_count;
    int max_size;
    int drop_from_head;
    char *q_id;
    msg_q_t msg_queue;
    memqueue_binder_l_t memqueue_binders;
    LIST_ENTRY(memqueue) expiry_next;
    uint32_t expiry;
    uint32_t consumer_expiry;
    h_hash_t *consumers;
    expiry_node_t *expiry_node;
    int msgs_in_queue;
};

struct memqueue_ins {
    h_hash_t *queue_store;
    uint64_t birth;
    struct rb_root memqueue_expiry;
    struct rb_root msg_expiry;
    struct rb_root consumer_expiry;
    lthread_cond_t *cond;
    lthread_t *lt_sleep;
    uint64_t unique_msg_id;
    
};

struct consumer {
    char *consumer_id;
    uint32_t expiry;
    expiry_node_t *expiry_node;
    memqueue_t *memqueue;
    LIST_ENTRY(consumer) expiry_next;
};

#define VERSION "memqueue 1.0"
#define set_server_header (http_add_header(cli, "Server", VERSION))

#define json_str(x) json_object_new_string((x))

void memqueue_release(memqueue_t *q);
void msg_release(msg_t *msg);
void obj_sched_expire(memqueue_ins_t *ins, void *obj, uint32_t msecs,
    obj_type_t type);
uint32_t sched_get_min_timeout(memqueue_ins_t *ins);
void objs_expire(memqueue_ins_t *ins, obj_type_t type);
void memqueue_resched(memqueue_ins_t *ins, memqueue_t *memqueue);
void consumer_resched(memqueue_ins_t *ins, consumer_t *consumer);
void consumer_expired(consumer_t *consumer);
#endif
