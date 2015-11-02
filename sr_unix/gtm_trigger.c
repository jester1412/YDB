/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_limits.h"
#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include <errno.h>

#include "compiler.h"
#include "error.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "hashtab_mname.h"
#include "lv_val.h"
#include "mv_stent.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#include "gv_trigger.h"
#include "gtm_trigger.h"
#include "op.h"
#include "gtmio.h"
#include "stringpool.h"
#include "alias.h"
#include "urx.h"
#include "zbreak.h"
#include "gtm_text_alloc.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "tp_frame.h"
#include "auto_zlink.h"
#include "golevel.h"
#include "flush_jmp.h"
#include "dollar_zlevel.h"
#include "gtmimagename.h"

#ifdef GTM_TRIGGER

#define PREFIX_SPACE		" "
#define ERROR_CAUSING_JUNK	"XX XX XX XX"
#define NEWLINE			"\n"
#define OBJECT_PARM		" -OBJECT="
#define OBJECT_FTYPE		DOTOBJ
#define NAMEOFRTN_PARM		" -NAMEOFRTN="
#define S_CUTOFF 		7
#define GTM_TRIGGER_SOURCE_NAME	"GTM Trigger"

GBLREF	boolean_t		run_time;
GBLREF	mv_stent		*mv_chain;
GBLREF	stack_frame		*frame_pointer;
GBLREF	uint4			trigger_name_cntr;
GBLREF	int			dollar_truth;
GBLREF	mstr			extnam_str;
GBLREF	mval			dollar_zsource;
GBLREF	unsigned char		*stackbase, *stacktop, *msp, *stackwarn;
GBLREF	symval			*curr_symval;
GBLREF	int4			gtm_trigger_depth;
GBLREF	int4			tstart_trigger_depth;
GBLREF	mval			dollar_etrap;
GBLREF	mval			dollar_ztrap;
GBLREF	mval			gtm_trigger_etrap;
GBLREF	mval			*dollar_ztcode;
GBLREF	mval			*dollar_ztdata;
GBLREF	mval			*dollar_ztoldval;
GBLREF	mval			*dollar_ztriggerop;
GBLREF	mval			*dollar_ztupdate;
GBLREF	mval			*dollar_ztvalue;
GBLREF	boolean_t		*ztvalue_changed_ptr;
GBLREF	int4			dollar_zcstatus;
GBLREF	rtn_tabent		*rtn_names, *rtn_names_end;
GBLREF	boolean_t		ztrap_explicit_null;
GBLREF	int			mumps_status;
GBLREF	tp_frame		*tp_pointer;
GBLREF	boolean_t		skip_dbtriggers;		/* see gbldefs.c for description of this global */
GBLREF	boolean_t		trigger_compile;		/* A trigger compilation is active */
GBLREF  short   		dollar_tlevel;
GBLREF	symval			*trigr_symval_list;
#ifdef DEBUG
GBLREF	ch_ret_type		(*ch_at_trigger_init)();
GBLREF	boolean_t		donot_INVOKE_MUMTSTART;
/* For debugging purposes - since a stack unroll does not let us see past the current GTM invocation, knowing
 * what these parms are can be the determining factor in debugging an issue -- knowing what gtm_trigger() is
 * attempting. For that reason, these values are also saved/restored.
 */
GBLREF	gv_trigger_t		*gtm_trigdsc_last;
GBLREF	gtm_trigger_parms	*gtm_trigprm_last;
#endif

LITREF	char			alphanumeric_table[];
LITREF	int			alphanumeric_table_len;
LITREF	mval			literal_null;

STATICDEF int4			gtm_trigger_comp_prev_run_time;

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMCHECK);
error_def(ERR_LABELUNKNOWN);
error_def(ERR_MAXTRIGNEST);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_REPEATERROR);
error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);
error_def(ERR_SYSCALL);
error_def(ERR_TPRETRY);
error_def(ERR_TRIGCOMPFAIL);
error_def(ERR_TRIGNAMEUNIQ);
error_def(ERR_TRIGTLVLCHNG);

/* Macro to re-initialize a symval block that was on a previously-used free chain */
#define REINIT_SYMVAL_BLK(svb, prev)									\
{													\
	symval	*ptr;											\
	ptr = svb;											\
	assert(NULL == ptr->xnew_var_list);								\
	assert(NULL == ptr->xnew_ref_list);								\
	reinitialize_hashtab_mname(&ptr->h_symtab);							\
	ptr->lv_flist = NULL;										\
	ptr->tp_save_all = 0;										\
	ptr->alias_activity = FALSE;									\
	ptr->last_tab = (prev);										\
	ptr->symvlvl = prev->symvlvl + 1;								\
	/* Equivalent of dqinit(ptr, sbs_que) except need coercion here */				\
	ptr->sbs_que.bl = (struct sbs_blk_struct *)ptr;							\
	ptr->sbs_que.fl = ptr->sbs_que.bl;								\
	/* The lv_blk chain can remain as is but need to reinit each block so no elements are "used" */	\
	for (lvbp = &ptr->first_block; lvbp; lvbp = lvbp->next)						\
	{	/* Likely only one of these blocks (some few lvvals) but loop in case.. */		\
		clrlen = INTCAST(lvbp->lv_free - lvbp->lv_base);					\
		if (0 != clrlen)									\
		{											\
			memset(lvbp->lv_base, '\0', clrlen);						\
			lvbp->lv_free = lvbp->lv_base;							\
		}											\
	}												\
}

