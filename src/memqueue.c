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

  memqueue.c
*/

#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <lthread.h>
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>

#include "memqueue_impl.h"
#include "router.h"
#include "log.h"
#include "args_parser.h"

static memqueue_ins_t memqueue_ins = {0};

static cli_binder_t *cli_binder_create(uint32_t timeout);
static int cli_binder_enqueue_msgs(cli_binder_t *cli_binder, memqueue_t *q,
    int rev, int latest);
static void cli_binder_release_pending_msgs(cli_binder_t *cli_binder);
static int cli_binder_has_pending_msgs(cli_binder_t *cli_binder);
static int cli_binder_enqueue_msg(cli_binder_t *cli_binder, msg_t *msg);
static void cli_wakeup(cli_binder_t *cli_binder);
static void cli_binder_free(cli_binder_t *b);

static void memqueue_respond(memqueue_ret_t ret, void *ptr);
static json_object* memqueue_create_result(cli_binder_t *cli_binder);

static void memqueue_msg_free(msg_t *msg);
static int memqueue_msg_create(memqueue_t *q, int32_t msecs, void *data,
    uint64_t data_len);
static void memqueue_msg_retain(msg_t *msg);

static memqueue_t *memqueue_create(memqueue_create_args_t *args);

static void memqueue_retain(memqueue_t *q);
static int memqueue_bind(memqueue_t *q, cli_binder_t *b, int rev,
    int include_consumers);
static void memqueue_unbind_all(cli_binder_t *b);

static void memqueue_free_consumer(consumer_t *consumer);
static void memqueue_refresh_consumer(memqueue_t *q, char *consumer_id);

static void obj_cleaner(void *arg);

static int memqueue_poll_queue(poll_args_t *user_args);
static int is_memqueue_full(memqueue_t *q);

struct {
    int resp_code;
    char *ret_str;
    char *ret_desc;
} memqueue_ret[] = {
    [MEMQUEUE_NOT_FOUND] =
        {404, "MEMQUEUE_NOT_FOUND", "memqueue not found"},
    [MEMQUEUE_CREATED] =
        {200, "MEMQUEUE_CREATED", "memqueue created successfully"},
    [MEMQUEUE_EXISTS] =
        {200, "MEMQUEUE_EXISTS", "memqueue already exists"},
    [MEMQUEUE_MSG_ADDED] =
        {200, "MEMQUEUE_MSG_ADDED", "message added successfully"},
    [MEMQUEUE_TIMEOUT] =
        {408, "MEMQUEUE_TIMEOUT", "poll timed out"},
    [MEMQUEUE_MSGS] =
        {200, "MEMQUEUE_MSGS", ""},
    [MEMQUEUE_DELETED] =
        {200, "MEMQUEUE_DELETED", "memqueue deleted successfully"},
    [MEMQUEUE_FULL] =
        {200, "MEMQUEUE_FULL", "memqueue is full."}
};

static int
memqueue_poll_queue(poll_args_t *user_args)
{

    int rev = 0;
    int i = 0;
    int total_msgs = 0;
    int total_bound_queues = 0;

    cli_binder_t *cli_binder = NULL;
    memqueue_t *q = NULL;

    cli_binder = cli_binder_create(user_args->timeout);
    for (i = 0; i < user_args->total; i++) {

        q = h_get(memqueue_ins.queue_store, user_args->queue_args[i].q_id);

        if (!q)
            continue;

        total_bound_queues++;

        if (q->consumer_expiry && user_args->queue_args[i].consumer_id != NULL)
            memqueue_refresh_consumer(q, user_args->queue_args[i].consumer_id);

        memqueue_resched(&memqueue_ins, q);

        //printf("q->rev is %d and user_rev is %d\n", q->rev, user_args->queue_args[i].rev);
        if (q->rev < user_args->queue_args[i].rev)
            rev = -1;
        else
            rev = user_args->queue_args[i].rev;

        /* bind poller */
        memqueue_bind(q, cli_binder, rev,
            user_args->queue_args[i].include_consumers);

        total_msgs += cli_binder_enqueue_msgs(cli_binder, q, rev,
            user_args->queue_args[i].latest);

    }

    if (total_bound_queues == 0) {
        cli_binder_free(cli_binder);
        memqueue_respond(MEMQUEUE_NOT_FOUND, NULL);
        return 0;
    }

    /* Do we have messages that match our rev requirements already? */
    if (total_msgs == 0)
        lthread_cond_wait(cli_binder->cond, cli_binder->timeout);

    if (cli_binder_has_pending_msgs(cli_binder))
        memqueue_respond(MEMQUEUE_MSGS, cli_binder);
    else
        memqueue_respond(MEMQUEUE_TIMEOUT, NULL);

    memqueue_unbind_all(cli_binder);
    cli_binder_release_pending_msgs(cli_binder);
    cli_binder_free(cli_binder);

    return 0;
}

