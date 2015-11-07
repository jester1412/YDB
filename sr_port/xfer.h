/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*  There is code that depends on the sequence in which entries
 *  appear in this table.
 *  Please add new entries only at the end of this table
 *  (and, please, avoid deleting any lines).
 */

XFER(xf_sto, op_sto),
XFER(xf_cat, op_cat),
XFER(xf_linefetch, op_linefetch),
XFER(xf_linestart, op_linestart),
XFER(xf_mval2bool, mval2bool),
XFER(xf_zbfetch, op_zbfetch),
XFER(xf_zbstart, op_zbstart),
XFER(xf_fnpiece, op_fnpiece),
XFER(xf_equ, op_equ),
XFER(xf_write, op_write),
XFER(xf_kill, op_kill),
XFER(xf_add, op_add),
XFER(xf_getindx, op_getindx),
XFER(xf_putindx, op_putindx),
XFER(xf_gvnaked, op_gvnaked_fast),
XFER(xf_ret, opp_ret),
XFER(xf_numcmp, op_numcmp),
XFER(xf_fnextract, op_fnextract),
XFER(xf_gvname, op_gvname_fast),
XFER(xf_mval2mint, mval2mint),
XFER(xf_contain, op_contain),
XFER(xf_wteol, op_wteol),
XFER(xf_gvget, op_gvget),
XFER(xf_sub, op_sub),
XFER(xf_fndata, op_fndata),
XFER(xf_callw, op_callw),
XFER(xf_extcall, op_extcall),
XFER(xf_forcenum, op_forcenum),
XFER(xf_srchindx, op_srchindx),
XFER(xf_newvar, opp_newvar),
XFER(xf_extjmp, op_extjmp),
XFER(xf_gvput, op_gvput),
XFER(xf_gvdata, op_gvdata),
XFER(xf_fnlength, op_fnlength),
XFER(xf_svget, op_svget),
XFER(xf_rterror, opp_rterror),
XFER(xf_commarg, opp_commarg),
XFER(xf_gvnext, op_gvnext),
XFER(xf_wttab, op_wttab),
XFER(xf_gvkill, op_gvkill),
XFER(xf_read, op_read),
XFER(xf_neg, op_neg),
XFER(xf_follow, op_follow),
XFER(xf_wtone, op_wtone),
XFER(xf_callb, op_callb),
XFER(xf_mint2mval, mint2mval),
XFER(xf_forinit, op_forinit),
XFER(xf_forloop, op_forloop),
XFER(xf_flt_mod, flt_mod),
XFER(xf_fntext, op_fntext),
XFER(xf_fnnext, op_fnnext),
XFER(xf_idiv, op_idiv),
XFER(xf_fnj2, op_fnj2),
XFER(xf_fnchar, op_fnchar),
XFER(xf_fnfind, op_fnfind),
XFER(xf_indset, opp_indset),
XFER(xf_fnascii, op_fnascii),
XFER(xf_halt, op_halt),
XFER(xf_mul, op_mul),
XFER(xf_indtext, opp_indtext),
XFER(xf_indglvn, opp_indglvn),
XFER(xf_killall, op_killall),
XFER(xf_use, op_use),
XFER(xf_div, op_div),
XFER(xf_fnj3, op_fnj3),
XFER(xf_forlcldol, op_forlcldol),
XFER(xf_forlcldow, op_forlcldow),
XFER(xf_unlock, op_unlock),
XFER(xf_wtff, op_wtff),
XFER(xf_break, opp_break),
XFER(xf_calll, op_calll),
XFER(xf_close, op_close),
XFER(xf_currtn, op_currtn),
XFER(xf_lock, op_lock),
XFER(xf_fetch, gtm_fetch),
XFER(xf_fnfnumber, op_fnfnumber),
XFER(xf_fnget, op_fnget),
XFER(xf_fngetdvi, op_fngetdvi),
XFER(xf_fngetjpi, op_fngetjpi),
XFER(xf_fngetsyi, op_fngetsyi),
XFER(xf_fngvget, op_fngvget),
XFER(xf_fnorder, op_fnorder),
XFER(xf_fnrandom, op_fnrandom),
XFER(xf_fnzfile, op_fnzfile),
XFER(xf_fnzm, op_fnzm),
XFER(xf_fnzparse, op_fnzparse),
XFER(xf_fnzpid, op_fnzpid),
XFER(xf_fnzpriv, op_fnzpriv),
XFER(xf_fnzsearch, op_fnzsearch),
XFER(xf_fnzsetprv, op_fnzsetprv),
XFER(xf_gettruth, op_gettruth),
XFER(xf_zallocate, op_zallocate),
XFER(xf_gvorder, op_gvorder),
XFER(xf_hang, op_hang),
XFER(xf_hardret, opp_hardret),
XFER(xf_indfun, opp_indfun),
XFER(xf_indlvadr, opp_indlvadr),
XFER(xf_indname, op_indname),
XFER(xf_indpat, opp_indpat),
XFER(xf_iretmvad, op_iretmvad),
XFER(xf_iretmval, opp_iretmval),
XFER(xf_job, op_job),
XFER(xf_labaddr, op_labaddr),
XFER(xf_lckincr, op_incrlock),
XFER(xf_lckdecr, op_decrlock),
XFER(xf_lvpatwrite, op_lvpatwrite),
XFER(xf_lvzwrite, op_lvzwrite),
XFER(xf_open, op_open),
XFER(xf_fnpopulation, op_fnpopulation),
XFER(xf_rdone, op_rdone),
XFER(xf_readfl, op_readfl),
XFER(xf_rhdaddr, op_rhdaddr),
XFER(xf_setpiece, op_setpiece),
#ifdef USHBIN_SUPPORTED
XFER(xf_setzbrk, opp_setzbrk), /* need assembly wrapper to handle shared routines - need to burn return pc */
#else
XFER(xf_setzbrk, op_setzbrk),
#endif
XFER(xf_svput, op_svput),
XFER(xf_view, op_view),
XFER(xf_xnew, opp_xnew),
XFER(xf_zcont, opp_zcont),
XFER(xf_zgoto, opp_zgoto),
XFER(xf_zlink, op_zlink),
XFER(xf_zmess, op_zmess),
XFER(xf_zprint, op_zprint),
XFER(xf_zshow, op_zshow),
XFER(xf_zsystem, op_zsystem),
XFER(xf_gvsavtarg, op_gvsavtarg),
XFER(xf_gvrectarg, op_gvrectarg),
XFER(xf_igetsrc, op_igetsrc),
XFER(xf_fnzdate, op_fnzdate),
XFER(xf_fntranslate, op_fntranslate),
XFER(xf_xkill, op_xkill),
XFER(xf_lkinit, op_lkinit),
XFER(xf_zattach, op_zattach),
XFER(xf_zedit, op_zedit),
XFER(xf_restartpc, op_restartpc),
XFER(xf_gvextnam, op_gvextnam_fast),
XFER(xf_fnzcall, op_fnzcall),
XFER(xf_fnview, op_fnview),
XFER(xf_zdeallocate, op_zdeallocate),
XFER(xf_indlvarg, opp_indlvarg),
XFER(xf_forchk1, op_forchk1),
XFER(xf_cvtparm, op_cvtparm),
XFER(xf_zprevious, op_zprevious),
XFER(xf_fnzprevious, op_fnzprevious),
XFER(xf_gvquery, op_gvquery),
XFER(xf_fnquery, op_fnquery),
XFER(xf_bindparm, op_bindparm),
XFER(xf_retarg, op_retarg),
XFER(xf_exfun, op_exfun),
XFER(xf_extexfun, op_extexfun),
XFER(xf_zhelp, op_zhelp),
XFER(xf_fnp1, op_fnp1),
XFER(xf_zg1, opp_zg1),
XFER(xf_newintrinsic, opp_newintrinsic),
XFER(xf_gvzwithdraw, op_gvzwithdraw),
XFER(xf_lvzwithdraw, op_lvzwithdraw),
XFER(xf_pattern, op_pattern),
XFER(xf_nullexp, op_nullexp),
XFER(xf_exfunret, op_exfunret),
XFER(xf_fnlvname, op_fnlvname),
XFER(xf_forlcldob, op_forlcldob),
XFER(xf_indrzshow, opp_indrzshow),
XFER(xf_gvzwrite, op_gvzwrite),
XFER(xf_zstep, op_zstep),
XFER(xf_mval2num, mval2num),
XFER(xf_lkname, op_lkname),
XFER(xf_fnztrnlnm, op_fnztrnlnm),
XFER(xf_ztcommit, op_ztcommit),
XFER(xf_ztstart, op_ztstart),
XFER(xf_equnul, op_equnul),
XFER(xf_fngetlki, op_fngetlki),
XFER(xf_fnzlkid, op_fnzlkid),
XFER(xf_indlvnamadr, opp_indlvnamadr),
XFER(xf_callspb, op_callspb),
XFER(xf_callspw, op_callspw),
XFER(xf_callspl, op_callspl),
XFER(xf_iocontrol, op_iocontrol),
XFER(xf_fnfgncal, op_fnfgncal),
XFER(xf_zcompile, op_zcompile),
XFER(xf_tcommit, opp_tcommit),
XFER(xf_trollback, opp_trollback),
XFER(xf_trestart, opp_trestart),
XFER(xf_tstart, opp_tstart),
XFER(xf_exp, op_exp),
XFER(xf_fnget2, op_fnget2),
XFER(xf_dummy, op_fnget2),	/* left to prevent massive slide - reuse at next opportunity */
XFER(xf_fnname, op_fnname),
XFER(xf_indfnname, opp_indfnname),
XFER(xf_fnlvprvname, op_fnlvprvname),
XFER(xf_gvo2, op_gvo2),
XFER(xf_fnlvnameo2, op_fnlvnameo2),
XFER(xf_fno2, op_fno2),
XFER(xf_indo2, op_indo2),
XFER(xf_get_msf, op_get_msf),
XFER(xf_dt_get, op_dt_get),
XFER(xf_dt_store, op_dt_store),
XFER(xf_dt_false, op_dt_false),
XFER(xf_dt_true, op_dt_true),
XFER(xf_fnzbitstr, op_fnzbitstr),
XFER(xf_fnzbitlen, op_fnzbitlen),
XFER(xf_fnzbitget, op_fnzbitget),
XFER(xf_fnzbitset, op_fnzbitset),
XFER(xf_fnzbitcoun, op_fnzbitcoun),
XFER(xf_fnzbitfind, op_fnzbitfind),
XFER(xf_fnzbitnot, op_fnzbitnot),
XFER(xf_fnzbitand, op_fnzbitand),
XFER(xf_fnzbitor, op_fnzbitor),
XFER(xf_fnzbitxor, op_fnzbitxor),
XFER(xf_fgnlookup, op_fgnlookup),
XFER(xf_sorts_after, op_sorts_after),
XFER(xf_fnzqgblmod, op_fnzqgblmod),
XFER(xf_fngvget1, op_fngvget1),
XFER(xf_fnget1, op_fnget1),
XFER(xf_setp1, op_setp1),
XFER(xf_setextract, op_setextract),
XFER(xf_inddevparms, opp_inddevparms),
XFER(xf_merge, op_merge),
XFER(xf_merge_arg, op_merge_arg),
XFER(xf_indmerge, opp_indmerge),
XFER(xf_m_srchindx, op_m_srchindx),
XFER(xf_fnstack1, op_fnstack1),
XFER(xf_fnstack2, op_fnstack2),
XFER(xf_fnqlength, op_fnqlength),
XFER(xf_fnqsubscript, op_fnqsubscript),
XFER(xf_fnreverse, op_fnreverse),
XFER(xf_psvput, opp_svput),
XFER(xf_fnzjobexam, op_fnzjobexam),
XFER(xf_fnzsigproc, op_fnzsigproc),
XFER(xf_fnincr, op_fnincr),
XFER(xf_gvincr, op_gvincr),
XFER(xf_indincr, opp_indincr),
XFER(xf_setzextract, op_setzextract),
XFER(xf_setzp1, op_setzp1),
XFER(xf_setzpiece, op_setzpiece),
XFER(xf_fnzascii, op_fnzascii),
XFER(xf_fnzchar, op_fnzchar),
XFER(xf_fnzextract, op_fnzextract),
XFER(xf_fnzfind, op_fnzfind),
XFER(xf_fnzj2, op_fnzj2),
XFER(xf_fnzlength, op_fnzlength),
XFER(xf_fnzpopulation, op_fnzpopulation),
XFER(xf_fnzpiece, op_fnzpiece),
XFER(xf_fnzp1, op_fnzp1),
XFER(xf_fnztranslate, op_fnztranslate),
#ifdef UNICODE_SUPPORTED
XFER(xf_fnzconvert2, op_fnzconvert2),
XFER(xf_fnzconvert3, op_fnzconvert3),
#endif
XFER(xf_fnzwidth, op_fnzwidth),
XFER(xf_fnzsubstr, op_fnzsubstr),
XFER(xf_setals2als, op_setals2als),
XFER(xf_setalsin2alsct, op_setalsin2alsct),
XFER(xf_setalsctin2als, op_setalsctin2als),
XFER(xf_setalsct2alsct, op_setalsct2alsct),
XFER(xf_killalias, op_killalias),
XFER(xf_killaliasall, op_killaliasall),
XFER(xf_fnzdata, op_fnzdata),
XFER(xf_clralsvars, op_clralsvars),
XFER(xf_fnzahandle, op_fnzahandle),
XFER(xf_fnztrigger, op_fnztrigger),
XFER(xf_exfunretals, op_exfunretals),
XFER(xf_setfnretin2als, op_setfnretin2als),
XFER(xf_setfnretin2alsct, op_setfnretin2alsct),
XFER(xf_zwritesvn, op_zwritesvn),
#ifdef UNIX
XFER(xf_ztrigger, op_ztrigger), /* Restrict to UNIX, not GTM_TRIGGER since is in ttt.txt which cannot be #ifdef'd out */
#endif
XFER(xf_zhalt, op_zhalt),
XFER(xf_fnzwrite, op_fnzwrite),
XFER(xf_igetdst, op_igetdst),
XFER(xf_indget1, op_indget1),
XFER(xf_glvnpop, op_glvnpop),
XFER(xf_glvnslot, op_glvnslot),
XFER(xf_indsavglvn, opp_indsavglvn),
XFER(xf_indsavlvn, opp_indsavlvn),
XFER(xf_rfrshlvn, op_rfrshlvn),
XFER(xf_savgvn, op_savgvn),
XFER(xf_savlvn, op_savlvn),
XFER(xf_shareslot, op_shareslot),
XFER(xf_stoglvn, op_stoglvn),
XFER(xf_rfrshgvn, op_rfrshgvn),
XFER(xf_indfnname2, op_indfnname2),
XFER(xf_indget2, op_indget2),
XFER(xf_indmerge2, op_indmerge2),
#ifdef UNIX
XFER(xf_fnzpeek, op_fnzpeek),
#endif
XFER(xf_litc, op_litc),
XFER(xf_stolitc, op_stolitc),
XFER(xf_fnzsocket, op_fnzsocket)
#ifdef UNIX
,
XFER(xf_fnzsyslog, op_fnzsyslog),
XFER(xf_zrupdate, op_zrupdate)
#endif
#ifdef USHBIN_SUPPORTED
,
XFER(xf_rhd_ext, op_rhd_ext),
XFER(xf_lab_ext, op_lab_ext)
#endif