#if defined(__hpux) && defined(__hppa)
/* HPUX-HPPA (PA-RISC) has an undetermined space register corruption issue with nested triggers. This
 * same issue would likely exist with call-ins except call-ins uses the slower longjmp() method to return.
 * For this one platform, we adopt the longjmp() return method to avoid the problems.
 */
void ci_ret_code(void);		/* Defined in gtmci.h but want to avoid pulling that into this module */
#else
/* All other platforms use this much faster direct return */
void gtm_levl_ret_code(void);
#endif
STATICFNDEF int gtm_trigger_invoke(void);

/* gtm_trigger - saves (some of) current environment, sets up new environment and drives a trigger.

   Triggers are one of two places where compiled M code is driven while the C stack is not at a constant level.
   The other place that does this is call-ins. Because this M code invocation needs to be separate from other
   running code, a new running environment is setup with its own base frame to prevent random unwinding back
   into earlier levels. All returns from the invoked generated code come back through gtm_trigger_invoke() with
   the exception of error handling looking for a handler or not having an error "handled" (clearing $ECODE) can
   just keep unwinding until all trigger levels are gone.

   Trigger names:

   Triggers have a base name set by MUPIP TRIGGER in the TRIGNAME hasht entry which is read by gv_trigger.c and
   passed to us. If it collides with an existing trigger name, we add some suffixing to it (up to two chars)
   and create it with that name.

   Trigger compilation:

   - When a trigger is presented to us for the first time, it needs to be compiled. We do this by writing it out
     using a system generated unique name to a temp file and compiling it with the -NAMEOFRTN parameter which
     sets the name of the routine different than the unique random object name.
   - The file is then linked in and its address recorded so the compilation only happens once.

   Trigger M stack format:

   - First created frame is a "base frame" (created by base_frame). This frame is set up to return to us
     (the caller) and has no backchain (old_frame_pointer is null). It also has the type SFT_TRIGR | SFT_COUNT
     so it is a counted frame (it is important to be counted so the mv_stents we create don't try to backup to
     a previous counted frame.
   - The second created frame is for the trigger being executed. We fill in the stack_frame from the trigger
     description and then let it rip by calling dm_start(). When the trigger returns through the base frame
     which calls gtm_levl_ret_code and pulls the return address of our call to dm_start off the stack and
     unwinds the appropriate saved regs, it returns back to us.

   Error handling in a trigger frame:

   - $ETRAP only. $ZTRAP is forbidden. Standard rules apply.
   - Error handling does not return to the trigger base frame but unwinds the base frame doing a rollback if
     necessary.
*/

CONDITION_HANDLER(gtm_trigger_complink_ch)
{	/* Condition handler for trigger compilation and link - be noisy but don't error out. Note that compilations do
	 * have their own handler but other errors are still possible. The primary use of this handler is (1) to remove
	 * the mv_stent we created and (2) most importantly to turn off the trigger_compile flag.
	 */
	START_CH;
	trigger_compile = FALSE;
	run_time = gtm_trigger_comp_prev_run_time;
	if (((unsigned char *)mv_chain == msp) && (MVST_MSAV == mv_chain->mv_st_type)
	    && (&dollar_zsource == mv_chain->mv_st_cont.mvs_msav.addr))
	{	/* Top mv_stent is one we pushed on there - get rid of it */
		dollar_zsource = mv_chain->mv_st_cont.mvs_msav.v;
		POP_MV_STENT();
	}
	if (DUMPABLE)
		/* Treat fatal errors thusly for a ch that may give better diagnostics */
		NEXTCH;
	if (ERR_TRIGCOMPFAIL != SIGNAL)
	{
		/* Echo error message if not general trigger compile failure message (which gtm_trigger outputs anyway */
		PRN_ERROR;
	}
	if ((SUCCESS == SEVERITY) || (INFO == SEVERITY))
	{	/* Just keep going for non-error issues */
		CONTINUE;
	}
	UNWIND(NULL, NULL);
}

CONDITION_HANDLER(gtm_trigger_ch)
{	/* Condition handler for trigger execution - This handler is pushed on first for a given trigger level, then
	   mdb_condition_handler is pushed on so will appearr multiple times as trigger depth increases. There is
	   always an mdb_condition_handler behind us for an earlier trigger level and we let it handle severe
	   errors for us as it gives better diagnostics (e.g. GTM_FATAL_ERROR dumps) in addition to the file core dump.
	*/
	START_CH;
	DBGTRIGR((stderr, "gtm_trigger_ch: Failsafe condition cond handler entered with SIGNAL = %d\n", SIGNAL));
	if (DUMPABLE)
		/* Treat fatal errors thusly */
		NEXTCH;
	if ((SUCCESS == SEVERITY) || (INFO == SEVERITY))
	{	/* Just keep going for non-error issues */
		CONTINUE;
	}
	mumps_status = SIGNAL;
	gtm_trigger_depth--;	/* Bypassing gtm_trigger_invoke() so do maint on depth indicator */
	assert(0 <= gtm_trigger_depth);
	/* Return back to gtm_trigger with error code */
	UNWIND(NULL, NULL);
}

STATICFNDEF int gtm_trigger_invoke(void)
{	/* Invoke trigger M routine. Separate so error returns to gtm_trigger with proper retcode */
	int	rc;

	ESTABLISH_RET(gtm_trigger_ch, mumps_status);
	gtm_trigger_depth++;
	DBGTRIGR((stderr, "gtm_trigger: Dispatching trigger at depth %d\n", gtm_trigger_depth));
	assert(0 < gtm_trigger_depth);
	assert(GTM_TRIGGER_DEPTH_MAX >= gtm_trigger_depth);
	rc = dm_start();
	gtm_trigger_depth--;
	DBGTRIGR((stderr, "gtm_trigger: Trigger returns with rc %d\n", rc));
	REVERT;
	assert(frame_pointer->type & SFT_TRIGR);
	assert(0 <= gtm_trigger_depth);
	return rc;
}