static void
memqueue_free_consumer(consumer_t *consumer)
{
    h_remove(consumer->memqueue->consumers, consumer->consumer_id);
    free(consumer->consumer_id);
    free(consumer);
}

void
memqueue_msg_release(msg_t *msg)
{
    msg->new = 0;
    msg->ref_count--;

    assert(msg->ref_count >= 0);
    if (msg->ref_count == 0) {
        LOG_TRACE("msg %llu in queue %s has expired.",
            msg->msg_id,
            msg->memqueue->q_id);
        TAILQ_REMOVE(&msg->memqueue->msg_queue, msg, next);
        msg->memqueue->msgs_in_queue--;
        memqueue_release(msg->memqueue);
        memqueue_msg_free(msg);
    } else {
        memqueue_release(msg->memqueue);
    }
}

static void
memqueue_msg_retain(msg_t *msg)
{
    msg->ref_count++;
    memqueue_retain(msg->memqueue);
}

static void
memqueue_retain(memqueue_t *q)
{
    q->ref_count++;
}

void
memqueue_release(memqueue_t *q)
{
    h_item_t *item = NULL;
    q->ref_count--;
    assert(q->ref_count >= 0);

    if (q->ref_count == 0) {
        LOG_TRACE("Queue %s expired.", q->q_id);
        h_remove(memqueue_ins.queue_store, q->q_id);

        while ((item = h_next(q->consumers))) {
            free(item->value);
        }

        h_free(q->consumers);
        free(q->q_id);
        free(q);
    }
}

static void
memqueue_respond(memqueue_ret_t ret, void *ptr)
{
    char *tmp = NULL;
    json_object *resp_object = NULL;
    json_object *msgs = NULL;
    memqueue_t *memqueue = NULL;

    resp_object = json_object_new_object();
    json_object_object_add(resp_object, "version", json_str(VERSION));

    json_object_object_add(resp_object, "ret",
        json_str(memqueue_ret[ret].ret_str));
    switch (ret) {
        case MEMQUEUE_NOT_FOUND:
        case MEMQUEUE_EXISTS:
        case MEMQUEUE_TIMEOUT:
        case MEMQUEUE_FULL:
            json_object_object_add(resp_object, "error",
                json_str(memqueue_ret[ret].ret_desc));
            break;
        case MEMQUEUE_MSGS:
            msgs = memqueue_create_result((cli_binder_t *)ptr);
            json_object_object_add(resp_object, "results", msgs);
            break;
        case MEMQUEUE_MSG_ADDED:
            json_object_object_add(resp_object, "ret_desc",
                json_str(memqueue_ret[ret].ret_desc));
            memqueue = (memqueue_t*)ptr;
            json_object_object_add(resp_object, "rev",
                json_object_new_int(memqueue->rev));
        case MEMQUEUE_DELETED:
        case MEMQUEUE_CREATED:
        case MEMQUEUE_OK:
            break;
    }

    if ((tmp = strdup((char *)json_object_to_json_string(resp_object))) == NULL)
        return;
    http_respond(memqueue_ret[ret].resp_code, tmp, strlen(tmp));
    json_object_put(resp_object);
    free(tmp);
}

static json_object*
memqueue_create_result(cli_binder_t *cli_binder)
{
    json_object *resp_object = NULL;
    json_object *queue_object = NULL;
    json_object *messages_array = NULL;
    json_object *consumers_array = NULL;
    pending_msg_t *pending_msg = NULL;
    memqueue_binder_t *memqueue_binder = NULL;
    int include_consumers = 0;

    resp_object = json_object_new_object();

    TAILQ_FOREACH(pending_msg, &cli_binder->pending_msgs, next) {
        queue_object = json_object_object_get(resp_object,
            pending_msg->msg->memqueue->q_id);

        if (!queue_object) {
            queue_object = json_object_new_object();
            messages_array = json_object_new_array();
            json_object_object_add(queue_object, "messages", messages_array);
            json_object_object_add(queue_object, "rev",
                json_object_new_int(pending_msg->msg->memqueue->rev));
            json_object_object_add(resp_object,
                pending_msg->msg->memqueue->q_id, queue_object);

            /* check to see if we need to include the current consumers */
            LIST_FOREACH(memqueue_binder, &cli_binder->memqueues, b_next) {
                if (memqueue_binder->memqueue == pending_msg->msg->memqueue &&
                    memqueue_binder->include_consumers) {
                    include_consumers = 1;
                    break;
                }
            }

            if (include_consumers) {
                consumers_array = json_object_new_array();
                h_item_t *item = NULL;
                while ((item = h_next(pending_msg->msg->memqueue->consumers)))
                    json_object_array_add(consumers_array,
                        json_str(item->key));
                json_object_object_add(queue_object, "consumers",
                    consumers_array);
                include_consumers = 0;
            }

        } else {
            messages_array = json_object_object_get(queue_object, "messages");
        }

        json_object_array_add(messages_array,
            json_object_new_string_len(pending_msg->msg->data,
                pending_msg->msg->data_len));

    }

    return resp_object;
}

