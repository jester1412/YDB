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

#include "gtm_string.h"

#include "rtnhdr.h"
#include "stack_frame.h"
#include "op.h"
#include "hashtab_mname.h"
#include "hashtab.h"
#include "lv_val.h"
#include "get_ret_targ.h"
#include "xfer_enum.h"
#include "dollar_quit.h"

/* Determine value to return for $QUIT:
 *
 *   0 - no return value requested
 *   1 - non-alias return value requested
 *   3 - alias return value requrested
 *
 * Determination of parm/no-parm is made by calling get_ret_targ() which searches the mv_stents
 * on the stack back to a counted frame to see of an MVST_PARM block containg a return mval was pushed
 * onto the stack signifying a return value is required. If a return value is required, determination of
 * the type of return value is made by examining the generated instruction stream at the return point
 * and checking for an OC_EXFUNRET or OC_EXFUNRETALS (non-alias and alias type return var processor
 * respectively) opcode following the return point. This is done by isolating the instruction that
 * indexes into the transfer table, extracting the xfer-table index and checking against known values
 * for op_exfunret and op_exfunretals to determine type of return. No match means no return value.
 *
 * Because this routine looks at the generated code stream at the return point, it is highly platform
 * dependent.
 */
int dollar_quit(void)
{
	stack_frame	*sf;
	mval		*parm_blk;
	int		retval;
	int		xfer_index;

	union
	{
		char		*instr;
		unsigned short	*instr_type;
		char		*xfer_offset_8;
		short		*xfer_offset_16;
		int		*xfer_offset_32;
	} ptrs;

	parm_blk = get_ret_targ(&sf);
	if (NULL == parm_blk)
		/* There was no parm block - return 0 */
		retval = 0;
	else
	{	/* There is a parm block - see if they want a "regular" or alias type return argument */
		sf = sf->old_frame_pointer;		/* Caller's frame */
		ptrs.instr = (char *)sf->mpc + EXFUNRET_INST_OFFSET;
#		if defined(__x86_64__) || defined(__i386)
		{
			if (0x53FF == *ptrs.instr_type)
			{	/* Short format CALL */
				ptrs.instr += SIZEOF(*ptrs.instr_type);
				xfer_index = *ptrs.xfer_offset_8 / SIZEOF(void *);
			} else if (0x93FF == *ptrs.instr_type)
			{	/* Long format CALL */
				ptrs.instr += SIZEOF(*ptrs.instr_type);
				xfer_index = *ptrs.xfer_offset_32 / SIZEOF(void *);
			} else
				xfer_index = -1;	/* Not an xfer index */
		}
#		elif defined(_AIX)
		{
			if (0xE97C == *ptrs.instr_type)
			{	/* ld of descriptor address from xfer table */
				ptrs.instr += SIZEOF(*ptrs.instr_type);
				xfer_index = *ptrs.xfer_offset_16 / SIZEOF(void *);
			} else
				xfer_index = -1;
		}
#		elif defined(__alpha)	/* Applies to both VMS and Tru64 as have same codegen */
		{
			if (UNIX_ONLY(0xA36C) VMS_ONLY(0xA36B) == *(ptrs.instr_type + 1))
				/* ldl of descriptor address from xfer table - little endian - offset prior to opcode*/
				xfer_index = *ptrs.xfer_offset_16 / SIZEOF(void *);
			else
				xfer_index = -1;
		}
#		elif defined(__sparc)
		{
			if (0xC85C == *ptrs.instr_type)
			{	/* ldx of rtn address from xfer table */
				ptrs.instr += SIZEOF(*ptrs.instr_type);
				xfer_index = (*ptrs.xfer_offset_16 & SPARC_MASK_OFFSET) / SIZEOF(void *);
			} else
				xfer_index = -1;
		}
#		elif defined(__MVS__)
		{
			format_RXY	instr_LG;
			union
			{
				int	offset;
				struct
				{	/* Used to reassemble the offset in the LG instruction */
#					ifdef BIGENDIAN
					int	offset_unused:12;
					int	offset_hi:8;
					int	offset_low:12;
#					else
					int	offset_low:12;
					int	offset_hi:8;
					int	offset_unused:12;
#					endif
				} instr_LG_bits;
			} RXY;

			memcpy(&instr_LG, ptrs.instr, SIZEOF(instr_LG));
			if ((S390_OPCODE_RXY_LG == instr_LG.opcode) && (S390_SUBCOD_RXY_LG == instr_LG.opcode2)
			    && (REG_R6 == instr_LG.r1) && (GTM_REG_XFER_TABLE == instr_LG.b2))
			{	/* LG of rtn address from xfer table */
				RXY.offset = 0;
				RXY.instr_LG_bits.offset_hi = instr_LG.dh2;
				RXY.instr_LG_bits.offset_low = instr_LG.dl2;
				xfer_index = RXY.offset / SIZEOF(void *);
			} else
				xfer_index = -1;
		}
#		elif defined(__hppa)
		{
			hppa_fmt_1	instr_LDX;
			union
			{
				int	offset;
				struct
				{
					signed int	high:19;
					unsigned int	low:13;
				} instr_offset;
			} fmt_1;

			memcpy(&instr_LDX, ptrs.instr, SIZEOF(instr_LDX));
			if (((HPPA_INS_LDW >> HPPA_SHIFT_OP) == instr_LDX.pop) && (GTM_REG_XFER_TABLE == instr_LDX.b)
			    && (R22 == instr_LDX.t))
			{	/* ldx of rtn address from xfer table */
				fmt_1.instr_offset.low = instr_LDX.im14a;
				fmt_1.instr_offset.high = instr_LDX.im14b;
				xfer_index = fmt_1.offset / SIZEOF(void *);
			} else
				xfer_index = -1;
		}
#		elif defined(__ia64)
		{
			ia64_bundle	xfer_ref_inst;		/* Buffer to put built instruction into */
			ia64_fmt_A4	adds_inst;		/* The actual adds instruction computing xfer reference */
			union
			{
				int	offset;
				struct
				{
#					ifdef BIGENDIAN
					signed int	sign:19;
					unsigned int	imm6d:6;
					unsigned int	imm7b:7;
#					else
					unsigned int	imm7b:7;
					unsigned int	imm6d:6;
					signed int	sign:19;
#					endif
				} instr_offset;
			} imm14;

#			ifdef BIGENDIAN
			xfer_ref_inst.hexValue.aValue = GTM_BYTESWAP_64(((ia64_bundle *)ptrs.instr)->hexValue.aValue);
			xfer_ref_inst.hexValue.bValue = GTM_BYTESWAP_64(((ia64_bundle *)ptrs.instr)->hexValue.bValue);
#			else
			xfer_ref_inst.hexValue.aValue = ((ia64_bundle *)ptrs.instr)->hexValue.aValue;
			xfer_ref_inst.hexValue.bValue = ((ia64_bundle *)ptrs.instr)->hexValue.bValue;
#			endif
			adds_inst.hexValue = xfer_ref_inst.format.inst3;	/* Extract instruction from bundle */
			if ((8 == adds_inst.format.pop) && (2 == adds_inst.format.x2a)
			    && (GTM_REG_XFER_TABLE == adds_inst.format.r3) && (IA64_REG_SCRATCH1 == adds_inst.format.r1))
			{	/* We have an xfer computation instruction. Find the offset to find which opcode */
				imm14.instr_offset.imm7b = adds_inst.format.imm7b;	/* Low order bits */
				imm14.instr_offset.imm6d = adds_inst.format.imm6d;	/* upper bits minus sign */
				imm14.instr_offset.sign = adds_inst.format.sb;		/* Sign bit propagated */
				xfer_index = imm14.offset / SIZEOF(void *);
			} else
				xfer_index = -1;
		}
#		else
#		  error Unsupported Platform
#		endif
		if (xf_exfunret == xfer_index)
			/* Need a QUIT with a non-alias return value */
			retval = 1;
		else if (xf_exfunretals == xfer_index)
			/* Need a QUIT with an alias return value */
			retval = 11;
		else
		{	/* Something weird afoot but plunge forwards.. */
			assert(FALSE);
			retval = 0;
		}
	}
	return retval;
}