int gtm_trigger_complink(gv_trigger_t *trigdsc, boolean_t dolink)
{
	char		rtnname[GTM_PATH_MAX + 1];
	char		objname[GTM_PATH_MAX + 1];
	char		zcomp_parms[(GTM_PATH_MAX * 2) + SIZEOF(mident_fixed) + SIZEOF(OBJECT_PARM) + SIZEOF(NAMEOFRTN_PARM)];
	mstr		save_zsource;
	int		rtnfd, rc, lenrtnname, lenobjname, len;
	char		*mident_suffix_p1, *mident_suffix_p2, *mident_suffix_top, *namesub1, *namesub2, *zcomp_parms_ptr;
	mval		zlfile, zcompprm;

	ESTABLISH_RET(gtm_trigger_complink_ch, ((0 == error_condition) ? dollar_zcstatus : error_condition ));
	 /* Verify there are 2 available chars for uniqueness */
	assert((MAX_MIDENT_LEN - TRIGGER_NAME_RESERVED_SPACE) > (trigdsc->rtn_desc.rt_name.len));
	assert(NULL == trigdsc->rtn_desc.rt_adr);
	gtm_trigger_comp_prev_run_time = run_time;
	run_time = TRUE;	/* Required by compiler */
	/* Verify the routine name set by MUPIP TRIGGER and read by gvtr_db_read_hasht() is not in use */
	if (NULL != find_rtn_hdr(&trigdsc->rtn_desc.rt_name))
	{	/* Ooops .. need name to be more unique.. */
		namesub1 = trigdsc->rtn_desc.rt_name.addr + trigdsc->rtn_desc.rt_name.len++;
		mident_suffix_top = (char *)alphanumeric_table + alphanumeric_table_len;
		/* Phase 1. See if any single character can add uniqueness */
		for (mident_suffix_p1 = (char *)alphanumeric_table; mident_suffix_p1 < mident_suffix_top; mident_suffix_p1++)
		{
			*namesub1 = *mident_suffix_p1;
			if (NULL == find_rtn_hdr(&trigdsc->rtn_desc.rt_name))
				break;
		}
		if (mident_suffix_p1 == mident_suffix_top)
		{	/* Phase 2. Phase 1 could not find uniqueness .. Find it with 2 char variations */
			namesub2 = trigdsc->rtn_desc.rt_name.addr + trigdsc->rtn_desc.rt_name.len++;
			for (mident_suffix_p1 = (char *)alphanumeric_table; mident_suffix_p1 < mident_suffix_top;
			     mident_suffix_p1++)
			{	/* First char loop */
				for (mident_suffix_p2 = (char *)alphanumeric_table; mident_suffix_p2 < mident_suffix_top;
				     mident_suffix_p2++)
				{	/* 2nd char loop */
					*namesub1 = *mident_suffix_p1;
					*namesub2 = *mident_suffix_p2;
					if (NULL == find_rtn_hdr(&trigdsc->rtn_desc.rt_name))
					{
						mident_suffix_p1 = mident_suffix_top + 1;	/* Break out of both loops */
						break;
					}
				}
			}
			if (mident_suffix_p1 == mident_suffix_top)
			{	/* Phase 3: Punt */
				assert(FALSE);
				rts_error(VARLSTCNT(5) ERR_TRIGNAMEUNIQ, 3, trigdsc->rtn_desc.rt_name.len - 2,
					  trigdsc->rtn_desc.rt_name.addr, alphanumeric_table_len * alphanumeric_table_len);
			}
		}
	}
	/* Write trigger execute string out to temporary file and compile it */
	assert(MAX_SRCLINE > (trigdsc->xecute_str.str.len + 1));
	SPRINTF(rtnname, "%s/trgtmpXXXXXXXXXX", DEFAULT_GTM_TMP);
	assert(GTM_PATH_MAX > strlen(rtnname));
	rtnfd = mkstemp(rtnname);
	if (0 > rtnfd)
		rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("mkstemp()"), CALLFROM, errno);
	rc = 0;
#	ifdef GEN_TRIGCOMPFAIL_ERROR
	{	/* Used ONLY to generate an error in a trigger compile by adding some junk in a previous line */
		DOWRITERC(rtnfd, ERROR_CAUSING_JUNK, strlen(ERROR_CAUSING_JUNK), rc); /* BYPASSOK */
		if (0 != rc)
			rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("write()"), CALLFROM, rc);
	}