static int
memqueue_bind(memqueue_t *q, cli_binder_t *b, int rev, int include_consumers)
{
    memqueue_binder_t *tmp = NULL;

    if ((tmp = calloc(1, sizeof(memqueue_binder_t))) == NULL)
        return -1;

    tmp->memqueue = q;
    tmp->cli_binder = b;
    tmp->rev = rev;
    tmp->include_consumers = include_consumers;

    LIST_INSERT_HEAD(&b->memqueues, tmp, b_next);
    LIST_INSERT_HEAD(&q->memqueue_binders, tmp, q_next);

    return 0;
}

static void
memqueue_unbind_all(cli_binder_t *b)
{
    memqueue_binder_t *memqueue_binder = NULL;
    memqueue_binder_t *tmp = NULL;

    LIST_FOREACH_SAFE(memqueue_binder, &b->memqueues, b_next, tmp) {
        LIST_REMOVE(memqueue_binder, b_next);
        LIST_REMOVE(memqueue_binder, q_next);
    }
}

static memqueue_t *
memqueue_create(memqueue_create_args_t *args)
{
    memqueue_t *q = NULL;

    if ((q = calloc(1, sizeof(memqueue_t))) == NULL)
        return NULL;

    if ((q->consumers = h_init(128)) == NULL) {
        free (q);
        return NULL;
    }

    if ((q->q_id = strdup(args->q_id)) == NULL) {
        free(q);
        return NULL;
    }

    q->rev = 0;
    q->ref_count = 0;
    q->expiry = args->expiry;
    q->max_size = args->max_size;
    q->consumer_expiry = args->consumer_expiry;
    q->drop_from_head = args->drop_from_head;
    q->msgs_in_queue = 0;

    TAILQ_INIT(&q->msg_queue);
    LIST_INIT(&q->memqueue_binders);

    if (args->expiry > 0)
        obj_sched_expire(&memqueue_ins, q, args->expiry, MEMQUEUE_TYPE);

    /* always retain the q. If it doesn't expire it will stay alive, else
     * the expiry will expire it.
     */
    memqueue_retain(q);

    h_insert(memqueue_ins.queue_store, q->q_id, q);

    LOG_TRACE("Queue %s created successfully. "
        "(rev: %d, expiry: %d, max_size: %d, consumer_expiry: %d "
        "drop_from_head: %d)",
        q->q_id,
        q->rev,
        q->expiry,
        q->max_size,
        q->consumer_expiry,
        q->drop_from_head);

    return q;
}

static cli_binder_t*
cli_binder_create(uint32_t timeout)
{
    cli_binder_t *b = NULL;

    if ((b = calloc(1, sizeof(cli_binder_t))) == NULL)
        return NULL;

    b->lt = lthread_current();
    b->timeout = timeout;
    lthread_cond_create(&b->cond);
    LIST_INIT(&b->memqueues);
    TAILQ_INIT(&b->pending_msgs);

    return b;
}

static void
cli_binder_free(cli_binder_t *b)
{
    free(b->cond);
    free(b);
}

static int
cli_binder_enqueue_msg(cli_binder_t *cli_binder, msg_t *msg)
{
    pending_msg_t *pending_msg = NULL;

    if ((pending_msg = calloc(1, sizeof(pending_msg_t))) == NULL)
        return -1;
    pending_msg->msg = msg;

    TAILQ_INSERT_TAIL(&cli_binder->pending_msgs, pending_msg, next);
    memqueue_msg_retain(msg);

    return 0;
}

static void
cli_wakeup(cli_binder_t *cli_binder)
{
    lthread_cond_signal(cli_binder->cond);
}

