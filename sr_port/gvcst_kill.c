/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h>
#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"
#include "gtm_inet.h"	/* Required for gtmsource.h */

#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "jnl.h"
#include "copy.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "interlock.h"
#ifdef GTM_TRIGGER
#include "rtnhdr.h"		/* for rtn_tabent in gv_trigger.h */
#include "gv_trigger.h"
#include "gtm_trigger.h"
#include "gv_trigger_protos.h"
#include "tp_restart.h"
#include "mv_stent.h"
#include "stringpool.h"
#	ifdef DEBUG
#	include "stack_frame.h"
#	include "tp_frame.h"
#	endif
#endif

/* Include prototypes */
#include "gvcst_kill_blk.h"
#include "t_qread.h"
#include "t_end.h"
#include "t_retry.h"
#include "t_begin.h"
#include "gvcst_expand_free_subtree.h"
#include "gvcst_protos.h"	/* for gvcst_kill,gvcst_search prototype */
#include "rc_cpt_ops.h"
#include "add_inter.h"
#include "sleep_cnt.h"
#include "wcs_sleep.h"
#include "wbox_test_init.h"
#include "memcoherency.h"
#include "util.h"
#include "op.h"			/* for op_tstart prototype */
#include "format_targ_key.h"	/* for format_targ_key prototype */
#include "tp_set_sgm.h"		/* for tp_set_sgm prototype */
#include "op_tcommit.h"		/* for op_tcommit prototype */

#ifdef GTM_TRIGGER
LITREF	mval	literal_null;
LITREF	mval	*fndata_table[2][2];
#endif

GBLREF	char			*update_array, *update_array_ptr;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey, *gv_altkey;
GBLREF	int4			gv_keysize;
GBLREF	gv_namehead		*gv_target;
GBLREF	uint4			update_array_size, cumul_update_array_size; /* needed for the ENSURE_UPDATE_ARRAY_SPACE macro */
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	kill_set		*kill_set_tail;
GBLREF	short			dollar_tlevel;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	unsigned char		cw_set_depth;
GBLREF	unsigned int		t_tries;
GBLREF	boolean_t		need_kip_incr;
GBLREF	uint4			update_trans;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	sgmnt_addrs		*kip_csa;
#ifdef GTM_TRIGGER
GBLREF	int4			gtm_trigger_depth;
GBLREF	int4			tstart_trigger_depth;
GBLREF	boolean_t		skip_INVOKE_RESTART;
GBLREF	boolean_t		skip_dbtriggers;	/* see gbldefs.c for description of this global */
GBLREF	boolean_t		implicit_tstart;	/* see gbldefs.c for comment */
GBLREF	boolean_t		ztwormhole_used;	/* TRUE if $ztwormhole was used by trigger code */
GBLREF	mval			dollar_ztwormhole;
#endif
#ifdef DEBUG
GBLREF	uint4			donot_commit;	/* see gdsfhead.h for the purpose of this debug-only global */
#endif

