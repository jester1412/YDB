/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TRANS_LOG_NAME_H_INCLUDED
#define TRANS_LOG_NAME_H_INCLUDED

typedef enum
{
	dont_sendmsg_on_log2long = 0,
	do_sendmsg_on_log2long
} translog_act;

int4 trans_log_name(mstr *log, mstr *trans, char *buffer, int4 buffer_len, translog_act do_sendmsg);

#endif
