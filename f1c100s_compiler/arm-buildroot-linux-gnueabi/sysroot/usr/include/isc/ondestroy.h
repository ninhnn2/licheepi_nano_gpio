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

#ifndef ISC_ONDESTROY_H
#define ISC_ONDESTROY_H 1

#include <isc/lang.h>
#include <isc/types.h>

ISC_LANG_BEGINDECLS

/*! \file isc/ondestroy.h
 * ondestroy handling.
 *
 * Any class ``X'' of objects that wants to send out notifications
 * on its destruction should declare a field of type isc_ondestroy_t
 * (call it 'ondest').
 *
 * \code
 * 	typedef struct {
 * 		...
 * 		isc_ondestroy_t	ondest;
 * 		...
 * 	} X;
 * \endcode
 *
 * When an object ``A'' of type X is created
 * it must initialize the field ondest with a call to
 *
 * \code
 * 	isc_ondestroy_init(&A->ondest).
 * \endcode
 *
 * X should also provide a registration function for third-party
 * objects to call to register their interest in being told about
 * the destruction of a particular instance of X.
 *
 * \code
 *	isc_result_t
 * 	X_ondestroy(X *instance, isc_task_t *task,
 * 		     isc_event_t **eventp) {
 * 		return(isc_ondestroy_register(&instance->ondest, task,eventp));
 * 	}
 * \endcode
 *
 *	Note: locking of the ondestory structure embedded inside of X, is
 * 	X's responsibility.
 *
 * When an instance of X is destroyed, a call to  isc_ondestroy_notify()
 * sends the notifications:
 *
 * \code
 *	X *instance;
 *	isc_ondestroy_t ondest = instance->ondest;
 *
 *	... completely cleanup 'instance' here...
 *
 * 	isc_ondestroy_notify(&ondest, instance);
 * \endcode
 *
 *
 * see lib/dns/zone.c for an ifdef'd-out example.
 */

struct isc_ondestroy {
	unsigned int magic;
	isc_eventlist_t events;
};

void
isc_ondestroy_init(isc_ondestroy_t *ondest);
/*%<
 * Initialize the on ondest structure. *must* be called before first call
 * to isc_ondestroy_register().
 */

isc_result_t
isc_ondestroy_register(isc_ondestroy_t *ondest, isc_task_t *task,
		       isc_event_t **eventp);

/*%<
 * Stores task and *eventp away inside *ondest.  Ownership of **event is
 * taken from the caller (and *eventp is set to NULL). The task is attached
 * to.
 */

void
isc_ondestroy_notify(isc_ondestroy_t *ondest, void *sender);
/*%<
 * Dispatches the event(s) to the task(s) that were given in
 * isc_ondestroy_register call(s) (done via calls to
 * isc_task_sendanddetach()).  Before dispatch, the sender value of each
 * event structure is set to the value of the sender parameter. The
 * internal structures of the ondest parameter are cleaned out, so no other
 * cleanup is needed.
 */

ISC_LANG_ENDDECLS

#endif /* ISC_ONDESTROY_H */
