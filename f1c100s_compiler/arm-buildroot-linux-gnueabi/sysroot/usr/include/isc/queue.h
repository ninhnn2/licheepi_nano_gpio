/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * See the COPYRIGHT file distributed with this work for additional
 * information regarding copyright ownership.
 */


/*
 * This is a generic implementation of a two-lock concurrent queue.
 * There are built-in mutex locks for the head and tail of the queue,
 * allowing elements to be safely added and removed at the same time.
 *
 * NULL is "end of list"
 * -1 is "not linked"
 */

#ifndef ISC_QUEUE_H
#define ISC_QUEUE_H 1

#include <stdbool.h>

#include <isc/assertions.h>
#include <isc/mutex.h>

#ifdef ISC_QUEUE_CHECKINIT
#define ISC_QLINK_INSIST(x) ISC_INSIST(x)
#else
#define ISC_QLINK_INSIST(x) (void)0
#endif

#define ISC_QLINK(type) struct { type *prev, *next; }

#define ISC_QLINK_INIT(elt, link) \
	do { \
		(elt)->link.next = (elt)->link.prev = (void *)(-1); \
	} while(0)

#define ISC_QLINK_LINKED(elt, link) ((void*)(elt)->link.next != (void*)(-1))

#define ISC_QUEUE(type) struct { \
	type *head, *tail; \
	isc_mutex_t headlock, taillock; \
}

#define ISC_QUEUE_INIT(queue, link) \
	do { \
		(void) isc_mutex_init(&(queue).taillock); \
		(void) isc_mutex_init(&(queue).headlock); \
		(queue).tail = (queue).head = NULL; \
	} while (0)

#define ISC_QUEUE_EMPTY(queue) ((queue).head == NULL)

#define ISC_QUEUE_DESTROY(queue) \
	do { \
		ISC_QLINK_INSIST(ISC_QUEUE_EMPTY(queue)); \
		(void) isc_mutex_destroy(&(queue).taillock); \
		(void) isc_mutex_destroy(&(queue).headlock); \
	} while (0)

/*
 * queues are meant to separate the locks at either end.  For best effect, that
 * means keeping the ends separate - i.e. non-empty queues work best.
 *
 * a push to an empty queue has to take the pop lock to update
 * the pop side of the queue.
 * Popping the last entry has to take the push lock to update
 * the push side of the queue.
 *
 * The order is (pop, push), because a pop is presumably in the
 * latency path and a push is when we're done.
 *
 * We do an MT hot test in push to see if we need both locks, so we can
 * acquire them in order.  Hopefully that makes the case where we get
 * the push lock and find we need the pop lock (and have to release it) rare.
 *
 * > 1 entry - no collision, push works on one end, pop on the other
 *   0 entry - headlock race
 *     pop wins - return(NULL), push adds new as both head/tail
 *     push wins - updates head/tail, becomes 1 entry case.
 *   1 entry - taillock race
 *     pop wins - return(pop) sets head/tail NULL, becomes 0 entry case
 *     push wins - updates {head,tail}->link.next, pop updates head
 *                 with new ->link.next and doesn't update tail
 *
 */
#define ISC_QUEUE_PUSH(queue, elt, link) \
	do { \
		bool headlocked = false; \
		ISC_QLINK_INSIST(!ISC_QLINK_LINKED(elt, link)); \
		LOCK(&(queue).taillock); \
		if ((queue).tail == NULL) { \
			UNLOCK(&(queue).taillock); \
			LOCK(&(queue).headlock); \
			LOCK(&(queue).taillock); \
			headlocked = true; \
		} \
		(elt)->link.prev = (queue).tail; \
		(elt)->link.next = NULL; \
		if ((queue).tail != NULL) \
			(queue).tail->link.next = (elt); \
		(queue).tail = (elt); \
		UNLOCK(&(queue).taillock); \
		if (headlocked) { \
			if ((queue).head == NULL) \
				(queue).head = (elt); \
			UNLOCK(&(queue).headlock); \
		} \
	} while (0)

#define ISC_QUEUE_POP(queue, link, ret) \
	do { \
		LOCK(&(queue).headlock); \
		ret = (queue).head; \
		while (ret != NULL) { \
			if (ret->link.next == NULL) { \
				LOCK(&(queue).taillock); \
				if (ret->link.next == NULL) { \
					(queue).head = (queue).tail = NULL; \
					UNLOCK(&(queue).taillock); \
					break; \
				}\
				UNLOCK(&(queue).taillock); \
			} \
			(queue).head = ret->link.next; \
			(queue).head->link.prev = NULL; \
			break; \
		} \
		UNLOCK(&(queue).headlock); \
		if (ret != NULL) \
			(ret)->link.next = (ret)->link.prev = (void *)(-1); \
	} while(0)

#define ISC_QUEUE_UNLINK(queue, elt, link) \
	do { \
		ISC_QLINK_INSIST(ISC_QLINK_LINKED(elt, link)); \
		LOCK(&(queue).headlock); \
		LOCK(&(queue).taillock); \
		if ((elt)->link.prev == NULL) \
			(queue).head = (elt)->link.next; \
		else \
			(elt)->link.prev->link.next = (elt)->link.next; \
		if ((elt)->link.next == NULL) \
			(queue).tail = (elt)->link.prev; \
		else \
			(elt)->link.next->link.prev = (elt)->link.prev; \
		UNLOCK(&(queue).taillock); \
		UNLOCK(&(queue).headlock); \
		(elt)->link.next = (elt)->link.prev = (void *)(-1); \
	} while(0)

#define ISC_QUEUE_UNLINKIFLINKED(queue, elt, link) \
	do { \
		LOCK(&(queue).headlock); \
		LOCK(&(queue).taillock); \
		if (ISC_QLINK_LINKED(elt, link)) { \
			if ((elt)->link.prev == NULL) \
				(queue).head = (elt)->link.next; \
			else \
				(elt)->link.prev->link.next = (elt)->link.next; \
			if ((elt)->link.next == NULL) \
				(queue).tail = (elt)->link.prev; \
			else \
				(elt)->link.next->link.prev = (elt)->link.prev; \
		} \
		UNLOCK(&(queue).taillock); \
		UNLOCK(&(queue).headlock); \
		(elt)->link.next = (elt)->link.prev = (void *)(-1); \
	} while(0)

#endif /* ISC_QUEUE_H */
