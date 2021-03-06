/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sched.h>
#include "gtm_unistd.h"

#include "rel_quant.h"
#include "gtm_rel_quant.h"
#include "have_crit.h"

/* relinquish the processor to the next process in the scheduling queue */
void rel_quant(void)
{
	DEFERRED_SIGNAL_HANDLING_CHECK;	/* Check for any deferred signals (timers etc.) that need to be handled */
	RELQUANT;
}
