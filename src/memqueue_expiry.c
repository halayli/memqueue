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

  memqueue_expiry.c
*/
#include <assert.h>
#include "memqueue_impl.h" 
#include "time.h" 

static expiry_node_t* rb_search(struct rb_root *root, uint32_t msecs);
static int rb_insert(struct rb_root *root, expiry_node_t *data);
static void possibly_node_free(expiry_node_t **data, struct rb_root *root);

static expiry_node_t*
rb_search(struct rb_root *root, uint32_t msecs)
{
    struct rb_node *node = root->rb_node;
    expiry_node_t *data = NULL;

    while (node) {
        data = container_of(node, expiry_node_t, node);

        if (msecs < data->msecs)
            node = node->rb_left;
        else if (msecs > data->msecs)
            node = node->rb_right;
        else
            return data;
    }

    return NULL;
}

static int
rb_insert(struct rb_root *root, expiry_node_t *data)
{
    expiry_node_t *this = NULL;
    struct rb_node **new = &(root->rb_node), *parent = NULL;

    while (*new)
    {
        parent = *new;
        this = container_of(parent, expiry_node_t, node);

        if (this->msecs > data->msecs)
            new = &((*new)->rb_left);
        else if (this->msecs < data->msecs)
            new = &((*new)->rb_right);
        else {
            assert(0);
            return -1;
        }
    }

    rb_link_node(&data->node, parent, new);
    rb_insert_color(&data->node, root);

    return 0;
}

void
obj_sched_expire(memqueue_ins_t *ins, void *obj, uint32_t msecs,
    obj_type_t type)
{
    expiry_node_t *tmp = NULL;
    struct rb_root *root = NULL;
    uint32_t t_diff_msecs = 0;
    int ret = 0;
    uint32_t min_timeout = sched_get_min_timeout(ins);

    if (type == MSG_TYPE)
        root = &ins->msg_expiry;
    else if (type == MEMQUEUE_TYPE)
        root = &ins->memqueue_expiry;
    else if (type == CONSUMER_TYPE)
        root = &ins->consumer_expiry;

    t_diff_msecs = tick_diff_msecs(ins->birth, rdtsc()) + msecs;
    tmp = rb_search(root, t_diff_msecs);

    /*printf("t_diff_msecs is %u, sched_get_min_timeout is %u, msecs is %u\n",
        t_diff_msecs, min_timeout, msecs);*/

    if (min_timeout == 0 || min_timeout > msecs)
        lthread_cond_signal(ins->cond);

    if (tmp == NULL &&
        (tmp = calloc(1, sizeof(expiry_node_t))) != NULL) {
        tmp->msecs = t_diff_msecs;
        ret = rb_insert(root, tmp);
        assert(ret != -1);

        if (type == MSG_TYPE) {
            LIST_INIT(&tmp->msg_objs);
            LIST_INSERT_HEAD(&tmp->msg_objs, (msg_t *)obj, expiry_next);
            ((msg_t *)obj)->expiry_node = tmp;
        } else if (type == MEMQUEUE_TYPE) {
            LIST_INIT(&tmp->memqueue_objs);
            LIST_INSERT_HEAD(&tmp->memqueue_objs, (memqueue_t *)obj,
                expiry_next);
            ((memqueue_t *)obj)->expiry_node = tmp;
        } else if (type == CONSUMER_TYPE) {
            LIST_INIT(&tmp->consumer_objs);
            LIST_INSERT_HEAD(&tmp->consumer_objs, (consumer_t *)obj,
                expiry_next);
            ((consumer_t *)obj)->expiry_node = tmp;
        }

        return;
    }

    if (tmp) {
        if (type == MSG_TYPE)
            LIST_INSERT_HEAD(&tmp->msg_objs, (msg_t *)obj, expiry_next);
        else if (type == MEMQUEUE_TYPE)
            LIST_INSERT_HEAD(&tmp->memqueue_objs, (memqueue_t *)obj,
                expiry_next);
        else if (type == CONSUMER_TYPE)
            LIST_INSERT_HEAD(&tmp->consumer_objs, (consumer_t *)obj,
                expiry_next);
    } else {
        assert(0);
    }

}

