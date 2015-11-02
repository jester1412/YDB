/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "error.h"
#include "filestruct.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "jnl.h"		/* needed for WCSFLU_* macros */
#include "sleep_cnt.h"
#include "util.h"
#include "gvcst_protos.h"	/* for gvcst_init prototype */
#include "change_reg.h"
#include "gtmmsg.h"
#include "mu_int_wait_rdonly.h"
#include "wcs_sleep.h"
#include "wcs_flu.h"
#include "interlock.h"
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
#ifdef GTM_SNAPSHOT
#include "db_snapshot.h"
#endif
#include "mupint.h"
#include "wbox_test_init.h"

#define MUPIP_INTEG "MUPIP INTEG"

GBLREF gd_region		*gv_cur_region;
GBLREF sgmnt_data		mu_int_data;
GBLREF unsigned char		*mu_int_master;
GBLREF uint4			mu_int_errknt;
GBLREF sgmnt_data_ptr_t		cs_data;
GBLREF pid_t			process_id;

GTMCRYPT_ONLY(
	GBLREF gtmcrypt_key_t	mu_int_encrypt_key_handle;
)
GBLREF boolean_t		ointeg_this_reg;
GTM_SNAPSHOT_ONLY(
	GBLREF util_snapshot_ptr_t	util_ss_ptr;
	GBLREF boolean_t		preserve_snapshot;
	GBLREF boolean_t		online_specified;
)