#	endif
	DOWRITERC(rtnfd, PREFIX_SPACE, strlen(PREFIX_SPACE), rc);	/* BYPASSOK */
	if (0 != rc)
		rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("write()"), CALLFROM, rc);
	DOWRITERC(rtnfd, trigdsc->xecute_str.str.addr, trigdsc->xecute_str.str.len, rc);
	if (0 != rc)
		rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("write()"), CALLFROM, rc);
	DOWRITERC(rtnfd, NEWLINE, strlen(NEWLINE), rc);			/* BYPASSOK */
	if (0 != rc)
		rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("write()"), CALLFROM, rc);
	CLOSEFILE(rtnfd, rc);
	if (0 != rc)
		rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("close()"), CALLFROM, rc);
	assert(MAX_MIDENT_LEN > trigdsc->rtn_desc.rt_name.len);
	zcomp_parms_ptr = zcomp_parms;
	lenrtnname = STRLEN(rtnname);
	MEMCPY_LIT(zcomp_parms_ptr, NAMEOFRTN_PARM);
	zcomp_parms_ptr += STRLEN(NAMEOFRTN_PARM);
	memcpy(zcomp_parms_ptr, trigdsc->rtn_desc.rt_name.addr, trigdsc->rtn_desc.rt_name.len);
	zcomp_parms_ptr += trigdsc->rtn_desc.rt_name.len;
	MEMCPY_LIT(zcomp_parms_ptr, OBJECT_PARM);
	zcomp_parms_ptr += STRLEN(OBJECT_PARM);
	strcpy(objname, rtnname);		/* Make copy of rtnname to become object name */
	strcat(objname, OBJECT_FTYPE);		/* Turn into object file reference */
	lenobjname = lenrtnname + STRLEN(OBJECT_FTYPE);
	memcpy(zcomp_parms_ptr, objname, lenobjname);
	zcomp_parms_ptr += lenobjname;
	*zcomp_parms_ptr++ = ' ';
	memcpy(zcomp_parms_ptr, rtnname, lenrtnname);
	zcomp_parms_ptr += lenrtnname;
	*zcomp_parms_ptr = '\0';		/* Null tail */
	len = INTCAST(zcomp_parms_ptr - zcomp_parms);
	assert((SIZEOF(zcomp_parms) - 1) > len);	/* Verify no overflow */
	zcompprm.mvtype = MV_STR;
	zcompprm.str.addr = zcomp_parms;
	zcompprm.str.len = len;
	/* Backup dollar_zsource so trigger doesn't show */
	PUSH_MV_STENT(MVST_MSAV);
	mv_chain->mv_st_cont.mvs_msav.v = dollar_zsource;
	mv_chain->mv_st_cont.mvs_msav.addr = &dollar_zsource;
	trigger_compile = TRUE;		/* Set flag so generates OC_FETCH instead of OC_LINEFETCH */
	op_zcompile(&zcompprm, FALSE);	/* Compile but don't require a .m file extension */
	trigger_compile = FALSE;	/* compile_source_file() establishes handler so always returns */
	if (0 != dollar_zcstatus)
	{	/* Someone err'd.. */
		run_time = gtm_trigger_comp_prev_run_time;
		REVERT;
		UNLINK(objname);	/* Remove files before return error */
		UNLINK(rtnname);
		return ERR_TRIGCOMPFAIL;
	}
	if (dolink)
	{	/* Link is optional as MUPIP TRIGGER doesn't need link */
		zlfile.mvtype = MV_STR;
		zlfile.str.addr = objname;
		zlfile.str.len = lenobjname;
		/* Specifying literal_null for a second arg (as opposed to NULL or 0) allows us to specify
		 * linking the object file (no compilation or looking for source). The 2nd arg is parms for
		 * recompilation and is non-null in an explicit zlink which we need to emulate.
		 */
#		ifdef GEN_TRIGLINKFAIL_ERROR
		UNLINK(objname);				/* delete object before it can be used */
#		endif
		op_zlink(&zlfile, (mval *)&literal_null);	/* need cast due to "extern const" attributes */
		/* No return here if link fails for some reason */
		trigdsc->rtn_desc.rt_adr = find_rtn_hdr(&trigdsc->rtn_desc.rt_name);
		if (NULL == trigdsc->rtn_desc.rt_adr)
			GTMASSERT;	/* Can't find routine we just put there? Catastrophic if happens */
		/* Replace the randomly generated source name with the constant "GTM Trigger" */
		trigdsc->rtn_desc.rt_adr->src_full_name.addr = GTM_TRIGGER_SOURCE_NAME;
		trigdsc->rtn_desc.rt_adr->src_full_name.len = STRLEN(GTM_TRIGGER_SOURCE_NAME);
	}
	if (MVST_MSAV == mv_chain->mv_st_type && &dollar_zsource == mv_chain->mv_st_cont.mvs_msav.addr)
	{       /* Top mv_stent is one we pushed on there - restore dollar_zsource and get rid of it */
		dollar_zsource = mv_chain->mv_st_cont.mvs_msav.v;
		POP_MV_STENT();
	} else
		assert(FALSE); 	/* This mv_stent should be the one we just pushed */
	/* Remove temporary files created */
	UNLINK(objname);	/* Delete the object file first since rtnname is the unique key */
	UNLINK(rtnname);	/* Delete the source file */
	run_time = gtm_trigger_comp_prev_run_time;
	REVERT;
	return 0;
}

