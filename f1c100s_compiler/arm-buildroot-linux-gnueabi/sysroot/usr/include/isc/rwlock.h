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


#ifndef ISC_RWLOCK_H
#define ISC_RWLOCK_H 1

#include <inttypes.h>

/*! \file isc/rwlock.h */

#include <isc/condition.h>
#include <isc/lang.h>
#include <isc/platform.h>
#include <isc/types.h>

#if defined(ISC_PLATFORM_HAVESTDATOMIC)
#if defined(__cplusplus)
#include <isc/stdatomic.h>
#else
#include <stdatomic.h>
#endif
#endif

ISC_LANG_BEGINDECLS

typedef enum {
	isc_rwlocktype_none = 0,
	isc_rwlocktype_read,
	isc_rwlocktype_write
} isc_rwlocktype_t;

#ifdef ISC_PLATFORM_USETHREADS
# if defined(ISC_PLATFORM_HAVESTDATOMIC)
#  define ISC_RWLOCK_USEATOMIC 1
#  define ISC_RWLOCK_USESTDATOMIC 1
# else /* defined(ISC_PLATFORM_HAVESTDATOMIC) */
#  if defined(ISC_PLATFORM_HAVEXADD) && defined(ISC_PLATFORM_HAVECMPXCHG)
#   define ISC_RWLOCK_USEATOMIC 1
#  endif
# endif /* defined(ISC_PLATFORM_HAVESTDATOMIC) */

struct isc_rwlock {
	/* Unlocked. */
	unsigned int		magic;
	isc_mutex_t		lock;

#if defined(ISC_RWLOCK_USEATOMIC)
	/*
	 * When some atomic instructions with hardware assistance are
	 * available, rwlock will use those so that concurrent readers do not
	 * interfere with each other through mutex as long as no writers
	 * appear, massively reducing the lock overhead in the typical case.
	 *
	 * The basic algorithm of this approach is the "simple
	 * writer-preference lock" shown in the following URL:
	 * http://www.cs.rochester.edu/u/scott/synchronization/pseudocode/rw.html
	 * but our implementation does not rely on the spin lock unlike the
	 * original algorithm to be more portable as a user space application.
	 */

	/* Read or modified atomically. */
#if defined(ISC_RWLOCK_USESTDATOMIC)
	atomic_int_fast32_t	spins;
	atomic_int_fast32_t	write_requests;
	atomic_int_fast32_t	write_completions;
	atomic_int_fast32_t	cnt_and_flag;
	atomic_int_fast32_t	write_granted;
#else
	int32_t		spins;
	int32_t		write_requests;
	int32_t		write_completions;
	int32_t		cnt_and_flag;
	int32_t		write_granted;
#endif

	/* Locked by lock. */
	isc_condition_t		readable;
	isc_condition_t		writeable;
	unsigned int		readers_waiting;

	/* Unlocked. */
	unsigned int		write_quota;

#else  /* ISC_RWLOCK_USEATOMIC */

	/*%< Locked by lock. */
	isc_condition_t		readable;
	isc_condition_t		writeable;
	isc_rwlocktype_t	type;

	/*% The number of threads that have the lock. */
	unsigned int		active;

	/*%
	 * The number of lock grants made since the lock was last switched
	 * from reading to writing or vice versa; used in determining
	 * when the quota is reached and it is time to switch.
	 */
	unsigned int		granted;

	unsigned int		spins;
	unsigned int		readers_waiting;
	unsigned int		writers_waiting;
	unsigned int		read_quota;
	unsigned int		write_quota;
	isc_rwlocktype_t	original;
#endif  /* ISC_RWLOCK_USEATOMIC */
};
#else /* ISC_PLATFORM_USETHREADS */
struct isc_rwlock {
	unsigned int		magic;
	isc_rwlocktype_t	type;
	unsigned int		active;
};
#endif /* ISC_PLATFORM_USETHREADS */


isc_result_t
isc_rwlock_init(isc_rwlock_t *rwl, unsigned int read_quota,
		unsigned int write_quota);

isc_result_t
isc_rwlock_lock(isc_rwlock_t *rwl, isc_rwlocktype_t type);

isc_result_t
isc_rwlock_trylock(isc_rwlock_t *rwl, isc_rwlocktype_t type);

isc_result_t
isc_rwlock_unlock(isc_rwlock_t *rwl, isc_rwlocktype_t type);

isc_result_t
isc_rwlock_tryupgrade(isc_rwlock_t *rwl);

void
isc_rwlock_downgrade(isc_rwlock_t *rwl);

void
isc_rwlock_destroy(isc_rwlock_t *rwl);

ISC_LANG_ENDDECLS

#endif /* ISC_RWLOCK_H */