void mu_int_reg(gd_region *reg, boolean_t *return_value)
{
	sgmnt_addrs     	*csa;
	freeze_status		status;
	GTMCRYPT_ONLY(
		int		crypt_status;
	)
	node_local_ptr_t	cnl;
	boolean_t		need_to_wait = FALSE, read_only;
	int			trynum;
	uint4			curr_wbox_seq_num;

	error_def(ERR_BUFFLUFAILED);
	error_def(ERR_DBRDONLY);
	error_def(ERR_MUKILLIP);
	error_def(ERR_SSV4NOALLOW);
	error_def(ERR_SSMMNOALLOW);
	*return_value = FALSE;

	ESTABLISH(mu_int_reg_ch);
	if (dba_usr == reg->dyn.addr->acc_meth)
	{
		util_out_print("!/Can't integ region !AD; not GDS format", TRUE,  REG_LEN_STR(reg));
		mu_int_errknt++;
		return;
	}
	gv_cur_region = reg;
	if (reg_cmcheck(reg))
	{
		util_out_print("!/Can't integ region across network", TRUE);
		mu_int_errknt++;
		return;
	}
	gvcst_init(gv_cur_region);
	if (gv_cur_region->was_open)
	{	/* already open under another name */
		gv_cur_region->open = FALSE;
		return;
	}
	change_reg();
	csa = &FILE_INFO(gv_cur_region)->s_addrs;
	cnl = csa->nl;
	read_only = gv_cur_region->read_only;
#	ifdef GTM_CRYPT
	/* Initialize mu_int_encrypt_key_handle to be used in mu_int_read */
	if (cs_data->is_encrypted)
	{
		/* Encryption init should have happened in db_init. */
		ASSERT_ENCRYPTION_INITIALIZED;
		/* If the encryption init failed in db_init, the below MACRO should return an error.
		 * Depending on the error returned, report the error.*/
		GTMCRYPT_GETKEY(cs_data->encryption_hash, mu_int_encrypt_key_handle, crypt_status);
		if (0 != crypt_status)
		{
			GC_GTM_PUTMSG(crypt_status, (gv_cur_region->dyn.addr->fname));
			return;
		}
	}
#	endif
	if (dba_mm == cs_data->acc_meth && read_only)
	{
		util_out_print("!/MM database is read only. MM database cannot be frozen without write access.", TRUE);
		gtm_putmsg(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
		mu_int_errknt++;
		return;
	}
	assert(NULL != mu_int_master);
	/* Ensure that we don't see an increase in the file header and master map size compared to it's maximum values */
	assert(SGMNT_HDR_LEN >= SIZEOF(sgmnt_data) && (MASTER_MAP_SIZE_MAX >= MASTER_MAP_SIZE(cs_data)));
	/* ONLINE INTEG if asked for explicitly by specifying -ONLINE is an error if the db has partial V4 blocks.
	 * However, if -ONLINE is not explicitly specified but rather assumed implicitly (as default for -REG)
	 * then turn off ONLINE INTEG for this region and continue as if -NOONLINE was specified
	 */
#	ifdef GTM_SNAPSHOT
	if (!cs_data->fully_upgraded)
	{
		ointeg_this_reg = FALSE; /* Turn off ONLINE INTEG for this region */
		if (online_specified)
		{
			gtm_putmsg(VARLSTCNT(4) ERR_SSV4NOALLOW, 2, DB_LEN_STR(gv_cur_region));
			util_out_print(NO_ONLINE_ERR_MSG, TRUE);
			mu_int_errknt++;
			return;
		}
	}
#	endif
	if (!ointeg_this_reg || read_only)
	{
		status = region_freeze(gv_cur_region, TRUE, FALSE, TRUE);
		switch (status)
		{
			case REG_ALREADY_FROZEN:
				util_out_print("!/Database for region !AD is already frozen, not integing",
					TRUE, REG_LEN_STR(gv_cur_region));
				mu_int_errknt++;
				return;
			case REG_HAS_KIP:
				/* We have already waited for KIP to reset. This time do not wait for KIP */
				status = region_freeze(gv_cur_region, TRUE, FALSE, FALSE);
				if (REG_ALREADY_FROZEN == status)
				{
					util_out_print("!/Database for region !AD is already frozen, not integing",
						TRUE, REG_LEN_STR(gv_cur_region));
					mu_int_errknt++;
					return;
				}
				break;
			case REG_FREEZE_SUCCESS:
				break;
			default:
				assert(FALSE);
		}
		if (read_only && !mu_int_wait_rdonly(csa, MUPIP_INTEG))
		{
			mu_int_errknt++;
			return;
		}
	}
	if (!ointeg_this_reg)
	{
		if (!read_only && !wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH))
		{
			gtm_putmsg(VARLSTCNT(6) ERR_BUFFLUFAILED, 4, LEN_AND_LIT(MUPIP_INTEG), DB_LEN_STR(gv_cur_region));
			mu_int_errknt++;
			return;
		}
		/* Take a copy of the file-header. To ensure it is consistent, do it while holding crit. */
		grab_crit(gv_cur_region);
		memcpy((uchar_ptr_t)&mu_int_data, (uchar_ptr_t)cs_data, SIZEOF(sgmnt_data));
		rel_crit(gv_cur_region);
		memcpy(mu_int_master, MM_ADDR(cs_data), MASTER_MAP_SIZE(cs_data));
	} else
	{
#		ifdef GTM_SNAPSHOT
		if (!ss_initiate(gv_cur_region, util_ss_ptr, &csa->ss_ctx, preserve_snapshot, MUPIP_INTEG))
		{
			mu_int_errknt++;
			assert(NULL != csa->ss_ctx);
			ss_release(&csa->ss_ctx);
			ointeg_this_reg = FALSE; /* Turn off ONLINE INTEG for this region */
			assert(process_id != cnl->in_crit); /* Ensure ss_initiate released the crit before returning */
			assert(process_id != cs_data->freeze); /* Ensure region is unfrozen before returning from ss_initiate */
			return;
		}
		assert(process_id != cnl->in_crit); /* Ensure ss_initiate released the crit before returning */
		assert(process_id != cs_data->freeze); /* Ensure region is unfrozen before returning from ss_initiate */
#		if defined(DEBUG)
		curr_wbox_seq_num = 1;
		cnl->wbox_test_seq_num = curr_wbox_seq_num; /* indicate we took the next step */
		GTM_WHITE_BOX_TEST(WBTEST_OINTEG_WAIT_ON_START, need_to_wait, TRUE);
		if (need_to_wait) /* wait for them to take next step */
		{
			trynum = 30; /* given 30 cycles to tell you to go */
			while ((curr_wbox_seq_num == cnl->wbox_test_seq_num) && trynum--)
				sleep(1);
			cnl->wbox_test_seq_num++; /* let them know we took the next step */
			assert(trynum);
		}
#		endif
#		endif
	}
	*return_value = mu_int_fhead();
	REVERT;
	return;
}
