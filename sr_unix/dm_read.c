/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* WARNING: this module contains a mixture of ASCII and EBCDIC on S390*/
#include "mdef.h"

#include "gtm_string.h"
#include <errno.h>
#include <signal.h>
#include "gtm_unistd.h"
#include "gtm_stdlib.h"

#include "iotcp_select.h"

#include "io.h"
#include "trmdef.h"
#include "iottdef.h"
#include "iottdefsp.h"
#include "iott_edit.h"
#include "stringpool.h"
#include "comline.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "cli.h"
#include "outofband.h"
#include "dm_read.h"
#include "gtm_tputs.h"

GBLREF io_pair 		io_curr_device;
GBLREF io_pair		io_std_device;
GBLREF bool		prin_in_dev_failure;
GBLREF io_desc		*active_device;
GBLREF spdesc 		stringpool;
GBLREF int4		outofband;
GBLREF mstr		*comline_base;
GBLDEF int		recall_num;
GBLDEF int		comline_index;
GBLREF mstr		gtmprompt;
LITREF unsigned char	lower_to_upper_table[];

GBLREF int		AUTO_RIGHT_MARGIN, EAT_NEWLINE_GLITCH;
GBLREF char		*CURSOR_UP, *CURSOR_DOWN, *CURSOR_LEFT, *CURSOR_RIGHT, *CLR_EOL;
GBLREF char		*KEY_BACKSPACE, *KEY_DC;
GBLREF char		*KEY_DOWN, *KEY_LEFT, *KEY_RIGHT, *KEY_UP;
GBLREF char		*KEY_INSERT;
GBLREF char		*KEYPAD_LOCAL, *KEYPAD_XMIT;

static unsigned char	cr = '\r';


#define	REC			"rec"
#define	RECALL			"recall"

#define	MAX_ERR_MSG_LEN		40

enum	RECALL_ERR_CODE
{
	NO_ERROR,
	ERR_OUT_OF_BOUNDS,
	ERR_NO_MATCH_STR
};

static	unsigned char	recall_error_msg[][MAX_ERR_MSG_LEN] =
{
	"",
	"Recall Error : Number exceeds limit",
	"Recall Error : No matching string"
};

error_def(ERR_IOEOF);
#ifdef __MVS__
error_def(ERR_ASC2EBCDICCONV);
#endif

void	dm_read (mval *v)
{
	int		up, down, right, left;
	int		backspace, delete, insert_key, keypad_len;
	boolean_t	insert_mode;
	int		cl, index, msk_num, msk_in, selstat, status;
	uint4		mask;
	unsigned char	inchar, *temp;
#ifdef __MVS__
	unsigned char	ebcdic_inchar, ascii_inchar;
#endif
	unsigned short	i, j, length, i_max;
	unsigned short	loop_variable, num_chars_to_go_left, num_lines_to_go_up;
	unsigned char	escape_sequence[ESC_LEN];
	unsigned short	escape_length = 0;

	d_tt_struct 	*tt_ptr;
	fd_set		input_fd;
	io_desc 	*io_ptr;
	io_termmask	mask_term;

	struct timeval	input_timeval;


	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);

	if (comline_base == NULL)
	{
		comline_base = (mstr *)malloc(MAX_RECALL * sizeof(mstr));
		memset(comline_base, 0, (MAX_RECALL * sizeof(mstr)));
	}

	active_device = io_curr_device.in;
	io_ptr = io_curr_device.in;
	tt_ptr = (d_tt_struct *)(io_ptr->dev_sp);
	assert (io_ptr->state == dev_open);
	if (tt == io_curr_device.out->type)
		iott_flush(io_curr_device.out);

	/* -----------------------------
	 * for possible escape sequence
	 * -----------------------------
	 */

	length = tt_ptr->in_buf_sz + ESC_LEN;
	if (stringpool.free + length > stringpool.top)
		stp_gcol (length);

	i = i_max = 0;
	temp = stringpool.free;
	mask = tt_ptr->term_ctrl;
	mask_term = tt_ptr->mask_term;
	mask_term.mask[ESC / NUM_BITS_IN_INT4] &= ~(1 << ESC);
	insert_mode = !(TT_NOINSERT & tt_ptr->ext_cap);
	DOWRITE_A(tt_ptr->fildes, &cr, 1);
	DOWRITE_A(tt_ptr->fildes, gtmprompt.addr, gtmprompt.len);
	j = gtmprompt.len;
	index = 0;
	cl = clmod (comline_index - index);

	/* to turn keypad on if possible */