int gtm_trigger(gv_trigger_t *trigdsc, gtm_trigger_parms *trigprm)
{
	mval		*lvvalue;
	lnr_tabent	*lbl_offset_p;
	uchar_ptr_t	transfer_addr;
	lv_blk		*lvbp;
	lv_val		*lvval;
	mname_entry	*mne_p;
	uint4		*indx_p;
	ht_ent_mname	*tabent;
	boolean_t	added;
	int		clrlen, rc, i, unwinds;
	mval		**lvvalarray;
	mv_stent	*mv_st_ent;
	symval		*new_symval;
	short		dollar_tlevel_start;
	stack_frame	*fp, *fpprev;
	DEBUG_ONLY(int4	dlevel;)

	assert(!skip_dbtriggers);	/* should not come here if triggers are not supposed to be invoked */
	assert(trigdsc);
	assert(trigprm);
	assert((MV_STR & trigdsc->xecute_str.mvtype) && (NULL != trigdsc->xecute_str.str.addr)
	       && (0 != trigdsc->xecute_str.str.len));
	assert(0 < dollar_tlevel);

	/* Determine if trigger needs to be compiled */
	if (NULL == trigdsc->rtn_desc.rt_adr)
	{	/* No routine hdr addr exists. Need to do compile */
		if (0 != gtm_trigger_complink(trigdsc, TRUE))
		{
			PRN_ERROR;	/* Leave record of what error caused the compilation failure if any */
			rts_error(VARLSTCNT(4) ERR_TRIGCOMPFAIL, 2, trigdsc->rtn_desc.rt_name.len, trigdsc->rtn_desc.rt_name.addr);
		}
	}
	assert(trigdsc->rtn_desc.rt_adr);
	assert(trigdsc->rtn_desc.rt_adr == CURRENT_RHEAD_ADR(trigdsc->rtn_desc.rt_adr));

	/* Setup trigger environment stack frame(s) for execution */
	if (!(frame_pointer->type & SFT_TRIGR))
	{	/* Create new trigger base frame first that back-stops stack unrolling and return to us */
		if (GTM_TRIGGER_DEPTH_MAX < (gtm_trigger_depth + 1))	/* Verify we won't nest too deep */
			rts_error(VARLSTCNT(3) ERR_MAXTRIGNEST, 1, GTM_TRIGGER_DEPTH_MAX);
		DBGTRIGR((stderr, "gtm_trigger: PUSH: frame_pointer 0x%016lx  ctxt value: 0x%016lx\n", frame_pointer, ctxt));
		frame_pointer->flags |= SFF_TRIGR_CALLD;	/* Do not return to this frame via MUM_TSTART */
		base_frame(trigdsc->rtn_desc.rt_adr);
		/* Finish base frame initialization - reset mpc/context to return to us without unwinding base frame */
		frame_pointer->type |= SFT_TRIGR;
#		if defined(__hpux) && defined(__hppa)
		/* For HPUX-HPPA (PA-RISC), we use longjmp() to return to gtm_trigger() to avoid some some space register
		 * corruption issues. Use call-ins already existing mechanism for doing this. Although we no longer support
		 * HPUX-HPPA for triggers due to some unlocated space register error, this code (effectively always ifdef'd
		 * out) left in in case it gets resurrected in the future (01/2010 SE).
		 */
		frame_pointer->mpc = CODE_ADDRESS(ci_ret_code);
		frame_pointer->ctxt = GTM_CONTEXT(ci_ret_code);
#		else
		frame_pointer->mpc = CODE_ADDRESS(gtm_levl_ret_code);
		frame_pointer->ctxt = GTM_CONTEXT(gtm_levl_ret_code);
#endif
		/* This base stack frame is also where we save environmental info for all triggers invoked at this stack level.
		 * Subsequent triggers fired at this level in this trigger invocation need only reinitialize a few things but
		 * can avoid "the big save".
		 */
		if (NULL == trigr_symval_list)
		{	/* No available symvals for use with this trigger, create one */
			symbinit();	/* Initialize a symbol table the trigger will use */
			curr_symval->trigr_symval = TRUE;	/* Mark as trigger symval so will be saved not decommissioned */
		} else
		{	/* Trigger symval is available for reuse */
			new_symval = trigr_symval_list;
			assert(new_symval->trigr_symval);
			trigr_symval_list = new_symval->last_tab;		/* dequeue new curr_symval from list */
			new_symval->last_tab = curr_symval;
			REINIT_SYMVAL_BLK(new_symval, curr_symval);
			curr_symval = new_symval;
			PUSH_MV_STENT(MVST_STAB);
			mv_chain->mv_st_cont.mvs_stab = new_symval;		/* So unw_mv_ent() can requeue it for later use */
		}
		/* Push our trigger environment save mv_stent onto the chain */
		PUSH_MV_STENT(MVST_TRIGR);
		mv_st_ent = mv_chain;
		/* Initialize the mv_stent elements processed by stp_gcol() which can be called by either op_gvsavtarg() or
		 * by the extnam saving code below. This initialization keeps stp_gcol() (should it be called) from attempting
		 * to process unset fields filled with garbage in them as valid mstr address/length pairs.
		 */
		mv_st_ent->mv_st_cont.mvs_trigr.savtarg.str.len = 0;
		mv_st_ent->mv_st_cont.mvs_trigr.savextref.len = 0;
		mv_st_ent->mv_st_cont.mvs_trigr.dollar_etrap_save.str.len = 0;
		mv_st_ent->mv_st_cont.mvs_trigr.dollar_ztrap_save.str.len = 0;
		mv_st_ent->mv_st_cont.mvs_trigr.saved_dollar_truth = dollar_truth;
		op_gvsavtarg(&mv_st_ent->mv_st_cont.mvs_trigr.savtarg);
		if (extnam_str.len)
		{
			ENSURE_STP_FREE_SPACE(extnam_str.len);
			mv_st_ent->mv_st_cont.mvs_trigr.savextref.addr = (char *)stringpool.free;
			memcpy(mv_st_ent->mv_st_cont.mvs_trigr.savextref.addr, extnam_str.addr, extnam_str.len);
			stringpool.free += extnam_str.len;
			assert(stringpool.free <= stringpool.top);
		}
		mv_st_ent->mv_st_cont.mvs_trigr.savextref.len = extnam_str.len;
		mv_st_ent->mv_st_cont.mvs_trigr.ztcode_save = dollar_ztcode;
		mv_st_ent->mv_st_cont.mvs_trigr.ztdata_save = dollar_ztdata;
		mv_st_ent->mv_st_cont.mvs_trigr.ztoldval_save = dollar_ztoldval;
		mv_st_ent->mv_st_cont.mvs_trigr.ztriggerop_save = dollar_ztriggerop;
		mv_st_ent->mv_st_cont.mvs_trigr.ztupdate_save = dollar_ztupdate;
		mv_st_ent->mv_st_cont.mvs_trigr.ztvalue_save = dollar_ztvalue;
		mv_st_ent->mv_st_cont.mvs_trigr.ztvalue_changed_ptr = ztvalue_changed_ptr;
#		ifdef DEBUG
		/* In a debug process, these fields give clues of what trigger we are working on */
		mv_st_ent->mv_st_cont.mvs_trigr.gtm_trigdsc_last_save = trigdsc;
		mv_st_ent->mv_st_cont.mvs_trigr.gtm_trigprm_last_save = trigprm;
#		endif
		assert(((0 == gtm_trigger_depth) && (ch_at_trigger_init == ctxt->ch))
		       || ((0 < gtm_trigger_depth) && (&mdb_condition_handler == ctxt->ch)));
		mv_st_ent->mv_st_cont.mvs_trigr.ctxt_save = ctxt;
		mv_st_ent->mv_st_cont.mvs_trigr.gtm_trigger_depth_save = gtm_trigger_depth;
		if (0 == gtm_trigger_depth)
		{	/* Only back up $*trap settings when initiating the first trigger level */
			mv_st_ent->mv_st_cont.mvs_trigr.dollar_etrap_save = dollar_etrap;
			mv_st_ent->mv_st_cont.mvs_trigr.dollar_ztrap_save = dollar_ztrap;
			mv_st_ent->mv_st_cont.mvs_trigr.ztrap_explicit_null_save = ztrap_explicit_null;
			dollar_ztrap.str.len = 0;
			ztrap_explicit_null = FALSE;
			if (NULL != gtm_trigger_etrap.str.addr)
				/* An etrap was defined for the trigger environment - Else existing $etrap persists */
				dollar_etrap = gtm_trigger_etrap;
		}
		mv_st_ent->mv_st_cont.mvs_trigr.mumps_status_save = mumps_status;
		mv_st_ent->mv_st_cont.mvs_trigr.run_time_save = run_time;
		mv_st_ent->mv_st_next = mv_chain->mv_st_next;	/* Copy since new entry is being replaced */
		mumps_status = 0;
		run_time = TRUE;	/* Previous value saved just above restored when frame pops */
	} else
	{	/* Trigger base frame exists so reinitialize the symbol table for new trigger invocation */
		REINIT_SYMVAL_BLK(curr_symval, curr_symval->last_tab);
		/* Locate the MVST_TRIGR mv_stent containing the backed up values. Some of those values need
		 * to be restored so the 2nd trigger has the same environment as the previous trigger at this level
		 */
		for (mv_st_ent = mv_chain;
		     (NULL != mv_st_ent) && (MVST_TRIGR != mv_st_ent->mv_st_type);
		     mv_st_ent = (mv_stent *)(mv_st_ent->mv_st_next + (char *)mv_st_ent))
			;
		assert(NULL != mv_st_ent);
		assert((char *)mv_st_ent < (char *)frame_pointer); /* Ensure mv_stent associated this trigger frame */
		/* Reinit backed up values from the trigger environment backup */
		dollar_truth = mv_st_ent->mv_st_cont.mvs_trigr.saved_dollar_truth;
		op_gvrectarg(&mv_st_ent->mv_st_cont.mvs_trigr.savtarg);
		extnam_str.len = mv_st_ent->mv_st_cont.mvs_trigr.savextref.len;
		if (extnam_str.len)
			memcpy(extnam_str.addr, mv_st_ent->mv_st_cont.mvs_trigr.savextref.addr, extnam_str.len);
		mumps_status = 0;
		assert(run_time);
		/* Note we do not reset the handlers for parallel triggers - set one time only when enter first level
		 * trigger. After that, whatever happens in trigger world, stays in trigger world.
		 */
	}
	assert(frame_pointer->type & SFT_TRIGR);
#	ifdef DEBUG
	gtm_trigdsc_last = trigdsc;
	gtm_trigprm_last = trigprm;
#	endif

	/* Set new value of trigger ISVs. Previous values already saved in trigger base frame */
	dollar_ztcode = &trigdsc->xecute_str;
	dollar_ztdata = (mval *)trigprm->ztdata_new;
	dollar_ztoldval = trigprm->ztoldval_new;
	dollar_ztriggerop = (mval *)trigprm->ztriggerop_new;
	dollar_ztupdate = trigprm->ztupdate_new;
	dollar_ztvalue = trigprm->ztvalue_new;
	ztvalue_changed_ptr = &trigprm->ztvalue_changed;

	/* Set values associated with trigger into symbol table */
	lvvalarray = trigprm->lvvalarray;
	for (i = 0, mne_p = trigdsc->lvnamearray, indx_p = trigdsc->lvindexarray;
	     i < trigdsc->numlvsubs; ++i, ++mne_p, ++indx_p)
	{	/* Once thru for each subscript we are to set */
		lvvalue = lvvalarray[*indx_p];			/* Locate mval that contains value */
		assert(NULL != lvvalue);
		assert(MV_DEFINED(lvvalue));			/* No sense in defining the undefined */
		lvval = lv_getslot(curr_symval);		/* Allocate an lvval to put into symbol table */
		LVVAL_INIT(lvval, curr_symval);
		lvval->v = *lvvalue;				/* Copy mval into lvval */
		added = add_hashtab_mname_symval(&curr_symval->h_symtab, mne_p, lvval, &tabent);
		assert(added);
		assert(NULL != tabent);
	}

	/* While the routine header is available in trigdsc, we also need the <null> label address associated with
	 *  the first (and only) line of code.
	 */
	lbl_offset_p = LNRTAB_ADR(trigdsc->rtn_desc.rt_adr);
	transfer_addr = (uchar_ptr_t)LINE_NUMBER_ADDR(trigdsc->rtn_desc.rt_adr, lbl_offset_p);
	/* Create new stack frame for invoked trigger in same fashion as gtm_init_env() creates its 2ndary frame */
#ifdef HAS_LITERAL_SECT
	new_stack_frame(trigdsc->rtn_desc.rt_adr, (unsigned char *)LINKAGE_ADR(trigdsc->rtn_desc.rt_adr), transfer_addr);
#else
	/* Any platform that does not follow pv-based linkage model either
	 *	(1) uses the following calculation to determine the context pointer value, or
	 *	(2) doesn't need a context pointer
	 */
	new_stack_frame(trigdsc->rtn_desc.rt_adr, PTEXT_ADR(trigdsc->rtn_desc.rt_adr), transfer_addr);
#endif
	dollar_tlevel_start = dollar_tlevel;
	/* Invoke trigger generated code */
	rc = gtm_trigger_invoke();
	if (1 == rc)
	{	/* Normal return code (from dm_start). Check if TP has been unwound or not */
		assert(dollar_tlevel <= dollar_tlevel_start);	/* Bigger would be quite the surprise */
		if (dollar_tlevel < dollar_tlevel_start)
		{	/* Our TP level was unwound during the trigger so throw an error */
			DBGTRIGR((stderr, "gtm_trigger: $TLEVEL less than at start - throwing TRIGTLVLCHNG\n"));
			gtm_trigger_fini(TRUE);	/* dump this trigger level */
			rts_error(VARLSTCNT(4) ERR_TRIGTLVLCHNG, 2, trigdsc->rtn_desc.rt_name.len,
				  trigdsc->rtn_desc.rt_name.addr);
		}
		rc = 0;			/* Be polite and return 0 for the (hopefully common) success case */
	} else if (ERR_TPRETRY == rc)
	{	/* We are restarting the entire transaction. There are two possibilities here:
		 * 1) This is a nested trigger level in which case we need to unwind further or
		 *    the outer trigger level was created by M code. If either is true, just
		 *    rethrow the TPRETRY error.
		 * 2) This is the outer trigger and the call to op_tstart() was done by our caller.
		 *    In this case, we just return to our caller with a code signifying they need
		 *    to restart the implied transaction.
		 */
		assert(dollar_tlevel && (tstart_trigger_depth <= gtm_trigger_depth));
		if ((tstart_trigger_depth < gtm_trigger_depth) || !tp_pointer->implicit_tstart)
		{	/* Unwind a trigger level to restart level or to next trigger boundary */
			gtm_trigger_fini(FALSE);	/* Get rid of this trigger level - we won't be returning */
			DBGTRIGR((stderr, "gtm_trigger: dm_start returned rethrow code - rethrowing ERR_TPRETRY\n"));
			INVOKE_RESTART;
		} else
		{	/* It is possible we are restarting a transaction that never got around to creating a base
			 * frame yet the implicit TStart was done. So if there is no trigger base frame, do not
			 * run gtm_trigger_fini() but instead do the one piece of cleanup it does that we still need.
			 */
			assert(donot_INVOKE_MUMTSTART);
			if (SFT_TRIGR & frame_pointer->type)
			{	/* Normal case when TP restart unwinding back to implicit beginning */
				gtm_trigger_fini(FALSE);
				DBGTRIGR((stderr, "gtm_trigger: dm_start returned rethrow code - returning to gvcst_<caller>\n"));
			} else
			{       /* Unusual case of trigger that died in no-mans-land before trigger base frame established.
				 * Remove the "do not return to me" flag only on non-error unwinds */
				assert(tp_pointer->implicit_tstart);
				assert(SFF_TRIGR_CALLD & frame_pointer->flags);
				frame_pointer->flags &= SFF_TRIGR_CALLD_OFF;
				DBGTRIGR((stderr, "gtm_trigger: unwinding no-base-frame trigger for TP restart\n"));
			}
		}
		/* Fall out and return ERR_TPRETRY to caller */
	} else if (0 == rc)
		/* We should never get a return code of 0. This would be out-of-design and a signal that something
		 * is quite broken. We cannot "rethrow" outside the trigger because it was not initially an error so
		 * mdb_condition_handler would have no record of it (rethrown errors must have originally occurred in
		 * or to be RE-thrown) and assert fail at best.
		 */
		GTMASSERT;
	else
	{	/* We have an unexpected return code due to some error during execution of the trigger that tripped
		 * gtm_trigger's safety handler (i.e. an error occurred in mdb_condition_handler() established by
		 * dm_start(). Since we are going to unwind the trigger frame and rethrow the error, we also have
		 * to unwind all the stack frames on top of the trigger frame. Figure out how many frames that is,
		 * unwind them all plus the trigger base frame before rethrowing the error.
		 */
		for (unwinds = 0, fp = frame_pointer; (NULL != fp) && !(SFT_TRIGR & fp->type); fp = fp->old_frame_pointer)
			unwinds++;
		assert((NULL != fp) && (SFT_TRIGR & fp->type));
		GOFRAMES(unwinds, TRUE);
		assert((NULL != frame_pointer) && !(SFT_TRIGR & frame_pointer->type));
		DBGTRIGR((stderr, "gtm_trigger: Unsupported return code (%d) - unwound %d frames and now rethrowing error\n",
			  rc, unwinds));
		rts_error(VARLSTCNT(1) ERR_REPEATERROR);
	}
	return rc;
}

