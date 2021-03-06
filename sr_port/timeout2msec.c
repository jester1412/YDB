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

	/*	timeout2msec - convert timeout seconds to milliseconds; check for range of value.
 *
 *	According to the M(UMPS) standard, a negative timeout value should be equivalent to a zero timeout value.
 *
 *	input:
 *
 *		timeout -	timeout value in seconds
 *
 *
 *	return value:
 *
 *		if timeout
 *			<= 0		zero
 *			> MAXPOSINT4	MAXPOSINT4
 *			else		timeout * 1000 (timeout value in milliseconds)
 */

int4	timeout2msec (uint8 timeout)
{

	/* if multiplying by 1000 causes arithmetic overflow, cap the value */
	return (((uint8)MAXPOSINT4 > (timeout * MILLISECS_IN_SEC)) ? (int4)(timeout * MILLISECS_IN_SEC) : MAXPOSINT4);
}