#ifndef __MVS__
	if (NULL != KEYPAD_XMIT && (keypad_len = strlen(KEYPAD_XMIT)))	/* embedded assignment */
		DOWRITE(tt_ptr->fildes, KEYPAD_XMIT, keypad_len);
#endif

	while (i_max < length)
	{
		if (outofband)
		{
			i = 0;
			outofband_action(FALSE);
			break;
		}

		FD_ZERO(&input_fd);
		FD_SET(tt_ptr->fildes, &input_fd);
		assert(FD_ISSET(tt_ptr->fildes, &input_fd) != 0);

		/* --------------------------------------------------------
		 * Arbitrarily-chosen timeout value to prevent consumption
		 * of resources in a tight loop when no input is available.
		 * --------------------------------------------------------
		 */

		input_timeval.tv_sec  = 100;
		input_timeval.tv_usec = 0;

		/* ------------------------------------------------------------------
		 * N.B.  On some Unix systems, the documentation for select() is
		 * ambiguous with respect to the first argument.  It specifies the
		 * number of contiguously-numbered file descriptors to be tested,
		 * starting with file descriptor zero.  Thus, it should be equal
		 * to the highest-numbered file descriptor to test plus one. (See
		 * _UNIX_Netowork_Programming_ by W. Richard Stevens, Section 6.13,
		 * pp. 328-331)
		 * -------------------------------------------------------------------
		 */

		SELECT(tt_ptr->fildes + 1, (void *)&input_fd, (void *)NULL, (void *)NULL,
			&input_timeval, selstat);
		if (0 > selstat)
			rts_error(VARLSTCNT(1) errno);
		else if (0 == selstat)
		{
			/* ---------------------------------------------------
			 * timeout but still not ready for reading, try again
			 * ---------------------------------------------------
			 */

			continue;
		}
		/* -----------------------------------
		 * selstat > 0; try reading something
		 * -----------------------------------
		 */
		else if ((status = DOREAD_A(tt_ptr->fildes, &inchar, 1)) < 0)
		{
			/* -------------------------
			 * Error return from read().
			 * -------------------------
			 */

			/*
			 * If error was EINTR, this
			 * code does not retry, so
			 * no EINTR wrapper macro for
			 * the read is necessary.
			 */

			if (errno != EINTR)
			{
				io_ptr->dollar.za = 9;
				rts_error(VARLSTCNT(1) errno);
			}
		}
		else if (status == 0)
		{
			/* ----------------------------------------------------
			 * select() says there's something to read, but read()
			 * found zero characters; assume connection dropped.
			 * ----------------------------------------------------
			 */

			if (io_curr_device.in == io_std_device.in)
			{
				if (!prin_in_dev_failure)
					prin_in_dev_failure = TRUE;
				else
					exit(errno);
			}
			rts_error(VARLSTCNT(1) ERR_IOEOF);
		}
		/* ------------------------------------------------------------
		 * select() says it's ready to read and read() found some data
		 * ------------------------------------------------------------
		 */
		else if (status > 0)
		{
			/* ------------------------
			 * set by call to select()
			 * ------------------------
			 */
			assert(FD_ISSET(tt_ptr->fildes, &input_fd) != 0);

			/* -------------------------------------------------------
			 * set prin_in_dev_failure to FALSE in case it was
			 * set to TRUE in the previous read which may have failed
			 * -------------------------------------------------------
			 */
			prin_in_dev_failure = FALSE;

			if (mask & TRM_CONVERT)
				inchar = lower_to_upper_table[inchar];

			if (j >= io_ptr->width && io_ptr->wrap && !(mask & TRM_NOECHO))
			{
				DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));
				j = 0;
			}
			msk_num = (uint4)inchar / NUM_BITS_IN_INT4;
			msk_in = (1 << ((uint4)inchar % NUM_BITS_IN_INT4));
			if (msk_in & mask_term.mask[msk_num])
			{
				/* carriage return has been typed */
				int		match_length;
				char		*argv[3];
				const char	delimiter_string[] = " \t";

				stringpool.free [i_max] = '\0';
					/* definitely you would not have exceeded the maximum length
					   allowed since otherwise you would have gone out of the while loop */

				match_length = strcspn((const char *)stringpool.free, delimiter_string);

				/* only "rec" and "recall" should be accepted */

				if (   ((match_length == strlen(REC)) || (match_length == strlen(RECALL)))
				    && strncmp((const char *)stringpool.free, RECALL, match_length) == 0)
				{
					enum RECALL_ERR_CODE	err_recall = NO_ERROR;

					strtok((char *)stringpool.free, delimiter_string);
					argv[1] = strtok(NULL, "");

					index = 0;
					DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));
					DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));

					if (argv[1] == NULL)
					{
						/* print out all the history strings */

						int	m;

						for (m = recall_num;  m > 0;  m--)
						{
							char	temp_str[MAX_RECALL_NUMBER_LENGTH + 1];
							int	n, cur_value, cur_cl;

							cur_value = recall_num + 1 - m;
							temp_str [MAX_RECALL_NUMBER_LENGTH] = ' ';

							for (n = MAX_RECALL_NUMBER_LENGTH - 1;  n >= 0;  n--)
							{
								temp_str[n] = '0' + cur_value % 10;
								cur_value = cur_value / 10;

								if (temp_str[n] == '0'  &&  cur_value == 0)
									temp_str[n] = ' ';
							}

							cur_cl = clmod(comline_index - m);
							DOWRITE_A(tt_ptr->fildes, temp_str, sizeof(temp_str));
							write_str((unsigned char *)comline_base[cur_cl].addr,
								comline_base[cur_cl].len, sizeof(temp_str), TRUE);
							DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));
						}

						DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));
						DOWRITE_A(tt_ptr->fildes, gtmprompt.addr, gtmprompt.len);
						i = i_max = 0;
						temp = stringpool.free;
						j = gtmprompt.len;
					}
					else
					{
						bool	decimal;
						int	recall_index = -1;

						decimal = cli_is_dcm(argv[1]);
									/* checks for a positive decimal number */

						if (!decimal)
						{
							int	 m, len = strlen(argv[1]);

							for (m = 1;  m <= recall_num;  m++)
							{
								if (strncmp(comline_base[clmod(comline_index - m)].addr,
									    argv[1],
									    len) == 0)
								{
									recall_index = clmod(comline_index - m);
									break;
								}
							}

							if (recall_index == -1)
							{
								/* no matching string found */
								err_recall = ERR_NO_MATCH_STR;
							}
						}
						else
						{
							if (ATOI(argv[1]) > recall_num)
							{
								/* out of bounds error */
								err_recall = ERR_OUT_OF_BOUNDS;
							}
							else
							{
								recall_index =
									clmod(comline_index + ATOI(argv[1]) - recall_num - 1);
							}
						}

						if (recall_index != -1)
						{
							DOWRITE_A(tt_ptr->fildes, gtmprompt.addr, gtmprompt.len);
							write_str((unsigned char *)comline_base[recall_index].addr,
								comline_base[recall_index].len, gtmprompt.len, TRUE);

							/* using memcpy since areas definitely dont overlap. */
							memcpy(stringpool.free, comline_base[recall_index].addr,
										comline_base[recall_index].len);
							temp = stringpool.free + comline_base[recall_index].len;
							i = i_max = comline_base[recall_index].len;
							j = (gtmprompt.len + comline_base[recall_index].len) % io_ptr->width;
						}
					}

					if (err_recall != NO_ERROR)
					{
						DOWRITE_A(tt_ptr->fildes, recall_error_msg[err_recall],
							strlen((char *)recall_error_msg[err_recall]));
						DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));
						DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));
						DOWRITE_A(tt_ptr->fildes, gtmprompt.addr, gtmprompt.len);
						i = i_max = 0;
						temp = stringpool.free;
						j = gtmprompt.len;
					}

					continue;
				}
				else
				{
					break;
				}
			}

