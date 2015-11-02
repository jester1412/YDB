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

#ifndef __STACK_FRAME_H__
#define __STACK_FRAME_H__

/* stack_frame.h - GT.M MUMPS stack frame */

/* There are several references to this structure from assembly language; these include:

	From VMS:	G_MSF.MAX
	From Vax:       stack_frame_copy.mar
	From OS390:	stack_frame.h (it's own version)
	From Unix:	g_msf.si

   Any changes to the stack frame must be reflected in those files as well.

   Warning: the lists above may not be complete.
*/

#include "hashtab_mname.h"


typedef struct stack_frame_struct	/* contents of the GT.M MUMPS stack frame */
{
	struct rhead_struct *rvector;	/* routine header */
	ht_ent_mname	**l_symtab;	/* local symbol table */
	unsigned char	*mpc;		/* mumps program counter */
	unsigned char	*ctxt;		/* context pointer (base register for use when there's no PC-relative address mode) */
#ifdef HAS_LITERAL_SECT
	int4		*literal_ptr;	/* pointer to base of literals */
#endif
	unsigned char	*temps_ptr;	/* pointer to base of temps */
	char		*vartab_ptr;	/* variable table may be in rvector or on stack */
  	GTM64_ONLY(struct stack_frame_struct *old_frame_pointer;) /* Moved old_frame_pointer near all
								     pointers for alignment and smaller stackframe
								     size
								   */

	short		vartab_len;	/* variable table length */
	short           temp_mvals;     /* temp mval count if this frame for an indirect rtn (ihdtyp) */
        NON_GTM64_ONLY(struct stack_frame_struct *old_frame_pointer;) /* old_frame_pointer remains at the same
									 place for 32-bit platforms
								       */
	/* note the alignment of these next two fields is critical to correct operation of
	   opp_ret on the VMS platforms. Any changes here need to be reflected there.
	*/
	unsigned short	type;
	unsigned char	flags;
	unsigned char   filler;
} stack_frame;

/* Stack frame types */
#define SFT_COUNT	(1 << 0)	/* 0x0001 frame counts (real code) or doesn't (transcendental code) */
#define SFT_DM		(1 << 1)	/* 0x0002 direct mode */
#define SFT_REP_OP	(1 << 2)	/* 0x0004 frame to replace opcode (zbreak has already occured) */
#define SFT_ZBRK_ACT	(1 << 3)	/* 0x0008 action frame for zbreak */
#define SFT_DEV_ACT	(1 << 4)	/* 0x0010 action frame for device error handler */
#define SFT_ZTRAP	(1 << 5)	/* 0x0020 ztrap frame */
#define SFT_EXTFUN	(1 << 6)	/* 0x0040 extfun frame */
#define SFT_ZSTEP_ACT	(1 << 7)	/* 0x0080 action frame for a zstep */
#define SFT_ZINTR	(1 << 8)	/* 0x0100 $zinterrupt frame */
#define SFT_TRIGR	(1 << 9)	/* 0x0200 Trigger base frame */

/* Flags for flag byte */
#define SFF_INDCE	(1 << 0)	/* 0x01 This frame is executing an indirect cache entry */
#define SFF_ZTRAP_ERR 	(1 << 1)	/* 0x02 error occured during $ZTRAP compilation */
#define SFF_DEV_ACT_ERR	(1 << 2)	/* 0x04 compilation error occured in device exception handler */
#define SFF_CI		(1 << 3)	/* 0x08 call-in base frame */
#define SFF_ETRAP_ERR	(1 << 4)	/* 0x10 An $ETRAP style error occurred while in this frame. A return to this frame will
					 *      cause the getframe macro to invoke error_return() for further error processing.
					 */
#define SFF_UNW_SYMVAL	(1 << 5)	/* 0x20 Unwound a symval in this stackframe (relevant to tp_restart) */
#define SFF_TRIGR_CALLD	(1 << 6)	/* 0x40 This frame initiated a trigger call - checked by MUM_TSTART to prevent error
					 * returns to the frame which would cause a restart of error handling.
					 */

#define SFF_INDCE_OFF   	~(SFF_INDCE)		/* Mask to turn off SFF_INDCE */
#define SFF_ZTRAP_ERR_OFF	~(SFF_ZTRAP_ERR)	/* Mask to turn off SFF_ZTRAP_ERR */
#define SFF_DEV_ACT_ERR_OFF	~(SFF_DEV_ACT_ERR)	/* Mask to turn off SFF_DEV_ACT_ERR */
#define SFF_CI_OFF		~(SFF_CI)		/* Mask to turn off SFF_CI */
#define SFF_ETRAP_ERR_OFF	~(SFF_ETRAP_ERR)	/* Mask to turn off SFF_ETRAP_ERR */
#define SFF_UNW_SYMVAL_OFF	~(SFF_UNW_SYMVAL)	/* Mask to turn off SFF_UNW_SYMVAL */
#define SFF_TRIGR_CALLD_OFF	~(SFF_TRIGR_CALLD)	/* Mask to turn off SFF_TRIGR_CALLD */

#define	ADJUST_FRAME_POINTER(fptr, shift)			\
{								\
	GBLREF	stack_frame	*error_frame;			\
	stack_frame		*oldfp;				\
								\
	oldfp = fptr;						\
	fptr = (stack_frame *)((char *)oldfp - shift);		\
	if (error_frame == oldfp)				\
	{	/* Adjust "error_frame" as well */		\
		assert(error_frame >= frame_pointer);		\
		error_frame = fptr;				\
	}							\
}

void new_stack_frame(rhdtyp *rtn_base, unsigned char *context, unsigned char *transfer_addr);
void new_stack_frame_sp(rhdtyp *rtn_base, unsigned char *context, unsigned char *transfer_addr);
int4 symbinit(void);
unsigned char *get_symb_line(unsigned char *out, unsigned char **b_line, unsigned char **ctxt);
unsigned char *symb_line(unsigned char *in_addr, unsigned char *out, unsigned char **b_line, rhdtyp *routine);
void copy_stack_frame(void);
void copy_stack_frame_sp(void);
void exfun_frame(void);
void exfun_frame_sp(void);
void exfun_frame_push_dummy_frame(void);
void base_frame(rhdtyp *base_address);
void pseudo_ret(void);
void call_dm(void);
int dm_start(void);
stack_frame *op_get_msf (void);
int op_zbreak(stack_frame *fp);
void adjust_frames(unsigned char *old_ptext_beg, unsigned char *old_ptext_end, unsigned char *new_ptext_beg);

#endif