/* Unwind a trigger level, pop all the associated mv_stents and dispense with the base frame.
 * Note that this unwind restores the condition handler stack pointer and correct gtm_trigger_depth value in
 * order to maintain these values properly in the event of a major unwind. This routine is THE routine to use to unwind
 * trigger base frames in all cases due to the cleanups it takes care of.
 */
void gtm_trigger_fini(boolean_t forced_unwind)
{
	if (0 == (frame_pointer->type & SFT_TRIGR))
		GTMASSERT;		/* Would normally be an assert but frame potential stack damage so severe
					   and resulting debug difficulty that we GTMASSERT instead. */
	op_unwind();
	/* restore frame_pointer stored at msp (see base_frame.c) */
	frame_pointer = *(stack_frame**)msp;
	msp += SIZEOF(stack_frame *);		/* Remove frame save pointer from stack */
	if (!forced_unwind)
	{	/* Remove the "do not return to me" flag only on non-error unwinds */
		assert(SFF_TRIGR_CALLD & frame_pointer->flags);
		frame_pointer->flags &= SFF_TRIGR_CALLD_OFF;
	} else
	{	/* Error unwind, make sure certain cleanups are done */
#		ifdef DEBUG
		assert(!dollar_tlevel || (tstart_trigger_depth <= gtm_trigger_depth));
		if (tstart_trigger_depth == gtm_trigger_depth) /* Unwinding gvcst_put() so get rid of flag it potentially set */
			donot_INVOKE_MUMTSTART = FALSE;
#		endif
		if (tp_pointer && (tp_pointer->fp == frame_pointer) && tp_pointer->implicit_tstart)
			op_trollback(-1);	/* We just unrolled the implicitly started TSTART so unroll what it did */
	}
	DBGTRIGR((stderr, "gtm_trigger: POP: frame_pointer 0x%016lx  ctxt value: 0x%016lx\n", frame_pointer, ctxt));
}