static int
memqueue_msg_create(memqueue_t *q, int32_t msecs, void *data,
    uint64_t data_len)
{
    void *buf = NULL;
    msg_t *new_msg = NULL;
    memqueue_binder_t *memqueue_binder = NULL;

    if ((buf = malloc(data_len)) == NULL)
        return -1;

    if ((new_msg = calloc(1, sizeof(msg_t))) == NULL) {
        free(buf);
        return -1;
    }

    q->rev++;

    memcpy(buf, data, data_len);
    new_msg->data = buf;
    new_msg->data_len = data_len;
    new_msg->memqueue = q;
    new_msg->rev = q->rev;
    new_msg->ref_count = 0;
    new_msg->new = 1;
    new_msg->expiry = msecs;
    new_msg->msg_id = ++memqueue_ins.unique_msg_id;

    if (msecs > 0) {
        obj_sched_expire(&memqueue_ins, new_msg, msecs, MSG_TYPE);
        /* only retain msg if it has an expiry. If it doesn't it will
         * get consumed by consumers and freed immediately.
         */
        memqueue_msg_retain(new_msg);
    }

    if (msecs == -1)
        memqueue_msg_retain(new_msg);

    LOG_TRACE("A new message %llu in queue %s created successfully."
        "(rev: %d, expiry %d, data_len: %d)",
        new_msg->msg_id,
        q->q_id,
        new_msg->rev,
        new_msg->expiry,
        new_msg->data_len);

    TAILQ_INSERT_TAIL(&q->msg_queue, new_msg, next);
    q->msgs_in_queue++;

    LIST_FOREACH(memqueue_binder, &q->memqueue_binders, q_next) {
        //printf("binder rev: %d, q->rev: %d\n", memqueue_binder->rev, q->rev);
        if (memqueue_binder->rev <= q->rev) {
            cli_binder_enqueue_msg(memqueue_binder->cli_binder, new_msg);
            cli_wakeup(memqueue_binder->cli_binder);
        }
    }

    return 0;
}

static void
memqueue_msg_free(msg_t *msg)
{
    free(msg->data);
    free(msg);
}

static void
cli_binder_release_pending_msgs(cli_binder_t *cli_binder)
{
    pending_msg_t *pending_msg = NULL;
    pending_msg_t *pending_msg_tmp = NULL;

    TAILQ_FOREACH_SAFE(pending_msg, &cli_binder->pending_msgs, next,
        pending_msg_tmp) {
        memqueue_msg_release(pending_msg->msg);
        TAILQ_REMOVE(&cli_binder->pending_msgs, pending_msg, next);
        free(pending_msg);
    }
}

static int
cli_binder_has_pending_msgs(cli_binder_t *cli_binder)
{
    return !TAILQ_EMPTY(&cli_binder->pending_msgs);
}

static int
cli_binder_enqueue_msgs(cli_binder_t *cli_binder, memqueue_t *q,
    int rev, int latest)
{
    msg_t *msg = NULL;
    int total_enqueued = 0;

    TAILQ_FOREACH(msg, &q->msg_queue, next) {
        //printf("user rev %d <= %d\n", rev, msg->rev);
        if ((rev < msg->rev) || (latest && msg->new == 1)) {
            cli_binder_enqueue_msg(cli_binder, msg);
            total_enqueued++;
        }
    }

    return total_enqueued;
}

static void
memqueue_refresh_consumer(memqueue_t *q, char *consumer_id)
{
    consumer_t *consumer = NULL;
    char *tmp = NULL;

    consumer = h_get(q->consumers, consumer_id);

    if (consumer) {
        consumer_resched(&memqueue_ins, consumer);
    } else {
        if ((consumer = calloc(1, sizeof(consumer_t))) == NULL)
            return;
        consumer->memqueue = q;
        consumer->expiry = q->consumer_expiry;
        if ((consumer->consumer_id = strdup(consumer_id)) == NULL) {
            free(consumer);
            return;
        }
        h_insert(q->consumers, consumer_id, consumer);
        asprintf(&tmp, "%s:joined", consumer_id);
        memqueue_msg_create(q, 2000, tmp, strlen(tmp));
        free(tmp);
        /* retain queue so that when the consumer expires
         * the queue will still be there. the consumer
         * has to release the queue afterwards.
         */
        memqueue_retain(q);
        obj_sched_expire(&memqueue_ins, consumer, q->consumer_expiry,
            CONSUMER_TYPE);
    }
}

static void
obj_cleaner(void *arg)
{
    uint32_t sleep = 0;
    lthread_detach();
    while (1) {
        if (sleep != 1)
            lthread_cond_wait(memqueue_ins.cond, sleep);
        objs_expire(&memqueue_ins, MSG_TYPE);
        objs_expire(&memqueue_ins, MEMQUEUE_TYPE);
        objs_expire(&memqueue_ins, CONSUMER_TYPE);
        sleep = sched_get_min_timeout(&memqueue_ins);
        //printf("sleep is %u\n", sleep);
    }
}