#ifdef __MVS__
			ebcdic_inchar =ascii_inchar = inchar;
			if ( -1 == __atoe_l((char *)&ebcdic_inchar, 1) )
				rts_error(VARLSTCNT(4) ERR_ASC2EBCDICCONV, 2, LEN_AND_LIT("__atoe_l"));
			inchar = ebcdic_inchar;
#endif

			if (   (   (int)inchar == tt_ptr->ttio_struct->c_cc[VERASE]
				|| ('\0' == KEY_BACKSPACE[1]  &&  inchar == KEY_BACKSPACE[0]))
			    &&  !(mask & TRM_PASTHRU))
			{
				if (i > 0)
				{
					move_cursor_left(j);
					j = (j - 1 + io_ptr->width) % io_ptr->width;
					stringpool.free [i_max] = ' ';
					write_str(temp, i_max - i + 1, j, FALSE);
					temp --;
					i--;
					i_max--;
					memmove(temp, temp + 1, i_max - i);
				}
			}
			else if (NATIVE_ESC == inchar)
			{
				escape_sequence[escape_length++] = inchar;
				io_ptr->esc_state = START;
				iott_escape(&inchar, &inchar + 1, io_ptr);
			}
			else if (escape_length != 0)
			{
				escape_sequence[escape_length++] = inchar;
				iott_escape(&inchar, &inchar + 1, io_ptr);
			}
			else
			{
#ifdef __MVS__
				inchar = ascii_inchar;
#endif
				switch (inchar)
				{
					case EDIT_SOL:	/* ctrl A  start of line */
					{
						int	num_lines_above;
						int	num_chars_left;

						num_lines_above = (i + gtmprompt.len) /
										io_ptr->width;
						num_chars_left = j - gtmprompt.len;
						move_cursor(tt_ptr->fildes, num_lines_above, num_chars_left);
						i = 0;
						temp = stringpool.free;
						j = gtmprompt.len;
						break;
					}
					case EDIT_EOL:	/* ctrl E  end of line */
					{
						int	num_lines_above;
						int	num_chars_left;

						num_lines_above =
							(i + gtmprompt.len) / io_ptr->width -
							(i_max + gtmprompt.len) / io_ptr->width;
						num_chars_left = j - (i_max + gtmprompt.len) % io_ptr->width;
						move_cursor(tt_ptr->fildes, num_lines_above, num_chars_left);
						i = i_max;
						temp = stringpool.free + i_max;
						j = (i_max + gtmprompt.len) % io_ptr->width;
						break;
					}
					case EDIT_LEFT:	/* ctrl B  left one */
					{
						if (i != 0)
						{
							move_cursor_left(j);
							temp--;
							i--;
							j = (j - 1 + io_ptr->width) % io_ptr->width;
						}
						break;
					}
					case EDIT_RIGHT:	/* ctrl F  right one */
					{
						if (i != i_max)
						{
							move_cursor_right(j);
							temp++;
							i++;
							j = (j + 1) % io_ptr->width;
						}
						break;
					}
					case EDIT_DEOL:	/* ctrl K  delete to end of line */
					{
						memset(temp, ' ', i_max - i);
						write_str(temp, i_max - i, j, FALSE);
						i_max = i;
						break;
					}
					case EDIT_ERASE:	/* ctrl U  delete whole line */
					{
						int	num_lines_above;
						int	num_chars_left;

						num_lines_above = (i + gtmprompt.len) /
										io_ptr->width;
						num_chars_left = j - gtmprompt.len;
						move_cursor(tt_ptr->fildes, num_lines_above, num_chars_left);
						memset(stringpool.free, ' ', i_max);
						write_str(stringpool.free, i_max, gtmprompt.len, FALSE);
						i = 0;
						i_max = 0;
						j = gtmprompt.len;
						temp = stringpool.free;
						break;
					}
					case EDIT_DELETE:	/* ctrl D  delete char */
					{
						if (i != i_max)
						{
							stringpool.free [i_max] = ' ';
							write_str(temp + 1, i_max - i, j, FALSE);
							memmove(temp, temp + 1, i_max - i);
							i_max--;
						}
						break;
					}
					default:
					{
						if (i == i_max)
						{	/* at end of input */
							*temp = inchar;
							write_str(temp, i_max - i + 1, j, TRUE);
						}
						else
						{
							if (insert_mode)
								memmove(temp + 1, temp, i_max - i);
							*temp = inchar;
							write_str(temp, i_max - i + (insert_mode ? 1 : 0), j, FALSE);
							move_cursor_right(j);
						}
						temp++;
						if (insert_mode || i == i_max)
							i_max++;
						i++;
						j = (j + 1) % io_ptr->width;
						break;
					}
				}
			}
		}

		if (escape_length != 0  &&  io_ptr->esc_state >= FINI)
		{
			down = strncmp((const char *)escape_sequence, KEY_DOWN, escape_length);
			up = strncmp((const char *)escape_sequence, KEY_UP, escape_length);
			right = strncmp((const char *)escape_sequence, KEY_RIGHT, escape_length);
			left = strncmp((const char *)escape_sequence, KEY_LEFT, escape_length);
			backspace = delete = insert_key = -1;

			if (KEY_BACKSPACE != NULL)
				backspace = strncmp((const char *)escape_sequence, KEY_BACKSPACE, escape_length);
			if (KEY_DC != NULL)
				delete = strncmp((const char *)escape_sequence, KEY_DC, escape_length);
			if (KEY_INSERT != NULL && '\0' != KEY_INSERT[0])
				insert_key = strncmp((const char *)escape_sequence, KEY_INSERT, escape_length);

			memset(escape_sequence, '\0', escape_length);
			escape_length = 0;

			if (io_ptr->esc_state == BADESC)
			{
				io_ptr->esc_state = START;
				break;
			}

			if (backspace == 0  ||  delete == 0)
			{
				if (i > 0)
				{
					move_cursor_left(j);
					j = (j - 1 + io_ptr->width) % io_ptr->width;
					stringpool.free[i_max] = ' ';
					write_str(temp, i_max - i + 1, j, FALSE);
					temp --;
					i--;
					i_max--;
					memmove(temp, temp + 1, i_max - i);
				}
			}

			if (0 == insert_key)
				insert_mode = !insert_mode;	/* toggle */
			else if (up == 0  ||  down == 0)
			{
				DOWRITE_A(tt_ptr->fildes, &cr, 1);
				DOWRITE_A(tt_ptr->fildes, gtmprompt.addr, gtmprompt.len);
				gtm_tputs(CLR_EOL, 1, outc);
				temp = stringpool.free; i = 0; i_max = 0; j = gtmprompt.len;
				if (up == 0)
				{
					if ((MAX_RECALL + 1 != index  &&  ((*(comline_base + cl)).len != 0)  ||  index == 0))
						index++;
				}
				else
				{
					assert (down == 0);
					if (index != 0)
						 --index;
				}
				if (0 < index  &&  index <= MAX_RECALL)
				{
					cl = clmod (comline_index - index);
					i = i_max = comline_base[cl].len;
					j = (unsigned)(i + gtmprompt.len) % io_ptr->width;
					if (i != 0)
					{
						memcpy(temp, comline_base[cl].addr, comline_base[cl].len);
						temp += i;
						write_str((unsigned char *)comline_base[cl].addr, i, gtmprompt.len, TRUE);
					}
				}
			}
			else if (   !(mask & TRM_NOECHO)
				 && !(right == 0  &&  i == i_max)
				 && !(left == 0   &&  i == 0))
			{
				if (right == 0)
				{
					move_cursor_right(j);
					temp++;
					i++;
					j = (j + 1) % io_ptr->width;
				}
				if (left == 0)
				{
					move_cursor_left(j);
					temp--;
					i--;
					j = (j - 1 + io_ptr->width) % io_ptr->width;
				}
			}
			io_ptr->esc_state = START;
		}
	}

	/* turn keypad off */