/* Routine to eliminate the zlinked trigger code for a given trigger about to be deleted. Operations performed
   differ depending on platform type (shared binary or not).
*/
void gtm_trigger_cleanup(gv_trigger_t *trigdsc)
{
	rtn_tabent	*rbot, *mid, *rtop;
	mident		*rtnname;
	rhdtyp		*rtnhdr;
	textElem	*telem;
	int		comp, size;

	/* First thing to do is find the routine header in the rtn_names list so we can remove it. */
	rtnname = &trigdsc->rtn_desc.rt_name;
	rbot = rtn_names;
	rtop = rtn_names_end;
	for (;;)
	{	/* See if routine exists in list via a binary search which reverts to serial
		   search when # of items drops below the threshold S_CUTOFF.
		*/
		if ((rtop - rbot) < S_CUTOFF)
		{
			comp = -1;
			for (mid = rbot; mid <= rtop ; mid++)
			{
				MIDENT_CMP(&mid->rt_name, rtnname, comp);
				if (0 == comp)
					break;
				if (0 < comp)
					GTMASSERT;	/* Routine should be found */
			}
			break;
		} else
		{	mid = rbot + (rtop - rbot)/2;
			MIDENT_CMP(&mid->rt_name, rtnname, comp);
			if (0 == comp)
				break;
			else if (0 > comp)
			{
				rbot = mid + 1;
				continue;
			} else
			{
				rtop = mid - 1;
				continue;
			}
		}
	}
	/* Remove the routine from the rtn_table */
	size = INTCAST((char *)rtn_names_end - (char *)mid);
	if (0 < size)
		memcpy((char *)mid, (char *)(mid + 1), size);	/* Remove this routine name from sorted table */
	rtn_names_end--;
	rtnhdr = trigdsc->rtn_desc.rt_adr;
	zr_remove(rtnhdr);					/* Remove any break points in this routine */
	urx_remove(rtnhdr);					/* Remove any unresolved entries */
#	ifdef USHBIN_SUPPORTED
	stp_move((char *)rtnhdr->literal_text_adr,
		 (char *)(rtnhdr->literal_text_adr + rtnhdr->literal_text_len));
	GTM_TEXT_FREE(rtnhdr->ptext_adr);			/* R/O releasable section */
	free(rtnhdr->literal_adr);				/* R/W releasable section part 1 */
	free(rtnhdr->linkage_adr);				/* R/W releasable section part 2 */
	free(rtnhdr->labtab_adr);				/* Usually non-releasable but triggers don't have labels so
								   this is just cleaning up a dangling null malloc */
	free(rtnhdr);
#	else
#	  if !defined(__linux__) || !defined(__i386) || !defined(COMP_GTA)
#	    error Unsupported NON-USHBIN platform
#	  endif
	/* For a non-shared binary platform we need to get an approximate addr range for stp_move. This is not
	   done when a routine is replaced on these platforms but in this case we are able to due to the isolated
	   environment if we only take the precautions of migrating potential literal text which may have been
	   pointed to by any set environment variables.

	   In this format, the only platform we support currently is Linux-x86 (i386) which uses GTM_TEXT_ALLOC
	   to allocate special storage for it to put executable code in. We can access the storage header for
	   this storage and find out how big it is and use that information to give stp_move a good range since
	   the literal segment occurs right at the end of allocated storage (for which there is no pointer
	   in the fileheader).
	*/
	telem = (textElem *)((char *)rtnhdr - offsetof(textElem, userStorage));
	assert(TextAllocated == telem->state);
	stp_move((char *)LNRTAB_ADR(rtnhdr) + (rtnhdr->lnrtab_len * SIZEOF(lnr_tabent)),
		 (char *)rtnhdr + telem->realLen);
	GTM_TEXT_FREE(rtnhdr);
#	endif
}
#endif /* GTM_TRIGGER */