void
consumer_expired(consumer_t *consumer)
{
    char *tmp = NULL;
    asprintf(&tmp, "%s:expired", consumer->consumer_id);
    memqueue_msg_create(consumer->memqueue, 2000, tmp, strlen(tmp));
    free(tmp);
    memqueue_release(consumer->memqueue);
    memqueue_free_consumer(consumer);
}

static int
on_queue_create(h_hash_t *args)
{
    memqueue_create_args_t user_args;
    args_memqueue_create_populate(&user_args, args);

    memqueue_t *q = h_get(memqueue_ins.queue_store, user_args.q_id);
    if (q) {
        memqueue_respond(MEMQUEUE_EXISTS, NULL);
        return 0;
    }

    memqueue_create(&user_args);

    memqueue_respond(MEMQUEUE_CREATED, NULL);

    return 0;
}

static int
on_msg_post(h_hash_t *args)
{
    char *q_id = h_get(args, "q_id");
    int32_t expiry = 0;
    char *expiry_str = http_get_querystring("expiry");

    if (expiry_str)
        expiry = atoi(expiry_str);

    memqueue_t *q = h_get(memqueue_ins.queue_store, q_id);
    if (!q) {
        memqueue_respond(MEMQUEUE_NOT_FOUND, NULL);
        return 0;
    }

    if (is_memqueue_full(q) != 1) {
        memqueue_msg_create(q, expiry, http_get_body(), http_get_body_len());
        memqueue_respond(MEMQUEUE_MSG_ADDED, q);
    } else {
        memqueue_respond(MEMQUEUE_FULL, NULL);
    }

    memqueue_resched(&memqueue_ins, q);

    return 0;
}

static int
is_memqueue_full(memqueue_t *q)
{
    if (q->max_size <= 0)
        return 0;

    if (q->max_size > q->msgs_in_queue)
        return 0;

    if (q->max_size &&
        q->msgs_in_queue >= q->max_size &&
        q->drop_from_head) {
        memqueue_msg_retain(TAILQ_FIRST(&q->msg_queue));
        memqueue_msg_release(TAILQ_FIRST(&q->msg_queue));
        return 0;
    }

    return 1;
}

static int
on_queue_delete(h_hash_t *args)
{
    char *q_id = h_get(args, "q_id");
    memqueue_t *q = h_get(memqueue_ins.queue_store, q_id);
    if (!q) {
        memqueue_respond(MEMQUEUE_NOT_FOUND, NULL);
        return 0;
    }

    memqueue_respond(MEMQUEUE_DELETED, NULL);
    memqueue_release(q);

    return 0;
}

static int
on_serv_file(h_hash_t *args)
{
    char *filename = h_get(args, "filename");
    char *filepath = NULL;

    return -2;
    if (asprintf(&filepath, "/tmp/%s", filename) == -1)
        return -2;

    int fd = open(filepath, O_RDONLY, 0);

    if (!fd) {
        free(filepath);
        return -2;
    }

    http_sendfile(200, fd);

    close(fd);
    free(filepath);

    return 0;
}

static int
on_queue_poll(h_hash_t *args)
{
    poll_args_t user_args;

    args_memqueue_poll_populate(&user_args, args);

    return memqueue_poll_queue(&user_args);

}

static int
on_multi_queue_poll(h_hash_t *args)
{
    poll_args_t user_args;

    args_memqueue_poll_populate(&user_args, NULL);

    return memqueue_poll_queue(&user_args);
}

int
memqueue_init(void)
{
    if ((memqueue_ins.queue_store = h_init(128)) == NULL)
        return -1;

    char tmp[256];
    sprintf(tmp, "%s_%s", "memqueue", "app_dbg");
    memqueue_ins.birth = rdtsc();
    lthread_cond_create(&memqueue_ins.cond);
    lthread_create(&memqueue_ins.lt_sleep, obj_cleaner, NULL);

    http_route_on("GET", "/(?<filename>.+)/?", on_serv_file);
    http_route_on("GET", "/(?<q_id>.+)/?", on_queue_poll);
    http_route_on("GET", "/mpoll/?", on_multi_queue_poll);
    http_route_on("PUT", "/(?<q_id>.+)/?", on_queue_create);
    http_route_on("POST", "/(?<q_id>.*)/?", on_msg_post);
    http_route_on("DELETE", "/(?<q_id>.*)/?", on_queue_delete);

    return 0;
}