#ifndef __MVS__
	if (NULL != KEYPAD_LOCAL && (keypad_len = strlen(KEYPAD_LOCAL)))	/* embedded assignment */
		DOWRITE(tt_ptr->fildes, KEYPAD_LOCAL, keypad_len);
#endif

	if (i_max == length)
		i_max = length - 1;

	io_ptr->dollar.za = 0;
	v->mvtype = MV_STR;
	v->str.len = i_max;
	v->str.addr = (char *)stringpool.free;

	if (i_max != 0)
	{
		cl = clmod (comline_index - 1);
		if (i_max != comline_base[cl].len  ||  memcmp(comline_base[cl].addr, stringpool.free, i_max))
		{
			comline_base[comline_index] = v->str;
			comline_index = clmod (comline_index + 1);
			if (recall_num != MAX_RECALL)
				recall_num ++;
		}
		stringpool.free += i_max;
	}

	if (!(mask & TRM_NOECHO))
	{
		if ((io_ptr->dollar.x += v->str.len) >= io_ptr->width && io_ptr->wrap)
		{
			io_ptr->dollar.y += (io_ptr->dollar.x / io_ptr->width);
			if (io_ptr->length != 0)
				io_ptr->dollar.y %= io_ptr->length;
			io_ptr->dollar.x %= io_ptr->width;
			if (io_ptr->dollar.x == 0)
				DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));
		}
	}

	active_device = 0;
	return;
}