void	gvcst_kill(bool do_subtree)
{
	bool			clue, flush_cache;
	boolean_t		next_fenced_was_null, write_logical_jnlrecs, jnl_format_done;
	boolean_t		left_extra, right_extra;
	cw_set_element		*tp_cse;
	enum cdb_sc		cdb_status;
	int			lev, end;
	uint4			prev_update_trans, actual_update;
	jnl_format_buffer	*jfb, *ztworm_jfb;
	jnl_action_code		operation;
	kill_set		kill_set_head, *ks, *temp_ks;
	node_local_ptr_t	cnl;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	srch_blk_status		*left,*right;
	srch_hist		*alt_hist;
	srch_rec_status		*left_rec_stat, local_srch_rec;
	uint4			segment_update_array_size;
	unsigned char		*base;
	int			lcl_dollar_tlevel;
	uint4			nodeflags;
	sgm_info		*si;
#	ifdef GTM_TRIGGER
	mint			dlr_data;
	boolean_t		is_tpwrap;
	boolean_t		lcl_implicit_tstart;	/* local copy of the global variable "implicit_tstart" */
	gtm_trigger_parms	trigparms;
	gvt_trigger_t		*gvt_trigger;
	gvtr_invoke_parms_t	gvtr_parms;
	int			gtm_trig_status;
	unsigned char		*save_msp;
	mv_stent		*save_mv_chain;
	mval			*ztold_mval = NULL, ztvalue_new, ztworm_val;
#	endif
#	ifdef DEBUG
	boolean_t		is_mm;
	uint4			dbg_research_cnt;
#	endif

	error_def(ERR_TPRETRY);

	csa = cs_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	DEBUG_ONLY(is_mm = (dba_mm == csd->acc_meth);)
	GTMTRIG_ONLY(
		TRIG_CHECK_REPLSTATE_MATCHES_EXPLICIT_UPDATE(gv_cur_region, csa);
		assert(!dollar_tlevel || (tstart_trigger_depth <= gtm_trigger_depth));
		if (!dollar_tlevel || (gtm_trigger_depth == tstart_trigger_depth))
		{	/* This is an explicit update. Set ztwormhole_used to FALSE. Note that we initialize this only at the
			 * beginning of the transaction and not at the beginning of each try/retry. If the application used
			 * $ztwormhole in any retsarting try of the transaction, we consider it necessary to write the
			 * TZTWORM/UZTWORM record even though it was not used in the succeeding/committing try.
			 */
			ztwormhole_used = FALSE;
		}
	)
	JNLPOOL_INIT_IF_NEEDED(csa, csd, cnl);
	if (0 == dollar_tlevel)
	{
		kill_set_head.next_kill_set = NULL;
		if (jnl_fence_ctl.level)	/* next_fenced_was_null is reliable only if we are in ZTransaction */
			next_fenced_was_null = (NULL == csa->next_fenced) ? TRUE : FALSE;
		/* In case of non-TP explicit updates that invoke triggers the kills happen inside of TP. If those kills
		 * dont cause any actual update, we need prev_update_trans set appropriately so update_trans can be reset.
		 */
		GTMTRIG_ONLY(prev_update_trans = 0;)
	} else
		prev_update_trans = sgm_info_ptr->update_trans;
	assert(('\0' != gv_currkey->base[0]) && gv_currkey->end);
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC;
	T_BEGIN_SETORKILL_NONTP_OR_TP(ERR_GVKILLFAIL);
	assert(NULL != update_array);
	assert(NULL != update_array_ptr);
	assert(0 != update_array_size);
	assert(update_array + update_array_size >= update_array_ptr);
	alt_hist = gv_target->alt_hist;
	GTMTRIG_ONLY(
		lcl_implicit_tstart = FALSE;
		trigparms.ztvalue_new = NULL;
	)
	operation = (do_subtree ? JNL_KILL : JNL_ZKILL);
	for (;;)
	{
		actual_update = 0;
		DEBUG_ONLY(dbg_research_cnt = 0;)
		jnl_format_done = FALSE;
		write_logical_jnlrecs = JNL_WRITE_LOGICAL_RECS(csa);
#		ifdef GTM_TRIGGER
		gvtr_parms.num_triggers_invoked = 0;	/* clear any leftover value */
		GVTR_INIT_AND_TPWRAP_IF_NEEDED(csa, csd, gv_target, gvt_trigger, lcl_implicit_tstart, is_tpwrap, ERR_GVKILLFAIL);
		assert(skip_dbtriggers || (gvt_trigger == gv_target->gvt_trigger));
		dlr_data = 0;
		if (!skip_dbtriggers && (NULL != gvt_trigger))
		{
			PUSH_ZTOLDMVAL_ON_M_STACK(ztold_mval, save_msp, save_mv_chain);
			/* Determine $ZTOLDVAL & $ZTDATA to fill in trigparms */
			cdb_status = gvcst_dataget(&dlr_data, ztold_mval);
                        if (cdb_sc_normal != cdb_status)
                                goto retry;
			assert((11 >= dlr_data) && (1 >= (dlr_data % 10)));
			/* Invoke triggers for KILL as long as $data is nonzero (1 or 10 or 11).
			 * Invoke triggers for ZKILL only if $data is 1 or 11 (for 10 case, ZKILL is a no-op).
			 */
			if (do_subtree ? dlr_data : (dlr_data & 1))
			{	/* Either node or its descendants exists. Invoke KILL triggers for this node.
				 * But first write journal records (ZTWORM and/or KILL) for the triggering nupdate.
				 * "ztworm_jfb", "jfb" and "jnl_format_done" are set by the below macro.
				 */
				JNL_FORMAT_ZTWORM_IF_NEEDED(csa, write_logical_jnlrecs,
						operation, gv_currkey, NULL, ztworm_jfb, jfb, jnl_format_done);
				/* Initialize trigger parms that dont depend on the context of the matching trigger */
				trigparms.ztoldval_new = ztold_mval;
				trigparms.ztdata_new = fndata_table[dlr_data / 10][dlr_data & 1];
				if (NULL == trigparms.ztvalue_new)
				{	/* Do not pass literal_null directly since $ztval can be modified inside trigger code
					 * and literal_null is in read-only segment so will not be modifiable. Hence the
					 * need for a dummy local variable mval "ztvalue_new" in the C stack.
					 */
					ztvalue_new = literal_null;
					trigparms.ztvalue_new = &ztvalue_new;
				}
				gvtr_parms.gvtr_cmd = do_subtree ? GVTR_CMDTYPE_KILL : GVTR_CMDTYPE_ZKILL;
				gvtr_parms.gvt_trigger = gvt_trigger;
				/* gvtr_parms.duplicate_set : No need to initialize as it is SET-specific */
				/* Now that we have filled in minimal information, let "gvtr_match_n_invoke" do the rest */
				gtm_trig_status = gvtr_match_n_invoke(&trigparms, &gvtr_parms);
				assert((0 == gtm_trig_status) || (ERR_TPRETRY == gtm_trig_status));
				if (ERR_TPRETRY == gtm_trig_status)
				{	/* A restart has been signalled inside trigger code for this implicit TP wrapped
					 * transaction. Redo gvcst_kill logic. The t_retry/tp_restart call has already
					 * been done on our behalf by gtm_trigger so we need to skip that part and do
					 * everything else before redoing gvcst_kill.
					 */
					assert(lcl_implicit_tstart);
					assert(0 < t_tries);
					assert(CDB_STAGNATE >= t_tries);
					cdb_status = cdb_sc_normal;	/* signal "retry:" to avoid t_retry call */
					goto retry;
				}
				REMOVE_ZTWORM_JFB_IF_NEEDED(ztworm_jfb, jfb, sgm_info_ptr);
			}
			/* else : we dont invoke any KILL/ZTKILL type triggers for a node whose $data is 0 */
			POP_MVALS_FROM_M_STACK_IF_NEEDED(ztold_mval, save_msp, save_mv_chain);
		}
#		endif
		assert(csd == cs_data);	/* assert csd is in sync with cs_data even if there were MM db file extensions */
		assert(csd == csa->hdr);
		si = sgm_info_ptr;	/* Has to be AFTER the GVTR_INIT_AND_TPWRAP_IF_NEEDED macro in case that sets
					 * sgm_info_ptr to a non-NULL value (if a non-TP transaction is tp wrapped for triggers).
					 */
		assert(t_tries < CDB_STAGNATE || csa->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
		if (0 == dollar_tlevel)
		{
			CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
			kill_set_tail = &kill_set_head;
			for (ks = &kill_set_head;  NULL != ks;  ks = ks->next_kill_set)
				ks->used = 0;
		} else
		{
			segment_update_array_size = UA_NON_BM_SIZE(csd);
			ENSURE_UPDATE_ARRAY_SPACE(segment_update_array_size);
		}
		clue = (0 != gv_target->clue.end);
research:
		if (cdb_sc_normal != (cdb_status = gvcst_search(gv_currkey, NULL)))
			goto retry;
		assert(gv_altkey->top == gv_currkey->top);
		assert(gv_altkey->top == gv_keysize);
		end = gv_currkey->end;
		assert(end < gv_currkey->top);
		memcpy(gv_altkey, gv_currkey, SIZEOF(gv_key) + end);
		base = &gv_altkey->base[0];
		if (do_subtree)
		{
			base[end - 1] = 1;
			assert(KEY_DELIMITER == base[end]);
			base[++end] = KEY_DELIMITER;
		} else
		{
			base[end] = 1;
			base[++end] = KEY_DELIMITER;
			base[++end] = KEY_DELIMITER;
		}
		gv_altkey->end = end;
		if (cdb_sc_normal != (cdb_status = gvcst_search(gv_altkey, alt_hist)))
			goto retry;
		if (alt_hist->depth != gv_target->hist.depth)
		{
			cdb_status = cdb_sc_badlvl;
			goto retry;
		}
		right_extra = FALSE;
		left_extra = TRUE;
		for (lev = 0; 0 != gv_target->hist.h[lev].blk_num; ++lev)
		{
			left = &gv_target->hist.h[lev];
			right = &alt_hist->h[lev];
			assert(0 != right->blk_num);
			left_rec_stat = left_extra ? &left->prev_rec : &left->curr_rec;
			if (left->blk_num == right->blk_num)
			{
				cdb_status = gvcst_kill_blk(left, lev, gv_currkey, *left_rec_stat, right->curr_rec,
								right_extra, &tp_cse);
				assert(!dollar_tlevel || (NULL == tp_cse) || (left->cse == tp_cse));
				assert( dollar_tlevel || (NULL == tp_cse));
				if (tp_cse)
					actual_update = UPDTRNS_DB_UPDATED_MASK;
				if (cdb_sc_normal == cdb_status)
					break;
				gv_target->clue.end = 0;/* If need to go up from leaf (or higher), history will cease to be valid */
				if (clue)
				{	/* Clue history valid only for data block, need to re-search */
					clue = FALSE;
					DEBUG_ONLY(dbg_research_cnt++;)
					goto research;
				}
				if (cdb_sc_delete_parent != cdb_status)
					goto retry;
				left_extra = right_extra
					   = TRUE;
			} else
			{
				gv_target->clue.end = 0; /* If more than one block involved, history will cease to be valid */
				if (clue)
				{	/* Clue history valid only for data block, need to re-search */
					clue = FALSE;
					DEBUG_ONLY(dbg_research_cnt++;)
					goto research;
				}
				local_srch_rec.offset = ((blk_hdr_ptr_t)left->buffaddr)->bsiz;
				local_srch_rec.match = 0;
				cdb_status = gvcst_kill_blk(left, lev, gv_currkey, *left_rec_stat, local_srch_rec, FALSE, &tp_cse);
				assert(!dollar_tlevel || (NULL == tp_cse) || (left->cse == tp_cse));
				assert( dollar_tlevel || (NULL == tp_cse));
				if (tp_cse)
					actual_update = UPDTRNS_DB_UPDATED_MASK;
				if (cdb_sc_normal == cdb_status)
					left_extra = FALSE;
				else if (cdb_sc_delete_parent == cdb_status)
				{
					left_extra = TRUE;
					cdb_status = cdb_sc_normal;
				} else
					goto retry;
				local_srch_rec.offset = local_srch_rec.match
						      = 0;
				cdb_status = gvcst_kill_blk(right, lev, gv_altkey, local_srch_rec, right->curr_rec,
								right_extra, &tp_cse);
				assert(!dollar_tlevel || (NULL == tp_cse) || (right->cse == tp_cse));
				assert( dollar_tlevel || (NULL == tp_cse));
				if (tp_cse)
					actual_update = UPDTRNS_DB_UPDATED_MASK;
				if (cdb_sc_normal == cdb_status)
					right_extra = FALSE;
				else if (cdb_sc_delete_parent == cdb_status)
				{
					right_extra = TRUE;
					cdb_status = cdb_sc_normal;
				} else
					goto retry;
			}
		}
		if (!dollar_tlevel)
		{
			assert(!jnl_format_done);
			assert(0 == actual_update); /* for non-TP, tp_cse is NULL even if cw_set_depth is non-zero */
			if (0 != cw_set_depth)
				actual_update = UPDTRNS_DB_UPDATED_MASK;
			/* reset update_trans (to potentially non-zero value) in case it got set to 0 in previous retry */
			/* do not treat redundant KILL as an update-transaction */
			update_trans = actual_update;
		} else
		{
			GTMTRIG_ONLY(
				if (!actual_update) /* possible only if the node we are attempting to KILL does not exist now */
				{	/* Note that it is possible that the node existed at the time of the "gvcst_dataget" but
					 * got killed later when we did the gvcst_search (right after the "research:" label). This
					 * is possible if any triggers invoked in between KILLed the node and/or all its
					 * descendants. We still want to consider this case as an actual update to the database
					 * as far as journaling is concerned (this is because we have already formatted the
					 * KILL journal record) so set actual_update to UPDTRNS_DB_UPDATED_MASK in this case.
					 * Note that it is possible that the node does not exist now due to a restartable
					 * situation (instead of due to a KILL inside trigger code). In that case, it is safe to
					 * set actual_update to UPDTRNS_DB_UPDATED_MASK (even though we did not do any update to
					 * the database) since we will be restarting anyways. For ZKILL, check if dlr_data was
					 * 1 or 11 and for KILL, check if it was 1, 10 or 11.
					 */
					DEBUG_ONLY(
						if (!gvtr_parms.num_triggers_invoked && (do_subtree ? dlr_data : (dlr_data & 1)))
						{	/* Triggers were not invoked but still the node that existed a few
							 * steps above does not exist now. This is a restartable situation.
							 * Assert that.
							 */
							assert(!skip_dbtriggers);
							donot_commit |= DONOTCOMMIT_GVCST_KILL_ZERO_TRIGGERS;
						}
					)
					if (do_subtree ? dlr_data : (dlr_data & 1))
						actual_update = UPDTRNS_DB_UPDATED_MASK;
				}
			)
			NON_GTMTRIG_ONLY(assert(!jnl_format_done);)
			assert(!actual_update || si->cw_set_depth
							GTMTRIG_ONLY(|| gvtr_parms.num_triggers_invoked || donot_commit));
			assert(!(prev_update_trans & ~UPDTRNS_VALID_MASK));
			if (!actual_update)
				si->update_trans = prev_update_trans;	/* restore status prior to redundant KILL */
		}
		if (write_logical_jnlrecs && actual_update)
		{	/* Maintain journal records only if the kill actually resulted in a database update. */
			if (0 == dollar_tlevel)
			{
				assert(!jnl_format_done);
				jfb = jnl_format(operation, gv_currkey, NULL, 0);
				assert(NULL != jfb);
			} else if (!jnl_format_done)
			{
				nodeflags = 0;
				GTMTRIG_ONLY(
					if (skip_dbtriggers)
						nodeflags = JS_SKIP_TRIGGERS_MASK;
					/* Do not replicate implicit updates */
					assert(tstart_trigger_depth <= gtm_trigger_depth);
					if (gtm_trigger_depth > tstart_trigger_depth)
						nodeflags = JS_NOT_REPLICATED_MASK;
				)
				/* Write KILL journal record */
				jfb = jnl_format(operation, gv_currkey, NULL, nodeflags);
				assert(NULL != jfb);
			}
		}
		flush_cache = FALSE;
		if (0 == dollar_tlevel)
		{
			if ((0 != csd->dsid) && (0 < kill_set_head.used)
				&& gv_target->hist.h[1].blk_num != alt_hist->h[1].blk_num)
			{	/* multi-level delete */
				rc_cpt_inval();
				flush_cache = TRUE;
			}
			if (0 < kill_set_head.used)		/* increase kill_in_prog */
			{
				need_kip_incr = TRUE;
				if (!csa->now_crit)	/* Do not sleep while holding crit */
					WAIT_ON_INHIBIT_KILLS(cnl, MAXWAIT2KILL);
			}
			if ((trans_num)0 == t_end(&gv_target->hist, alt_hist))
			{	/* In case this is MM and t_end caused a database extension, reset csd */
				assert(is_mm || (csd == cs_data));
				csd = cs_data;
				if (jnl_fence_ctl.level && next_fenced_was_null && actual_update && write_logical_jnlrecs)
				{	/* If ZTransaction and first KILL and the kill resulted in an update
					 * Note that "write_logical_jnlrecs" is used above instead of JNL_WRITE_LOGICAL_RECS(csa)
					 * since the value of the latter macro might have changed inside the call to t_end()
					 * (since jnl state changes could change the JNL_ENABLED check which is part of the macro).
					 */
					assert(NULL != csa->next_fenced);
					assert(jnl_fence_ctl.fence_list == csa);
					jnl_fence_ctl.fence_list = csa->next_fenced;
					csa->next_fenced = NULL;
				}
				need_kip_incr = FALSE;
				assert(NULL == kip_csa);
				continue;
			}
			/* In case this is MM and t_end caused a database extension, reset csd */
			assert(is_mm || (csd == cs_data));
			csd = cs_data;
		} else
                {
                        cdb_status = tp_hist(alt_hist);
                        if (cdb_sc_normal != cdb_status)
                                goto retry;
                }
		/* Note down $tlevel (used later) before it is potentially changed by op_tcommit below */
		lcl_dollar_tlevel = dollar_tlevel;
#		ifdef GTM_TRIGGER
		if (lcl_implicit_tstart)
		{
			GVTR_OP_TCOMMIT(cdb_status);
			if (cdb_sc_normal != cdb_status)
                                goto retry;
		}
#		endif
		INCR_GVSTATS_COUNTER(csa, cnl, n_kill, 1);
		if (0 != gv_target->clue.end)
		{	/* If clue is still valid, then the deletion was confined to a single block */
			assert(gv_target->hist.h[0].blk_num == alt_hist->h[0].blk_num);
			/* In this case, the "right hand" key (which was searched via gv_altkey) was the last search
			 * and should become the clue.  Furthermore, the curr.match from this last search should be
			 * the history's curr.match.  However, this record will have been shuffled to the position of
			 * the "left hand" key, and therefore, the original curr.offset should be left untouched. */
			gv_target->hist.h[0].curr_rec.match = alt_hist->h[0].curr_rec.match;
			COPY_CURRKEY_TO_GVTARGET_CLUE(gv_target, gv_altkey);
		}
		NON_GTMTRIG_ONLY(assert(lcl_dollar_tlevel == dollar_tlevel);)
		if (0 == lcl_dollar_tlevel)
		{
			assert(0 == dollar_tlevel);
			assert(0 < kill_set_head.used || (NULL == kip_csa));
			if (0 < kill_set_head.used)     /* free subtree, decrease kill_in_prog */
			{	/* If csd->dsid is non-zero then some rc code was exercised before the changes
				 * to prevent pre-commit expansion of the kill subtree. Not clear on what to do now.
				 */
				assert(!csd->dsid);
				ENABLE_WBTEST_ABANDONEDKILL;
				gvcst_expand_free_subtree(&kill_set_head);
				/* In case this is MM and gvcst_expand_free_subtree() called gvcst_bmp_mark_free() called t_retry()
				 * which remapped an extended database, reset csd */
				assert(is_mm || (csd == cs_data));
				csd = cs_data;
				DECR_KIP(csd, csa, kip_csa);
			}
			assert(0 < kill_set_head.used || (NULL == kip_csa));
			for (ks = kill_set_head.next_kill_set;  NULL != ks;  ks = temp_ks)
			{
				temp_ks = ks->next_kill_set;
				free(ks);
			}
			assert(0 < kill_set_head.used || (NULL == kip_csa));
		}
		GTMTRIG_ONLY(assert(NULL == ztold_mval);)
		return;
retry:
		GTMTRIG_ONLY(POP_MVALS_FROM_M_STACK_IF_NEEDED(ztold_mval, save_msp, save_mv_chain);)
#		ifdef GTM_TRIGGER
		if (lcl_implicit_tstart)
		{
			assert(!skip_dbtriggers);
			assert(!skip_INVOKE_RESTART);
			assert((cdb_sc_normal != cdb_status) || (ERR_TPRETRY == gtm_trig_status));
			if (cdb_sc_normal != cdb_status)
				skip_INVOKE_RESTART = TRUE; /* causes t_retry to invoke only tp_restart * without any rts_error */
			/* else: t_retry has already been done by gtm_trigger so no need to do it again for this try */
		}
#		endif
		assert((cdb_sc_normal != cdb_status) GTMTRIG_ONLY(|| lcl_implicit_tstart));
		if (cdb_sc_normal != cdb_status)
			t_retry(cdb_status);
		else
		{	/* else: t_retry has already been done so no need to do that again but need to still invoke tp_restart
			 * to complete pending "tprestart_state" related work.
			 */
			GTMTRIG_ONLY(assert(ERR_TPRETRY == gtm_trig_status);)
			tp_restart(1);
		}
		GTMTRIG_ONLY(assert(!skip_INVOKE_RESTART);) /* if set to TRUE above, should have been reset by t_retry */
		/* At this point, we can be in TP only if we implicitly did a tstart in gvcst_kill (as part of a trigger update).
		 * Assert that. Since the t_retry/tp_restart would have reset si->update_trans, we need to set it again.
		 * So reinvoke the T_BEGIN call only in case of TP. For non-TP, update_trans is unaffected by t_retry.
		 */
		assert((0 == dollar_tlevel) GTMTRIG_ONLY(|| lcl_implicit_tstart));
		if (dollar_tlevel)
		{
			tp_set_sgm();	/* set sgm_info_ptr & first_sgm_info for TP start */
			T_BEGIN_SETORKILL_NONTP_OR_TP(ERR_GVKILLFAIL);	/* set update_trans and t_err for wrapped TP */
		}
		/* In case this is MM and t_retry() remapped an extended database, reset csd */
		assert(is_mm || (csd == cs_data));
		csd = cs_data;
	}
}
