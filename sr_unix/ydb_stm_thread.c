/****************************************************************
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#ifdef YDB_USE_POSIX_TIMERS
#include <sys/syscall.h>	/* for "syscall" */
#endif

#include "gtm_semaphore.h"

#include "libyottadb_int.h"
#include "mdq.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab_int4.h"     /* needed for tp.h */
#include "buddy_list.h"
#include "tp.h"
#include "gtm_multi_thread.h"
#include "caller_id.h"
#include "gtmci.h"
#include "gtm_exit_handler.h"
#include "memcoherency.h"
#include "sig_init.h"

GBLREF	boolean_t	simpleThreadAPI_active;
GBLREF	pthread_t	gtm_main_thread_id;
GBLREF	boolean_t	gtm_main_thread_id_set;
GBLREF	uint64_t 	stmTPToken;			/* Counter used to generate unique token for SimpleThreadAPI TP */
GBLREF	int		stapi_signal_handler_deferred;
GBLREF	pthread_mutex_t	ydb_engine_threadsafe_mutex[];
GBLREF	pthread_t	ydb_engine_threadsafe_mutex_holder[];
#ifdef YDB_USE_POSIX_TIMERS
GBLREF	pid_t		posix_timer_thread_id;
GBLREF	boolean_t	posix_timer_created;
#endif

/* Routine to manage worker thread(s) for the *_st() interface routines (Simple Thread API aka the
 * Simple Thread Method). Note for the time being, only one worker thread is created. In the future,
 * if/when YottaDB becomes fully-threaded, more worker threads may be allowed.
 *
 * Note there is no parameter or return value from this routine currently (both passed as NULL). The
 * routine signature is dictated by this routine being driven by pthread_create().
 */
void *ydb_stm_thread(void *parm)
{
	int			sig_num, status, tLevel;
	pthread_t		mutex_holder_thread_id, prev_wake_up_thread_id;
	enum sig_handler_t	sig_handler_type;
	uint64_t		i, prev_wake_up_i, prev_diff_i;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Now that we are establishing this main work queue thread, we need to make sure all timers and checks done by
	 * YottaDB *and* user code deal with THIS thread and not some other random thread.
	 */
	assert(gtm_main_thread_id_set);
	assert(!simpleThreadAPI_active);
	gtm_main_thread_id = pthread_self();
	INITIALIZE_THREAD_MUTEX_IF_NEEDED; /* Initialize thread-mutex variables if not already done */
#	ifdef YDB_USE_POSIX_TIMERS
	assert(0 == posix_timer_created);
	assert(0 == posix_timer_thread_id);
	posix_timer_thread_id = syscall(SYS_gettid);
#	endif
	SHM_WRITE_MEMORY_BARRIER;
	simpleThreadAPI_active = TRUE;	/* to indicate to caller/creator thread that we are done with setup */
	/* Note: This MAIN worker thread runs indefinitely till the process exits or a "ydb_exit" is done */
	for (i = 1, prev_wake_up_i = 0, prev_diff_i = 1; ydb_init_complete; i++)
	{
		assert(ydb_engine_threadsafe_mutex_holder[0] != pthread_self());
		SLEEP_USEC(1, TRUE);	/* Sleep for 1 micro-second; TRUE to indicate if system call is interrupted,
					 * restart the sleep. This way we are sure each iteration sleeps for at least
					 * 1 micro-second. This guarantees that "i" (which is a 64-bit quantity)
					 * can never overflow in half-a-million years which is okay since that is
					 * an impossibly high span of time. If the sleep instead ended up being less,
					 * the span of time needed to overflow "i" could get more likely.
					 */
		if (stapi_signal_handler_deferred)
		{	/* A signal handler was deferred. Try getting the YottaDB engine multi-thread mutex lock to
			 * see if we can invoke the signal handler in this thread. If we cannot get the lock, forward
			 * the signal to the thread that currently holds the ydb engine mutex lock in the hope that
			 * it can handle the signal right away. In any case, keep retrying this in a loop periodically
			 * so we avoid potential hangs due to indefinitely delaying handling of timer signals.
			 */
			SET_YDB_ENGINE_MUTEX_HOLDER_THREAD_ID(mutex_holder_thread_id, tLevel);
			if (!mutex_holder_thread_id)
			{
				status = pthread_mutex_trylock(&ydb_engine_threadsafe_mutex[0]);
				assert((0 == status) || (EBUSY == status));
				/* If status is non-zero, we do not have the YottaDB engine lock so we cannot call
				 * "rts_error_csa" etc. therefore just silently continue to next iteration.
				 */
				if (0 == status)
				{
					assert(0 == ydb_engine_threadsafe_mutex_holder[0]);
					ydb_engine_threadsafe_mutex_holder[0] = pthread_self();
					STAPI_INVOKE_DEFERRED_SIGNAL_HANDLER_IF_NEEDED;
					ydb_engine_threadsafe_mutex_holder[0] = 0;
					status = pthread_mutex_unlock(&ydb_engine_threadsafe_mutex[0]);
					/* If status is non-zero, we do have the YottaDB engine lock so we CAN call
					 * "rts_error_csa" etc. therefore do just that.
					 */
					if (status)
					{
						assert(FALSE);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
							RTS_ERROR_LITERAL("pthread_mutex_unlock()"), CALLFROM, status);
					}
				}
				continue;
			}
			/* YottaDB engine thread lock is held by another thread. So try forward signal to lock holding thread. */
			assert(!pthread_equal(mutex_holder_thread_id, pthread_self()));
			if (mutex_holder_thread_id == prev_wake_up_thread_id)
			{	/* Avoid waking up same thread too frequently (possible if lock holding thread is in TP
				 * in which case the wake up will result in the signal handler being invoked and returning
				 * right away because of logic in FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED.
				 */
				assert(i > prev_wake_up_i);
				if ((i - prev_wake_up_i) < prev_diff_i)
					continue;
				/* Each "i" corresponds to a microsecond (SLEEP_USEC(1) call). To avoid frequent wake ups,
				 * wake it up once, then sleep for 10 microseconds, then 100 microsecnds, then 1000 microseconds
				 * and then come back to 10, 100, 1000 microsecond sequence again. This way we sleep at most
				 * 1 millisecond between multiple wakeups of the same thread (i.e. do not sleep a lot between
				 * multiple attempts at wake up) and yet do not waste time on wake-up system calls by doing it
				 * too frequently either.
				 */
				prev_diff_i = prev_diff_i * 10;
				if (MICROSECS_IN_MSEC < prev_diff_i)	/* time between wakeups should not exceed 1 millisecond */
					prev_diff_i = 10;
			} else
				prev_diff_i = 1;	/* Waking up different thread than before. Reset "prev_diff_i". */
			prev_wake_up_i = i;
			prev_wake_up_thread_id = mutex_holder_thread_id;
			for (sig_handler_type = 0; sig_handler_type < sig_hndlr_num_entries; sig_handler_type++)
			{
				if (!STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_handler_type))
					continue;
				sig_num = stapi_signal_handler_oscontext[sig_handler_type].sig_num;
				if (sig_num)
					pthread_kill(mutex_holder_thread_id, sig_num);
			}
		} else
			prev_diff_i = 1;	/* All pending signal handler handling is done. Reset "prev_diff_i". */
	}
	return NULL;
}

/* Function that does exit handling in SimpleThreadAPI mode */
void	ydb_stm_thread_exit(void)
{
	gtm_exit_handler(); /* rundown all open database resource */
	return;
}