uint32_t
sched_get_min_timeout(memqueue_ins_t *ins)
{
    struct rb_root *roots[] = {&ins->msg_expiry, &ins->memqueue_expiry,
        &ins->consumer_expiry};
    struct rb_node *node = NULL;
    expiry_node_t *data = NULL;
    uint32_t t_diff_msecs = 0, min = 0, msecs = 0;
    int i = 0;
    int found = 0;

    t_diff_msecs = tick_diff_msecs(ins->birth, rdtsc());

    for (i = 0; i < 3; i++) {
        node = roots[i]->rb_node;
        while (node) {
            data = container_of(node, expiry_node_t, node);
            msecs = data->msecs;
            node = node->rb_left;
            found = 1;
        }

        if (!found)
            continue;

        //printf("min found i %d  %d\n", i, msecs);

        if (msecs > t_diff_msecs) {
            if (min > (msecs - t_diff_msecs) || min == 0)
                min = msecs - t_diff_msecs;
        } else {
            return 1;
        }

        found = 0;
    }

    //printf("min is %d\n",min);

    return min;
}

void
objs_expire(memqueue_ins_t *ins, obj_type_t type)
{
    struct rb_root *root = NULL;
    uint32_t t_diff_msecs = 0;
    expiry_node_t *data = NULL;
    msg_t *msg = NULL;
    msg_t *msg_tmp = NULL;
    memqueue_t *memqueue = NULL;
    memqueue_t *memqueue_tmp = NULL;
    consumer_t *consumer = NULL;
    consumer_t *consumer_tmp = NULL;
    struct rb_node *node = NULL;

    if (type == MSG_TYPE)
        root = &ins->msg_expiry;
    else if (type == MEMQUEUE_TYPE)
        root = &ins->memqueue_expiry;
    else if (type == CONSUMER_TYPE)
        root = &ins->consumer_expiry;

    node = root->rb_node;

    t_diff_msecs = tick_diff_msecs(ins->birth, rdtsc());

    while (node) {
        data = container_of(node, expiry_node_t, node);
        node = node->rb_left;

        if (data->msecs <= t_diff_msecs) {
            if (type == MSG_TYPE) {
                LIST_FOREACH_SAFE(msg, &data->msg_objs, expiry_next, msg_tmp) {
                    LIST_REMOVE(msg, expiry_next);
                    msg_release(msg);
                    msg->expiry_node = NULL;
                }
            } else if (type == MEMQUEUE_TYPE) {
                LIST_FOREACH_SAFE(memqueue, &data->memqueue_objs, expiry_next,
                    memqueue_tmp) {
                    LIST_REMOVE(memqueue, expiry_next);
                    memqueue_release(memqueue);
                    memqueue->expiry_node = NULL;
                }
            } else if (type == CONSUMER_TYPE) {
                LIST_FOREACH_SAFE(consumer, &data->consumer_objs, expiry_next,
                    consumer_tmp) {
                    LIST_REMOVE(consumer, expiry_next);
                    consumer_expired(consumer);
                    consumer->expiry_node = NULL;
                }
            }

            possibly_node_free(&data, root);
        }
    }
}

void
memqueue_resched(memqueue_ins_t *ins, memqueue_t *memqueue)
{
   
    if (!memqueue->expiry_node)
        return;

    LIST_REMOVE(memqueue, expiry_next);
    possibly_node_free(&memqueue->expiry_node, &ins->memqueue_expiry);

    obj_sched_expire(ins, memqueue, memqueue->expiry, MEMQUEUE_TYPE);
}

void
consumer_resched(memqueue_ins_t *ins, consumer_t *consumer)
{
   
    if (!consumer->expiry_node)
        return;

    LIST_REMOVE(consumer, expiry_next);
    possibly_node_free(&consumer->expiry_node, &ins->consumer_expiry);

    obj_sched_expire(ins, consumer, consumer->expiry, CONSUMER_TYPE);
}

static void
possibly_node_free(expiry_node_t **data, struct rb_root *root)
{
    if (LIST_EMPTY(&(*data)->memqueue_objs) &&
        LIST_EMPTY(&(*data)->msg_objs) &&
        LIST_EMPTY(&(*data)->consumer_objs)) {
        rb_erase(&(*data)->node, root);
        free(*data);
        *data = NULL;
    }
}
