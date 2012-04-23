memqueue
========

Introduction
------------

memqueue is an in-memory queue server. Its goal is to share messages across multiple consumers polling from the same queue where each consumer can consume at their own pace without missing messages. This is made possible by using message expiry and queue revisions.

memqueue queues get a new revision each time a message is inserted. Consumers can specify which revision they want to poll from, allowing them to consume 
messages that have already been consumed but are not expired yet.

## Example

A good example that illustrates memqueue's features is a group chat server over HTTP.
We'll create a single queue for the group. Each user will poll from this queue waiting for new messages and when a new message arrives, the queue revision is bumped by one and the message get sent to all consumers along with the latest revision. During the time a consumer is consuming a message and reconnecting, new messages can come in and the queue revision can be bumped by few digits. This is not an issue because the next time each of the consumers connect, they'll provide the revision they are at and the poll will retrieve all the messages they have missed after this revision.

# memqueue REST API 

memqueue has an HTTP REST interface and runs its own HTTP server built on top of [lthread](https://github.com/halayli/lthread/). Websocket support is on the TODO list.

###Create a new queue in memory

    PUT /<queue_name>

**parameters**

`expiry: Integer (ms)`. (Optional)

Milliseconds of queue inactivity before it get's removed. Queue activity can be either queue polling or posting a new message.

`max_size: Integer`. (Optional)

Number of messages a queue will hold before it either starts dropping messages from head or rejects messages depending on the parameter *drop_from_head*.

`drop_from_head: Boolean (0 or 1)`. (Optional)

if *max_size* is specified, and the queue is full, this parameter specifies whether to drop a message from head and insert  a new one from tail or reject the message.

`consumer_expiry: Integer (ms)`. (optional)

If set, consumers can specify their `<consumer_id>` when polling to inform other consumers that `<consumer_id>` started polling. memqueue only inform other consumers about *new* consumers. A consumer is considered new if he hasn't been seen polling in the last *consumer_expiry* period.

###Post a message to `<queue_name>`

    POST /<queue_name>

**Parameters**

`expiry: Integer (ms)`. (optional)

Message `data` must be included in the HTTP POST body

###Poll messages from `<queue_name>`

    GET /<queue_name>

**Parameters**

`rev: Integer`. (optional)

`timeout: Integer (ms)`. (optional)

`latest: Boolean(0 or 1)`. (optional)

`consumer_id: String`. (optional)

`include_consumers: Boolean(0 or 1)`. (Optional)

###Poll from multiple queues

    GET /mpoll

**Parameters**

Takes the same parameters as GET / `<queue_name>` with a -`<n>` appended to the parameter where `<n>` is an integer grouping each of the queue parameters.

`total_queues: Integer`. (required)

Specifies the total number of queues you are polling from.

###Delete queue `<queue_name>`

    DELETE /<queue_name>
