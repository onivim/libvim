/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

/*
 * buffer.c: functions for dealing with the buffer structure
 */

/*
 * The buffer list is a double linked list of all buffers.
 * Each buffer can be in one of these states:
 * never loaded: BF_NEVERLOADED is set, only the file name is valid
 *   not loaded: b_ml.ml_mfp == NULL, no memfile allocated
 *	 hidden: b_nwindows == 0, loaded but not displayed in a window
 *	 normal: loaded and displayed in a window
 *
 * Instead of storing file names all over the place, each file name is
 * stored in the buffer list. It can be referenced by a number.
 *
 * The current implementation remembers all file names ever used.
 */

#include "vim.h"

static char_u *buflist_match(regmatch_T *rmp, buf_T *buf, int ignore_case);
static char_u *fname_match(regmatch_T *rmp, char_u *name, int ignore_case);
static void buflist_setfpos(buf_T *buf, win_T *win, linenr_T lnum, colnr_T col, int copy_options);
#ifdef UNIX
static buf_T *buflist_findname_stat(char_u *ffname, stat_T *st);
static int otherfile_buf(buf_T *buf, char_u *ffname, stat_T *stp);
static int buf_same_ino(buf_T *buf, stat_T *stp);
#else
static int otherfile_buf(buf_T *buf, char_u *ffname);
#endif
static int append_arg_number(win_T *wp, char_u *buf, int buflen, int add_file);
static void free_buffer(buf_T *);
static void free_buffer_stuff(buf_T *buf, int free_options);
static void clear_wininfo(buf_T *buf);

#ifdef UNIX
#define dev_T dev_t
#else
#define dev_T unsigned
#endif

#if defined(FEAT_QUICKFIX)
static char *msg_loclist = N_("[Location List]");
static char *msg_qflist = N_("[Quickfix List]");
#endif
static char *e_auabort = N_("E855: Autocommands caused command to abort");

/* Number of times free_buffer() was called. */
static int buf_free_count = 0;

/* Read data from buffer for retrying. */
static int
read_buffer(
    int read_stdin, /* read file from stdin, otherwise fifo */
    exarg_T *eap,   /* for forced 'ff' and 'fenc' or NULL */
    int flags)      /* extra flags for readfile() */
{
  int retval = OK;
  linenr_T line_count;

  /*
     * Read from the buffer which the text is already filled in and append at
     * the end.  This makes it possible to retry when 'fileformat' or
     * 'fileencoding' was guessed wrong.
     */
  line_count = curbuf->b_ml.ml_line_count;
  retval = readfile(
      read_stdin ? NULL : curbuf->b_ffname,
      read_stdin ? NULL : curbuf->b_fname,
      (linenr_T)line_count, (linenr_T)0, (linenr_T)MAXLNUM, eap,
      flags | READ_BUFFER);
  if (retval == OK)
  {
    /* Delete the binary lines. */
    while (--line_count >= 0)
      ml_delete((linenr_T)1, FALSE);
  }
  else
  {
    /* Delete the converted lines. */
    while (curbuf->b_ml.ml_line_count > line_count)
      ml_delete(line_count, FALSE);
  }
  /* Put the cursor on the first line. */
  curwin->w_cursor.lnum = 1;
  curwin->w_cursor.col = 0;

  if (read_stdin)
  {
    /* Set or reset 'modified' before executing autocommands, so that
	 * it can be changed there. */
    if (!readonlymode && !BUFEMPTY())
      changed();
    else if (retval == OK)
      unchanged(curbuf, FALSE);

    if (retval == OK)
    {
#ifdef FEAT_EVAL
      apply_autocmds_retval(EVENT_STDINREADPOST, NULL, NULL, FALSE,
                            curbuf, &retval);
#else
      apply_autocmds(EVENT_STDINREADPOST, NULL, NULL, FALSE, curbuf);
#endif
    }
  }
  return retval;
}

/*
 * Open current buffer, that is: open the memfile and read the file into
 * memory.
 * Return FAIL for failure, OK otherwise.
 */
int open_buffer(
    int read_stdin, /* read file from stdin */
    exarg_T *eap,   /* for forced 'ff' and 'fenc' or NULL */
    int flags)      /* extra flags for readfile() */
{
  int retval = OK;
  bufref_T old_curbuf;
  int read_fifo = FALSE;

  /*
     * The 'readonly' flag is only set when BF_NEVERLOADED is being reset.
     * When re-entering the same buffer, it should not change, because the
     * user may have reset the flag by hand.
     */
  if (readonlymode && curbuf->b_ffname != NULL && (curbuf->b_flags & BF_NEVERLOADED))
    curbuf->b_p_ro = TRUE;

  if (ml_open(curbuf) == FAIL)
  {
    /*
	 * There MUST be a memfile, otherwise we can't do anything
	 * If we can't create one for the current buffer, take another buffer
	 */
    close_buffer(NULL, curbuf, 0, FALSE);
    FOR_ALL_BUFFERS(curbuf)
    if (curbuf->b_ml.ml_mfp != NULL)
      break;
    /*
	 * if there is no memfile at all, exit
	 * This is OK, since there are no changes to lose.
	 */
    if (curbuf == NULL)
    {
      emsg(_("E82: Cannot allocate any buffer, exiting..."));
      getout(2);
    }
    emsg(_("E83: Cannot allocate buffer, using other one..."));
    enter_buffer(curbuf);
    return FAIL;
  }

  /* The autocommands in readfile() may change the buffer, but only AFTER
     * reading the file. */
  set_bufref(&old_curbuf, curbuf);
  modified_was_set = FALSE;

  /* mark cursor position as being invalid */
  curwin->w_valid = 0;

  if (curbuf->b_ffname != NULL)
  {
    int old_msg_silent = msg_silent;
#ifdef UNIX
    int save_bin = curbuf->b_p_bin;
    int perm;
#endif
#ifdef UNIX
    perm = mch_getperm(curbuf->b_ffname);
    if (perm >= 0 && (S_ISFIFO(perm) || S_ISSOCK(perm)
#ifdef OPEN_CHR_FILES
                      || (S_ISCHR(perm) && is_dev_fd_file(curbuf->b_ffname))
#endif
                          ))
      read_fifo = TRUE;
    if (read_fifo)
      curbuf->b_p_bin = TRUE;
#endif
    if (shortmess(SHM_FILEINFO))
      msg_silent = 1;
    retval = readfile(curbuf->b_ffname, curbuf->b_fname,
                      (linenr_T)0, (linenr_T)0, (linenr_T)MAXLNUM, eap,
                      flags | READ_NEW | (read_fifo ? READ_FIFO : 0));
#ifdef UNIX
    if (read_fifo)
    {
      curbuf->b_p_bin = save_bin;
      if (retval == OK)
        retval = read_buffer(FALSE, eap, flags);
    }
#endif
    msg_silent = old_msg_silent;
    /* Help buffer is filtered. */
    if (bt_help(curbuf))
      fix_help_buffer();
  }
  else if (read_stdin)
  {
    int save_bin = curbuf->b_p_bin;

    /*
	 * First read the text in binary mode into the buffer.
	 * Then read from that same buffer and append at the end.  This makes
	 * it possible to retry when 'fileformat' or 'fileencoding' was
	 * guessed wrong.
	 */
    curbuf->b_p_bin = TRUE;
    retval = readfile(NULL, NULL, (linenr_T)0,
                      (linenr_T)0, (linenr_T)MAXLNUM, NULL,
                      flags | (READ_NEW + READ_STDIN));
    curbuf->b_p_bin = save_bin;
    if (retval == OK)
      retval = read_buffer(TRUE, eap, flags);
  }
  /* if first time loading this buffer, init b_chartab[] */
  if (curbuf->b_flags & BF_NEVERLOADED)
  {

    (void)buf_init_chartab(curbuf, FALSE);
  }

  /*
     * Set/reset the Changed flag first, autocmds may change the buffer.
     * Apply the automatic commands, before processing the modelines.
     * So the modelines have priority over autocommands.
     */
  /* When reading stdin, the buffer contents always needs writing, so set
     * the changed flag.  Unless in readonly mode: "ls | gview -".
     * When interrupted and 'cpoptions' contains 'i' set changed flag. */
  if ((got_int && vim_strchr(p_cpo, CPO_INTMOD) != NULL) || modified_was_set /* ":set modified" used in autocmd */
#ifdef FEAT_EVAL
      || (aborting() && vim_strchr(p_cpo, CPO_INTMOD) != NULL)
#endif
  )
    changed();
  else if (retval == OK && !read_stdin && !read_fifo)
    unchanged(curbuf, FALSE);
  save_file_ff(curbuf); /* keep this fileformat */

  /* Set last_changedtick to avoid triggering a TextChanged autocommand right
     * after it was added. */
  curbuf->b_last_changedtick = CHANGEDTICK(curbuf);
  /* require "!" to overwrite the file, because it wasn't read completely */
#ifdef FEAT_EVAL
  if (aborting())
#else
  if (got_int)
#endif
    curbuf->b_flags |= BF_READERR;

#ifdef FEAT_FOLDING
  /* Need to update automatic folding.  Do this before the autocommands,
     * they may use the fold info. */
  foldUpdateAll(curwin);
#endif

  /* need to set w_topline, unless some autocommand already did that. */
  if (!(curwin->w_valid & VALID_TOPLINE))
  {
    curwin->w_topline = 1;
#ifdef FEAT_DIFF
    curwin->w_topfill = 0;
#endif
  }
#ifdef FEAT_EVAL
  apply_autocmds_retval(EVENT_BUFENTER, NULL, NULL, FALSE, curbuf, &retval);
#else
  apply_autocmds(EVENT_BUFENTER, NULL, NULL, FALSE, curbuf);
#endif

  if (retval == OK)
  {
    /*
	 * The autocommands may have changed the current buffer.  Apply the
	 * modelines to the correct buffer, if it still exists and is loaded.
	 */
    if (bufref_valid(&old_curbuf) && old_curbuf.br_buf->b_ml.ml_mfp != NULL)
    {
      aco_save_T aco;

      /* Go to the buffer that was opened. */
      aucmd_prepbuf(&aco, old_curbuf.br_buf);
      do_modelines(0);
      curbuf->b_flags &= ~(BF_CHECK_RO | BF_NEVERLOADED);

#ifdef FEAT_EVAL
      apply_autocmds_retval(EVENT_BUFWINENTER, NULL, NULL, FALSE, curbuf,
                            &retval);
#else
      apply_autocmds(EVENT_BUFWINENTER, NULL, NULL, FALSE, curbuf);
#endif

      /* restore curwin/curbuf and a few other things */
      aucmd_restbuf(&aco);
    }
  }

  return retval;
}

/*
 * Store "buf" in "bufref" and set the free count.
 */
void set_bufref(bufref_T *bufref, buf_T *buf)
{
  bufref->br_buf = buf;
  bufref->br_fnum = buf == NULL ? 0 : buf->b_fnum;
  bufref->br_buf_free_count = buf_free_count;
}

/*
 * Return TRUE if "bufref->br_buf" points to the same buffer as when
 * set_bufref() was called and it is a valid buffer.
 * Only goes through the buffer list if buf_free_count changed.
 * Also checks if b_fnum is still the same, a :bwipe followed by :new might get
 * the same allocated memory, but it's a different buffer.
 */
int bufref_valid(bufref_T *bufref)
{
  return bufref->br_buf_free_count == buf_free_count
             ? TRUE
             : buf_valid(bufref->br_buf) && bufref->br_fnum == bufref->br_buf->b_fnum;
}

/*
 * Return TRUE if "buf" points to a valid buffer (in the buffer list).
 * This can be slow if there are many buffers, prefer using bufref_valid().
 */
int buf_valid(buf_T *buf)
{
  buf_T *bp;

  /* Assume that we more often have a recent buffer, start with the last
     * one. */
  for (bp = lastbuf; bp != NULL; bp = bp->b_prev)
    if (bp == buf)
      return TRUE;
  return FALSE;
}

/*
 * A hash table used to quickly lookup a buffer by its number.
 */
static hashtab_T buf_hashtab;

static void
buf_hashtab_add(buf_T *buf)
{
  sprintf((char *)buf->b_key, "%x", buf->b_fnum);
  if (hash_add(&buf_hashtab, buf->b_key) == FAIL)
    emsg(_("E931: Buffer cannot be registered"));
}

static void
buf_hashtab_remove(buf_T *buf)
{
  hashitem_T *hi = hash_find(&buf_hashtab, buf->b_key);

  if (!HASHITEM_EMPTY(hi))
    hash_remove(&buf_hashtab, hi);
}

/*
 * Return TRUE when buffer "buf" can be unloaded.
 * Give an error message and return FALSE when the buffer is locked or the
 * screen is being redrawn and the buffer is in a window.
 */
static int
can_unload_buffer(buf_T *buf)
{
  int can_unload = !buf->b_locked;

  if (can_unload && updating_screen)
  {
    win_T *wp;

    FOR_ALL_WINDOWS(wp)
    if (wp->w_buffer == buf)
    {
      can_unload = FALSE;
      break;
    }
  }
  if (!can_unload)
    emsg(_("E937: Attempt to delete a buffer that is in use"));
  return can_unload;
}

/*
 * Close the link to a buffer.
 * "action" is used when there is no longer a window for the buffer.
 * It can be:
 * 0			buffer becomes hidden
 * DOBUF_UNLOAD		buffer is unloaded
 * DOBUF_DELETE		buffer is unloaded and removed from buffer list
 * DOBUF_WIPE		buffer is unloaded and really deleted
 * When doing all but the first one on the current buffer, the caller should
 * get a new buffer very soon!
 *
 * The 'bufhidden' option can force freeing and deleting.
 *
 * When "abort_if_last" is TRUE then do not close the buffer if autocommands
 * cause there to be only one window with this buffer.  e.g. when ":quit" is
 * supposed to close the window but autocommands close all other windows.
 */
void close_buffer(
    win_T *win, /* if not NULL, set b_last_cursor */
    buf_T *buf,
    int action,
    int abort_if_last)
{
  int is_curbuf;
  int nwindows;
  bufref_T bufref;
  int is_curwin = (curwin != NULL && curwin->w_buffer == buf);
  win_T *the_curwin = curwin;
  tabpage_T *the_curtab = curtab;
  int unload_buf = (action != 0);
  int del_buf = (action == DOBUF_DEL || action == DOBUF_WIPE);
  int wipe_buf = (action == DOBUF_WIPE);

  /*
     * Force unloading or deleting when 'bufhidden' says so.
     * The caller must take care of NOT deleting/freeing when 'bufhidden' is
     * "hide" (otherwise we could never free or delete a buffer).
     */
  if (buf->b_p_bh[0] == 'd') /* 'bufhidden' == "delete" */
  {
    del_buf = TRUE;
    unload_buf = TRUE;
  }
  else if (buf->b_p_bh[0] == 'w') /* 'bufhidden' == "wipe" */
  {
    del_buf = TRUE;
    unload_buf = TRUE;
    wipe_buf = TRUE;
  }
  else if (buf->b_p_bh[0] == 'u') /* 'bufhidden' == "unload" */
    unload_buf = TRUE;

#ifdef FEAT_TERMINAL
  if (bt_terminal(buf) && (buf->b_nwindows == 1 || del_buf))
  {
    if (term_job_running(buf->b_term))
    {
      if (wipe_buf || unload_buf)
      {
        if (!can_unload_buffer(buf))
          return;

        /* Wiping out or unloading a terminal buffer kills the job. */
        free_terminal(buf);
      }
      else
      {
        /* The job keeps running, hide the buffer. */
        del_buf = FALSE;
        unload_buf = FALSE;
      }
    }
    else
    {
      /* A terminal buffer is wiped out if the job has finished. */
      del_buf = TRUE;
      unload_buf = TRUE;
      wipe_buf = TRUE;
    }
  }
#endif

  /* Disallow deleting the buffer when it is locked (already being closed or
     * halfway a command that relies on it). Unloading is allowed. */
  if ((del_buf || wipe_buf) && !can_unload_buffer(buf))
    return;

  /* check no autocommands closed the window */
  if (win != NULL && win_valid_any_tab(win))
  {
    /* Set b_last_cursor when closing the last window for the buffer.
	 * Remember the last cursor position and window options of the buffer.
	 * This used to be only for the current window, but then options like
	 * 'foldmethod' may be lost with a ":only" command. */
    if (buf->b_nwindows == 1)
      set_last_cursor(win);
    buflist_setfpos(buf, win,
                    win->w_cursor.lnum == 1 ? 0 : win->w_cursor.lnum,
                    win->w_cursor.col, TRUE);
  }

  set_bufref(&bufref, buf);

  /* When the buffer is no longer in a window, trigger BufWinLeave */
  if (buf->b_nwindows == 1)
  {
    ++buf->b_locked;
    if (apply_autocmds(EVENT_BUFWINLEAVE, buf->b_fname, buf->b_fname,
                       FALSE, buf) &&
        !bufref_valid(&bufref))
    {
      /* Autocommands deleted the buffer. */
    aucmd_abort:
      emsg(_(e_auabort));
      return;
    }
    --buf->b_locked;
    if (abort_if_last && one_window())
      /* Autocommands made this the only window. */
      goto aucmd_abort;

    /* When the buffer becomes hidden, but is not unloaded, trigger
	 * BufHidden */
    if (!unload_buf)
    {
      ++buf->b_locked;
      if (apply_autocmds(EVENT_BUFHIDDEN, buf->b_fname, buf->b_fname,
                         FALSE, buf) &&
          !bufref_valid(&bufref))
        /* Autocommands deleted the buffer. */
        goto aucmd_abort;
      --buf->b_locked;
      if (abort_if_last && one_window())
        /* Autocommands made this the only window. */
        goto aucmd_abort;
    }
#ifdef FEAT_EVAL
    if (aborting()) /* autocmds may abort script processing */
      return;
#endif
  }

  /* If the buffer was in curwin and the window has changed, go back to that
     * window, if it still exists.  This avoids that ":edit x" triggering a
     * "tabnext" BufUnload autocmd leaves a window behind without a buffer. */
  if (is_curwin && curwin != the_curwin && win_valid_any_tab(the_curwin))
  {
    block_autocmds();
    goto_tabpage_win(the_curtab, the_curwin);
    unblock_autocmds();
  }

  nwindows = buf->b_nwindows;

  /* decrease the link count from windows (unless not in any window) */
  if (buf->b_nwindows > 0)
    --buf->b_nwindows;

#ifdef FEAT_DIFF
  if (diffopt_hiddenoff() && !unload_buf && buf->b_nwindows == 0)
    diff_buf_delete(buf); /* Clear 'diff' for hidden buffer. */
#endif

  /* Return when a window is displaying the buffer or when it's not
     * unloaded. */
  if (buf->b_nwindows > 0 || !unload_buf)
    return;

  /* Always remove the buffer when there is no file name. */
  if (buf->b_ffname == NULL)
    del_buf = TRUE;

  /* When closing the current buffer stop Visual mode before freeing
     * anything. */
  if (buf == curbuf && VIsual_active
#if defined(EXITFREE)
      && !entered_free_all_mem
#endif
  )
    end_visual_mode();

  /*
     * Free all things allocated for this buffer.
     * Also calls the "BufDelete" autocommands when del_buf is TRUE.
     */
  /* Remember if we are closing the current buffer.  Restore the number of
     * windows, so that autocommands in buf_freeall() don't get confused. */
  is_curbuf = (buf == curbuf);
  buf->b_nwindows = nwindows;

  buf_freeall(buf, (del_buf ? BFA_DEL : 0) + (wipe_buf ? BFA_WIPE : 0));

  /* Autocommands may have deleted the buffer. */
  if (!bufref_valid(&bufref))
    return;
#ifdef FEAT_EVAL
  if (aborting()) /* autocmds may abort script processing */
    return;
#endif

  /*
     * It's possible that autocommands change curbuf to the one being deleted.
     * This might cause the previous curbuf to be deleted unexpectedly.  But
     * in some cases it's OK to delete the curbuf, because a new one is
     * obtained anyway.  Therefore only return if curbuf changed to the
     * deleted buffer.
     */
  if (buf == curbuf && !is_curbuf)
    return;

  if (win_valid_any_tab(win) && win->w_buffer == buf)
    win->w_buffer = NULL; /* make sure we don't use the buffer now */

  /* Autocommands may have opened or closed windows for this buffer.
     * Decrement the count for the close we do here. */
  if (buf->b_nwindows > 0)
    --buf->b_nwindows;

  /*
     * Remove the buffer from the list.
     */
  if (wipe_buf)
  {
    if (buf->b_sfname != buf->b_ffname)
      VIM_CLEAR(buf->b_sfname);
    else
      buf->b_sfname = NULL;
    VIM_CLEAR(buf->b_ffname);
    if (buf->b_prev == NULL)
      firstbuf = buf->b_next;
    else
      buf->b_prev->b_next = buf->b_next;
    if (buf->b_next == NULL)
      lastbuf = buf->b_prev;
    else
      buf->b_next->b_prev = buf->b_prev;
    free_buffer(buf);
  }
  else
  {
    if (del_buf)
    {
      /* Free all internal variables and reset option values, to make
	     * ":bdel" compatible with Vim 5.7. */
      free_buffer_stuff(buf, TRUE);

      /* Make it look like a new buffer. */
      buf->b_flags = BF_CHECK_RO | BF_NEVERLOADED;

      /* Init the options when loaded again. */
      buf->b_p_initialized = FALSE;
    }
    buf_clear_file(buf);
    if (del_buf)
      buf->b_p_bl = FALSE;
  }
}

/*
 * Make buffer not contain a file.
 */
void buf_clear_file(buf_T *buf)
{
  buf->b_ml.ml_line_count = 1;
  unchanged(buf, TRUE);
  buf->b_shortname = FALSE;
  buf->b_p_eol = TRUE;
  buf->b_start_eol = TRUE;
  buf->b_p_bomb = FALSE;
  buf->b_start_bomb = FALSE;
  buf->b_ml.ml_mfp = NULL;
  buf->b_ml.ml_flags = ML_EMPTY; /* empty buffer */
}

/*
 * buf_freeall() - free all things allocated for a buffer that are related to
 * the file.  Careful: get here with "curwin" NULL when exiting.
 * flags:
 * BFA_DEL	  buffer is going to be deleted
 * BFA_WIPE	  buffer is going to be wiped out
 * BFA_KEEP_UNDO  do not free undo information
 */
void buf_freeall(buf_T *buf, int flags)
{
  int is_curbuf = (buf == curbuf);
  bufref_T bufref;
  int is_curwin = (curwin != NULL && curwin->w_buffer == buf);
  win_T *the_curwin = curwin;
  tabpage_T *the_curtab = curtab;

  /* Make sure the buffer isn't closed by autocommands. */
  ++buf->b_locked;
  set_bufref(&bufref, buf);
  if (buf->b_ml.ml_mfp != NULL)
  {
    if (apply_autocmds(EVENT_BUFUNLOAD, buf->b_fname, buf->b_fname,
                       FALSE, buf) &&
        !bufref_valid(&bufref))
      /* autocommands deleted the buffer */
      return;
  }
  if ((flags & BFA_DEL) && buf->b_p_bl)
  {
    if (apply_autocmds(EVENT_BUFDELETE, buf->b_fname, buf->b_fname,
                       FALSE, buf) &&
        !bufref_valid(&bufref))
      /* autocommands deleted the buffer */
      return;
  }
  if (flags & BFA_WIPE)
  {
    if (apply_autocmds(EVENT_BUFWIPEOUT, buf->b_fname, buf->b_fname,
                       FALSE, buf) &&
        !bufref_valid(&bufref))
      /* autocommands deleted the buffer */
      return;
  }
  --buf->b_locked;

  /* If the buffer was in curwin and the window has changed, go back to that
     * window, if it still exists.  This avoids that ":edit x" triggering a
     * "tabnext" BufUnload autocmd leaves a window behind without a buffer. */
  if (is_curwin && curwin != the_curwin && win_valid_any_tab(the_curwin))
  {
    block_autocmds();
    goto_tabpage_win(the_curtab, the_curwin);
    unblock_autocmds();
  }

#ifdef FEAT_EVAL
  if (aborting()) /* autocmds may abort script processing */
    return;
#endif

  /*
     * It's possible that autocommands change curbuf to the one being deleted.
     * This might cause curbuf to be deleted unexpectedly.  But in some cases
     * it's OK to delete the curbuf, because a new one is obtained anyway.
     * Therefore only return if curbuf changed to the deleted buffer.
     */
  if (buf == curbuf && !is_curbuf)
    return;
#ifdef FEAT_DIFF
  diff_buf_delete(buf); /* Can't use 'diff' for unloaded buffer. */
#endif

#ifdef FEAT_FOLDING
  /* No folds in an empty buffer. */
  {
    win_T *win;
    tabpage_T *tp;

    FOR_ALL_TAB_WINDOWS(tp, win)
    if (win->w_buffer == buf)
      clearFolding(win);
  }
#endif

  ml_close(buf, TRUE);         /* close and delete the memline/memfile */
  buf->b_ml.ml_line_count = 0; /* no lines in buffer */
  if ((flags & BFA_KEEP_UNDO) == 0)
  {
    u_blockfree(buf); /* free the memory allocated for undo */
    u_clearall(buf);  /* reset all undo information */
  }
  buf->b_flags &= ~BF_READERR; /* a read error is no longer relevant */
}

/*
 * Free a buffer structure and the things it contains related to the buffer
 * itself (not the file, that must have been done already).
 */
static void
free_buffer(buf_T *buf)
{
  ++buf_free_count;
  free_buffer_stuff(buf, TRUE);
#ifdef FEAT_EVAL
  /* b:changedtick uses an item in buf_T, remove it now */
  dictitem_remove(buf->b_vars, (dictitem_T *)&buf->b_ct_di);
  unref_var_dict(buf->b_vars);
#endif
#ifdef FEAT_LUA
  lua_buffer_free(buf);
#endif
#ifdef FEAT_MZSCHEME
  mzscheme_buffer_free(buf);
#endif
#ifdef FEAT_PYTHON
  python_buffer_free(buf);
#endif
#ifdef FEAT_PYTHON3
  python3_buffer_free(buf);
#endif
#ifdef FEAT_JOB_CHANNEL
  channel_buffer_free(buf);
#endif
#ifdef FEAT_TERMINAL
  free_terminal(buf);
#endif
#ifdef FEAT_JOB_CHANNEL
  vim_free(buf->b_prompt_text);
  free_callback(&buf->b_prompt_callback);
#endif

  buf_hashtab_remove(buf);

  aubuflocal_remove(buf);

  if (autocmd_busy)
  {
    /* Do not free the buffer structure while autocommands are executing,
	 * it's still needed. Free it when autocmd_busy is reset. */
    buf->b_next = au_pending_free_buf;
    au_pending_free_buf = buf;
  }
  else
    vim_free(buf);
}

/*
 * Initializes b:changedtick.
 */
static void
init_changedtick(buf_T *buf)
{
  dictitem_T *di = (dictitem_T *)&buf->b_ct_di;

  di->di_flags = DI_FLAGS_FIX | DI_FLAGS_RO;
  di->di_tv.v_type = VAR_NUMBER;
  di->di_tv.v_lock = VAR_FIXED;
  di->di_tv.vval.v_number = 0;

#ifdef FEAT_EVAL
  STRCPY(buf->b_ct_di.di_key, "changedtick");
  (void)dict_add(buf->b_vars, di);
#endif
}

/*
 * Free stuff in the buffer for ":bdel" and when wiping out the buffer.
 */
static void
free_buffer_stuff(
    buf_T *buf,
    int free_options) /* free options as well */
{
  if (free_options)
  {
    clear_wininfo(buf); /* including window-local options */
    free_buf_options(buf, TRUE);
  }
#ifdef FEAT_EVAL
  {
    varnumber_T tick = CHANGEDTICK(buf);

    vars_clear(&buf->b_vars->dv_hashtab); /* free all buffer variables */
    hash_init(&buf->b_vars->dv_hashtab);
    init_changedtick(buf);
    CHANGEDTICK(buf) = tick;
  }
#endif
  uc_clear(&buf->b_ucmds); // clear local user commands
#ifdef FEAT_SIGNS
  buf_delete_signs(buf, (char_u *)"*"); // delete any signs
#endif
#ifdef FEAT_LOCALMAP
  map_clear_int(buf, MAP_ALL_MODES, TRUE, FALSE); /* clear local mappings */
  map_clear_int(buf, MAP_ALL_MODES, TRUE, TRUE);  /* clear local abbrevs */
#endif
  VIM_CLEAR(buf->b_start_fenc);
}

/*
 * Free the b_wininfo list for buffer "buf".
 */
static void
clear_wininfo(buf_T *buf)
{
  wininfo_T *wip;

  while (buf->b_wininfo != NULL)
  {
    wip = buf->b_wininfo;
    buf->b_wininfo = wip->wi_next;
    if (wip->wi_optset)
    {
      clear_winopt(&wip->wi_opt);
#ifdef FEAT_FOLDING
      deleteFoldRecurse(&wip->wi_folds);
#endif
    }
    vim_free(wip);
  }
}

/*
 * Go to another buffer.  Handles the result of the ATTENTION dialog.
 */
void goto_buffer(
    exarg_T *eap,
    int start,
    int dir,
    int count)
{
  bufref_T old_curbuf;

  set_bufref(&old_curbuf, curbuf);

  swap_exists_action = SEA_DIALOG;
  (void)do_buffer(*eap->cmd == 's' ? DOBUF_SPLIT : DOBUF_GOTO,
                  start, dir, count, eap->forceit);
  if (swap_exists_action == SEA_QUIT && *eap->cmd == 's')
  {
#if defined(FEAT_EVAL)
    cleanup_T cs;

    /* Reset the error/interrupt/exception state here so that
	 * aborting() returns FALSE when closing a window. */
    enter_cleanup(&cs);
#endif

    /* Quitting means closing the split window, nothing else. */
    win_close(curwin, TRUE);
    swap_exists_action = SEA_NONE;
    swap_exists_did_quit = TRUE;

#if defined(FEAT_EVAL)
    /* Restore the error/interrupt/exception state if not discarded by a
	 * new aborting error, interrupt, or uncaught exception. */
    leave_cleanup(&cs);
#endif
  }
  else
    handle_swap_exists(&old_curbuf);
}

/*
 * Handle the situation of swap_exists_action being set.
 * It is allowed for "old_curbuf" to be NULL or invalid.
 */
void handle_swap_exists(bufref_T *old_curbuf)
{
#if defined(FEAT_EVAL)
  cleanup_T cs;
#endif
  buf_T *buf;

  if (swap_exists_action == SEA_QUIT)
  {
#if defined(FEAT_EVAL)
    /* Reset the error/interrupt/exception state here so that
	 * aborting() returns FALSE when closing a buffer. */
    enter_cleanup(&cs);
#endif

    /* User selected Quit at ATTENTION prompt.  Go back to previous
	 * buffer.  If that buffer is gone or the same as the current one,
	 * open a new, empty buffer. */
    swap_exists_action = SEA_NONE; /* don't want it again */
    swap_exists_did_quit = TRUE;
    close_buffer(curwin, curbuf, DOBUF_UNLOAD, FALSE);
    if (old_curbuf == NULL || !bufref_valid(old_curbuf) || old_curbuf->br_buf == curbuf)
      buf = buflist_new(NULL, NULL, 1L, BLN_CURBUF | BLN_LISTED);
    else
      buf = old_curbuf->br_buf;
    if (buf != NULL)
    {
      int old_msg_silent = msg_silent;

      if (shortmess(SHM_FILEINFO))
        msg_silent = 1; // prevent fileinfo message
      enter_buffer(buf);
      // restore msg_silent, so that the command line will be shown
      msg_silent = old_msg_silent;
    }
    /* If "old_curbuf" is NULL we are in big trouble here... */

#if defined(FEAT_EVAL)
    /* Restore the error/interrupt/exception state if not discarded by a
	 * new aborting error, interrupt, or uncaught exception. */
    leave_cleanup(&cs);
#endif
  }
  else if (swap_exists_action == SEA_RECOVER)
  {
#if defined(FEAT_EVAL)
    /* Reset the error/interrupt/exception state here so that
	 * aborting() returns FALSE when closing a buffer. */
    enter_cleanup(&cs);
#endif

    /* User selected Recover at ATTENTION prompt. */
    msg_scroll = TRUE;
    ml_recover(FALSE);
    msg_puts("\n"); /* don't overwrite the last message */
    cmdline_row = msg_row;
    do_modelines(0);

#if defined(FEAT_EVAL)
    /* Restore the error/interrupt/exception state if not discarded by a
	 * new aborting error, interrupt, or uncaught exception. */
    leave_cleanup(&cs);
#endif
  }
  swap_exists_action = SEA_NONE;
}

/*
 * do_bufdel() - delete or unload buffer(s)
 *
 * addr_count == 0: ":bdel" - delete current buffer
 * addr_count == 1: ":N bdel" or ":bdel N [N ..]" - first delete
 *		    buffer "end_bnr", then any other arguments.
 * addr_count == 2: ":N,N bdel" - delete buffers in range
 *
 * command can be DOBUF_UNLOAD (":bunload"), DOBUF_WIPE (":bwipeout") or
 * DOBUF_DEL (":bdel")
 *
 * Returns error message or NULL
 */
char *
do_bufdel(
    int command,
    char_u *arg, /* pointer to extra arguments */
    int addr_count,
    int start_bnr, /* first buffer number in a range */
    int end_bnr,   /* buffer nr or last buffer nr in a range */
    int forceit)
{
  int do_current = 0;    /* delete current buffer? */
  int deleted = 0;       /* number of buffers deleted */
  char *errormsg = NULL; /* return value */
  int bnr;               /* buffer number */
  char_u *p;

  if (addr_count == 0)
  {
    (void)do_buffer(command, DOBUF_CURRENT, FORWARD, 0, forceit);
  }
  else
  {
    if (addr_count == 2)
    {
      if (*arg) /* both range and argument is not allowed */
        return _(e_trailing);
      bnr = start_bnr;
    }
    else /* addr_count == 1 */
      bnr = end_bnr;

    for (; !got_int; ui_breakcheck())
    {
      /*
	     * delete the current buffer last, otherwise when the
	     * current buffer is deleted, the next buffer becomes
	     * the current one and will be loaded, which may then
	     * also be deleted, etc.
	     */
      if (bnr == curbuf->b_fnum)
        do_current = bnr;
      else if (do_buffer(command, DOBUF_FIRST, FORWARD, (int)bnr,
                         forceit) == OK)
        ++deleted;

      /*
	     * find next buffer number to delete/unload
	     */
      if (addr_count == 2)
      {
        if (++bnr > end_bnr)
          break;
      }
      else /* addr_count == 1 */
      {
        arg = skipwhite(arg);
        if (*arg == NUL)
          break;
        if (!VIM_ISDIGIT(*arg))
        {
          p = skiptowhite_esc(arg);
          bnr = buflist_findpat(arg, p, command == DOBUF_WIPE,
                                FALSE, FALSE);
          if (bnr < 0) /* failed */
            break;
          arg = p;
        }
        else
          bnr = getdigits(&arg);
      }
    }
    if (!got_int && do_current && do_buffer(command, DOBUF_FIRST, FORWARD, do_current, forceit) == OK)
      ++deleted;

    if (deleted == 0)
    {
      if (command == DOBUF_UNLOAD)
        STRCPY(IObuff, _("E515: No buffers were unloaded"));
      else if (command == DOBUF_DEL)
        STRCPY(IObuff, _("E516: No buffers were deleted"));
      else
        STRCPY(IObuff, _("E517: No buffers were wiped out"));
      errormsg = (char *)IObuff;
    }
    else if (deleted >= p_report)
    {
      if (command == DOBUF_UNLOAD)
        smsg(NGETTEXT("%d buffer unloaded",
                      "%d buffers unloaded", deleted),
             deleted);
      else if (command == DOBUF_DEL)
        smsg(NGETTEXT("%d buffer deleted",
                      "%d buffers deleted", deleted),
             deleted);
      else
        smsg(NGETTEXT("%d buffer wiped out",
                      "%d buffers wiped out", deleted),
             deleted);
    }
  }

  return errormsg;
}

/*
 * Make the current buffer empty.
 * Used when it is wiped out and it's the last buffer.
 */
static int
empty_curbuf(
    int close_others,
    int forceit,
    int action)
{
  int retval;
  buf_T *buf = curbuf;
  bufref_T bufref;

  if (action == DOBUF_UNLOAD)
  {
    emsg(_("E90: Cannot unload last buffer"));
    return FAIL;
  }

  set_bufref(&bufref, buf);
  if (close_others)
    /* Close any other windows on this buffer, then make it empty. */
    close_windows(buf, TRUE);

  setpcmark();
  retval = do_ecmd(0, NULL, NULL, NULL, ECMD_ONE,
                   forceit ? ECMD_FORCEIT : 0, curwin);

  /*
     * do_ecmd() may create a new buffer, then we have to delete
     * the old one.  But do_ecmd() may have done that already, check
     * if the buffer still exists.
     */
  if (buf != curbuf && bufref_valid(&bufref) && buf->b_nwindows == 0)
    close_buffer(NULL, buf, action, FALSE);
  if (!close_others)
    need_fileinfo = FALSE;
  return retval;
}

/*
 * Implementation of the commands for the buffer list.
 *
 * action == DOBUF_GOTO	    go to specified buffer
 * action == DOBUF_SPLIT    split window and go to specified buffer
 * action == DOBUF_UNLOAD   unload specified buffer(s)
 * action == DOBUF_DEL	    delete specified buffer(s) from buffer list
 * action == DOBUF_WIPE	    delete specified buffer(s) really
 *
 * start == DOBUF_CURRENT   go to "count" buffer from current buffer
 * start == DOBUF_FIRST	    go to "count" buffer from first buffer
 * start == DOBUF_LAST	    go to "count" buffer from last buffer
 * start == DOBUF_MOD	    go to "count" modified buffer from current buffer
 *
 * Return FAIL or OK.
 */
int do_buffer(
    int action,
    int start,
    int dir,     /* FORWARD or BACKWARD */
    int count,   /* buffer number or number of buffers */
    int forceit) /* TRUE for :...! */
{
  buf_T *buf;
  buf_T *bp;
  int unload = (action == DOBUF_UNLOAD || action == DOBUF_DEL || action == DOBUF_WIPE);

  switch (start)
  {
  case DOBUF_FIRST:
    buf = firstbuf;
    break;
  case DOBUF_LAST:
    buf = lastbuf;
    break;
  default:
    buf = curbuf;
    break;
  }
  if (start == DOBUF_MOD) /* find next modified buffer */
  {
    while (count-- > 0)
    {
      do
      {
        buf = buf->b_next;
        if (buf == NULL)
          buf = firstbuf;
      } while (buf != curbuf && !bufIsChanged(buf));
    }
    if (!bufIsChanged(buf))
    {
      emsg(_("E84: No modified buffer found"));
      return FAIL;
    }
  }
  else if (start == DOBUF_FIRST && count) /* find specified buffer number */
  {
    while (buf != NULL && buf->b_fnum != count)
      buf = buf->b_next;
  }
  else
  {
    bp = NULL;
    while (count > 0 || (!unload && !buf->b_p_bl && bp != buf))
    {
      /* remember the buffer where we start, we come back there when all
	     * buffers are unlisted. */
      if (bp == NULL)
        bp = buf;
      if (dir == FORWARD)
      {
        buf = buf->b_next;
        if (buf == NULL)
          buf = firstbuf;
      }
      else
      {
        buf = buf->b_prev;
        if (buf == NULL)
          buf = lastbuf;
      }
      /* don't count unlisted buffers */
      if (unload || buf->b_p_bl)
      {
        --count;
        bp = NULL; /* use this buffer as new starting point */
      }
      if (bp == buf)
      {
        /* back where we started, didn't find anything. */
        emsg(_("E85: There is no listed buffer"));
        return FAIL;
      }
    }
  }

  if (buf == NULL) /* could not find it */
  {
    if (start == DOBUF_FIRST)
    {
      /* don't warn when deleting */
      if (!unload)
        semsg(_(e_nobufnr), count);
    }
    else if (dir == FORWARD)
      emsg(_("E87: Cannot go beyond last buffer"));
    else
      emsg(_("E88: Cannot go before first buffer"));
    return FAIL;
  }

  /*
     * delete buffer buf from memory and/or the list
     */
  if (unload)
  {
    int forward;
    bufref_T bufref;

    if (!can_unload_buffer(buf))
      return FAIL;

    set_bufref(&bufref, buf);

    /* When unloading or deleting a buffer that's already unloaded and
	 * unlisted: fail silently. */
    if (action != DOBUF_WIPE && buf->b_ml.ml_mfp == NULL && !buf->b_p_bl)
      return FAIL;

    if (!forceit && bufIsChanged(buf))
    {
      {
        semsg(_("E89: No write since last change for buffer %d (add ! to override)"),
              buf->b_fnum);
        return FAIL;
      }
    }

    /* When closing the current buffer stop Visual mode. */
    if (buf == curbuf && VIsual_active)
      end_visual_mode();

    /*
	 * If deleting the last (listed) buffer, make it empty.
	 * The last (listed) buffer cannot be unloaded.
	 */
    FOR_ALL_BUFFERS(bp)
    if (bp->b_p_bl && bp != buf)
      break;
    if (bp == NULL && buf == curbuf)
      return empty_curbuf(TRUE, forceit, action);

    /*
	 * If the deleted buffer is the current one, close the current window
	 * (unless it's the only window).  Repeat this so long as we end up in
	 * a window with this buffer.
	 */
    while (buf == curbuf && !(curwin->w_closing || curwin->w_buffer->b_locked > 0) && (!ONE_WINDOW || first_tabpage->tp_next != NULL))
    {
      if (win_close(curwin, FALSE) == FAIL)
        break;
    }

    /*
	 * If the buffer to be deleted is not the current one, delete it here.
	 */
    if (buf != curbuf)
    {
      close_windows(buf, FALSE);
      if (buf != curbuf && bufref_valid(&bufref) && buf->b_nwindows <= 0)
        close_buffer(NULL, buf, action, FALSE);
      return OK;
    }

    /*
	 * Deleting the current buffer: Need to find another buffer to go to.
	 * There should be another, otherwise it would have been handled
	 * above.  However, autocommands may have deleted all buffers.
	 * First use au_new_curbuf.br_buf, if it is valid.
	 * Then prefer the buffer we most recently visited.
	 * Else try to find one that is loaded, after the current buffer,
	 * then before the current buffer.
	 * Finally use any buffer.
	 */
    buf = NULL; /* selected buffer */
    bp = NULL;  /* used when no loaded buffer found */
    if (au_new_curbuf.br_buf != NULL && bufref_valid(&au_new_curbuf))
      buf = au_new_curbuf.br_buf;
#ifdef FEAT_JUMPLIST
    else if (curwin->w_jumplistlen > 0)
    {
      int jumpidx;

      jumpidx = curwin->w_jumplistidx - 1;
      if (jumpidx < 0)
        jumpidx = curwin->w_jumplistlen - 1;

      forward = jumpidx;
      while (jumpidx != curwin->w_jumplistidx)
      {
        buf = buflist_findnr(curwin->w_jumplist[jumpidx].fmark.fnum);
        if (buf != NULL)
        {
          if (buf == curbuf || !buf->b_p_bl)
            buf = NULL; /* skip current and unlisted bufs */
          else if (buf->b_ml.ml_mfp == NULL)
          {
            /* skip unloaded buf, but may keep it for later */
            if (bp == NULL)
              bp = buf;
            buf = NULL;
          }
        }
        if (buf != NULL) /* found a valid buffer: stop searching */
          break;
        /* advance to older entry in jump list */
        if (!jumpidx && curwin->w_jumplistidx == curwin->w_jumplistlen)
          break;
        if (--jumpidx < 0)
          jumpidx = curwin->w_jumplistlen - 1;
        if (jumpidx == forward) /* List exhausted for sure */
          break;
      }
    }
#endif

    if (buf == NULL) /* No previous buffer, Try 2'nd approach */
    {
      forward = TRUE;
      buf = curbuf->b_next;
      for (;;)
      {
        if (buf == NULL)
        {
          if (!forward) /* tried both directions */
            break;
          buf = curbuf->b_prev;
          forward = FALSE;
          continue;
        }
        /* in non-help buffer, try to skip help buffers, and vv */
        if (buf->b_help == curbuf->b_help && buf->b_p_bl)
        {
          if (buf->b_ml.ml_mfp != NULL) /* found loaded buffer */
            break;
          if (bp == NULL) /* remember unloaded buf for later */
            bp = buf;
        }
        if (forward)
          buf = buf->b_next;
        else
          buf = buf->b_prev;
      }
    }
    if (buf == NULL) /* No loaded buffer, use unloaded one */
      buf = bp;
    if (buf == NULL) /* No loaded buffer, find listed one */
    {
      FOR_ALL_BUFFERS(buf)
      if (buf->b_p_bl && buf != curbuf)
        break;
    }
    if (buf == NULL) /* Still no buffer, just take one */
    {
      if (curbuf->b_next != NULL)
        buf = curbuf->b_next;
      else
        buf = curbuf->b_prev;
    }
  }

  if (buf == NULL)
  {
    /* Autocommands must have wiped out all other buffers.  Only option
	 * now is to make the current buffer empty. */
    return empty_curbuf(FALSE, forceit, action);
  }

  /*
     * make buf current buffer
     */
  if (action == DOBUF_SPLIT) /* split window first */
  {
    /* If 'switchbuf' contains "useopen": jump to first window containing
	 * "buf" if one exists */
    if ((swb_flags & SWB_USEOPEN) && buf_jump_open_win(buf))
      return OK;
    /* If 'switchbuf' contains "usetab": jump to first window in any tab
	 * page containing "buf" if one exists */
    if ((swb_flags & SWB_USETAB) && buf_jump_open_tab(buf))
      return OK;
    if (win_split(0, 0) == FAIL)
      return FAIL;
  }

  /* go to current buffer - nothing to do */
  if (buf == curbuf)
    return OK;

  /*
     * Check if the current buffer may be abandoned.
     */
  if (action == DOBUF_GOTO && !can_abandon(curbuf, forceit))
  {
    {
      no_write_message();
      return FAIL;
    }
  }

  /* Go to the other buffer. */
  set_curbuf(buf, action);

  if (action == DOBUF_SPLIT)
    RESET_BINDING(curwin); /* reset 'scrollbind' and 'cursorbind' */

#if defined(FEAT_EVAL)
  if (aborting()) /* autocmds may abort script processing */
    return FAIL;
#endif

  return OK;
}

/*
 * Set current buffer to "buf".  Executes autocommands and closes current
 * buffer.  "action" tells how to close the current buffer:
 * DOBUF_GOTO	    free or hide it
 * DOBUF_SPLIT	    nothing
 * DOBUF_UNLOAD	    unload it
 * DOBUF_DEL	    delete it
 * DOBUF_WIPE	    wipe it out
 */
void set_curbuf(buf_T *buf, int action)
{
  buf_T *prevbuf;
  int unload = (action == DOBUF_UNLOAD || action == DOBUF_DEL || action == DOBUF_WIPE);
  bufref_T newbufref;
  bufref_T prevbufref;

  setpcmark();
  if (!cmdmod.keepalt)
    curwin->w_alt_fnum = curbuf->b_fnum; /* remember alternate file */
  buflist_altfpos(curwin);               /* remember curpos */

  /* Don't restart Select mode after switching to another buffer. */
  VIsual_reselect = FALSE;

  /* close_windows() or apply_autocmds() may change curbuf and wipe out "buf"
     */
  prevbuf = curbuf;
  set_bufref(&prevbufref, prevbuf);
  set_bufref(&newbufref, buf);

  /* Autocommands may delete the curren buffer and/or the buffer we wan to go
     * to.  In those cases don't close the buffer. */
  if (!apply_autocmds(EVENT_BUFLEAVE, NULL, NULL, FALSE, curbuf) || (bufref_valid(&prevbufref) && bufref_valid(&newbufref)
#ifdef FEAT_EVAL
                                                                     && !aborting()
#endif
                                                                         ))
  {
    if (unload)
      close_windows(prevbuf, FALSE);
#if defined(FEAT_EVAL)
    if (bufref_valid(&prevbufref) && !aborting())
#else
    if (bufref_valid(&prevbufref))
#endif
    {
      win_T *previouswin = curwin;
      if (prevbuf == curbuf)
        u_sync(FALSE);
      close_buffer(prevbuf == curwin->w_buffer ? curwin : NULL, prevbuf,
                   unload ? action : (action == DOBUF_GOTO && !buf_hide(prevbuf) && !bufIsChanged(prevbuf)) ? DOBUF_UNLOAD : 0, FALSE);
      if (curwin != previouswin && win_valid(previouswin))
        /* autocommands changed curwin, Grr! */
        curwin = previouswin;
    }
  }
  /* An autocommand may have deleted "buf", already entered it (e.g., when
     * it did ":bunload") or aborted the script processing.
     * If curwin->w_buffer is null, enter_buffer() will make it valid again */
  if ((buf_valid(buf) && buf != curbuf
#ifdef FEAT_EVAL
       && !aborting()
#endif
           ) ||
      curwin->w_buffer == NULL)
  {
    enter_buffer(buf);
  }
}

/*
 * Enter a new current buffer.
 * Old curbuf must have been abandoned already!  This also means "curbuf" may
 * be pointing to freed memory.
 */
void enter_buffer(buf_T *buf)
{
  /* Copy buffer and window local option values.  Not for a help buffer. */
  buf_copy_options(buf, BCO_ENTER | BCO_NOHELP);
  if (!buf->b_help)
    get_winopts(buf);
#ifdef FEAT_FOLDING
  else
    /* Remove all folds in the window. */
    clearFolding(curwin);
  foldUpdateAll(curwin); /* update folds (later). */
#endif

  /* Get the buffer in the current window. */
  curwin->w_buffer = buf;
  curbuf = buf;
  ++curbuf->b_nwindows;

#ifdef FEAT_DIFF
  if (curwin->w_p_diff)
    diff_buf_add(curbuf);
#endif

  /* Cursor on first line by default. */
  curwin->w_cursor.lnum = 1;
  curwin->w_cursor.col = 0;
  curwin->w_cursor.coladd = 0;
  curwin->w_set_curswant = TRUE;
  curwin->w_topline_was_set = FALSE;

  /* mark cursor position as being invalid */
  curwin->w_valid = 0;

  buflist_setfpos(curbuf, curwin, curbuf->b_last_cursor.lnum,
                  curbuf->b_last_cursor.col, TRUE);

  /* Make sure the buffer is loaded. */
  if (curbuf->b_ml.ml_mfp == NULL) /* need to load the file */
  {
    /* If there is no filetype, allow for detecting one.  Esp. useful for
	 * ":ball" used in a autocommand.  If there already is a filetype we
	 * might prefer to keep it. */
    if (*curbuf->b_p_ft == NUL)
      did_filetype = FALSE;

    open_buffer(FALSE, NULL, 0);
  }
  else
  {
    if (!msg_silent && !shortmess(SHM_FILEINFO))
      need_fileinfo = TRUE; // display file info after redraw

    // check if file changed
    (void)buf_check_timestamp(curbuf, FALSE);

    curwin->w_topline = 1;
#ifdef FEAT_DIFF
    curwin->w_topfill = 0;
#endif
    apply_autocmds(EVENT_BUFENTER, NULL, NULL, FALSE, curbuf);
    apply_autocmds(EVENT_BUFWINENTER, NULL, NULL, FALSE, curbuf);
  }

  /* If autocommands did not change the cursor position, restore cursor lnum
     * and possibly cursor col. */
  if (curwin->w_cursor.lnum == 1 && inindent(0))
    buflist_getfpos();

  check_arg_idx(curwin); /* check for valid arg_idx */

  /* when autocmds didn't change it */
  if (curwin->w_topline == 1 && !curwin->w_topline_was_set)
    scroll_cursor_halfway(FALSE); /* redisplay at correct position */

  /* Change directories when the 'acd' option is set. */
  DO_AUTOCHDIR;
#ifdef FEAT_KEYMAP
  if (curbuf->b_kmap_state & KEYMAP_INIT)
    (void)keymap_init();
#endif
#ifdef FEAT_VIMINFO
  curbuf->b_last_used = vim_time();
#endif

  redraw_later(NOT_VALID);
}

#if defined(FEAT_AUTOCHDIR) || defined(PROTO)
/*
 * Change to the directory of the current buffer.
 * Don't do this while still starting up.
 */
void do_autochdir(void)
{
  if ((starting == 0 || test_autochdir) && curbuf->b_ffname != NULL && vim_chdirfile(curbuf->b_ffname, "auto") == OK)
    shorten_fnames(TRUE);
}
#endif

void no_write_message(void)
{
#ifdef FEAT_TERMINAL
  if (term_job_running(curbuf->b_term))
    emsg(_("E948: Job still running (add ! to end the job)"));
  else
#endif
    emsg(_("E37: No write since last change (add ! to override)"));
}

void no_write_message_nobang(buf_T *buf UNUSED)
{
#ifdef FEAT_TERMINAL
  if (term_job_running(buf->b_term))
    emsg(_("E948: Job still running"));
  else
#endif
    emsg(_("E37: No write since last change"));
}

/*
 * functions for dealing with the buffer list
 */

static int top_file_num = 1; /* highest file number */

/*
 * Return TRUE if the current buffer is empty, unnamed, unmodified and used in
 * only one window.  That means it can be re-used.
 */
int curbuf_reusable(void)
{
  return (curbuf != NULL && curbuf->b_ffname == NULL && curbuf->b_nwindows <= 1 && (curbuf->b_ml.ml_mfp == NULL || BUFEMPTY())
#if defined(FEAT_QUICKFIX)
          && !bt_quickfix(curbuf)
#endif
          && !curbufIsChanged());
}

/*
 * Add a file name to the buffer list.  Return a pointer to the buffer.
 * If the same file name already exists return a pointer to that buffer.
 * If it does not exist, or if fname == NULL, a new entry is created.
 * If (flags & BLN_CURBUF) is TRUE, may use current buffer.
 * If (flags & BLN_LISTED) is TRUE, add new buffer to buffer list.
 * If (flags & BLN_DUMMY) is TRUE, don't count it as a real buffer.
 * If (flags & BLN_NEW) is TRUE, don't use an existing buffer.
 * If (flags & BLN_NOOPT) is TRUE, don't copy options from the current buffer
 *				    if the buffer already exists.
 * This is the ONLY way to create a new buffer.
 */
buf_T *
buflist_new(
    char_u *ffname_arg, // full path of fname or relative
    char_u *sfname_arg, // short fname or NULL
    linenr_T lnum,      // preferred cursor line
    int flags)          // BLN_ defines
{
  char_u *ffname = ffname_arg;
  char_u *sfname = sfname_arg;
  buf_T *buf;
#ifdef UNIX
  stat_T st;
#endif

  if (top_file_num == 1)
    hash_init(&buf_hashtab);

  fname_expand(curbuf, &ffname, &sfname); // will allocate ffname

  /*
     * If file name already exists in the list, update the entry.
     */
#ifdef UNIX
  /* On Unix we can use inode numbers when the file exists.  Works better
     * for hard links. */
  if (sfname == NULL || mch_stat((char *)sfname, &st) < 0)
    st.st_dev = (dev_T)-1;
#endif
  if (ffname != NULL && !(flags & (BLN_DUMMY | BLN_NEW)) && (buf =
#ifdef UNIX
                                                                 buflist_findname_stat(ffname, &st)
#else
                                                                 buflist_findname(ffname)
#endif
                                                                 ) != NULL)
  {
    vim_free(ffname);
    if (lnum != 0)
      buflist_setfpos(buf, curwin, lnum, (colnr_T)0, FALSE);

    if ((flags & BLN_NOOPT) == 0)
      /* copy the options now, if 'cpo' doesn't have 's' and not done
	     * already */
      buf_copy_options(buf, 0);

    if ((flags & BLN_LISTED) && !buf->b_p_bl)
    {
      bufref_T bufref;

      buf->b_p_bl = TRUE;
      set_bufref(&bufref, buf);
      if (!(flags & BLN_DUMMY))
      {
        if (apply_autocmds(EVENT_BUFADD, NULL, NULL, FALSE, buf) && !bufref_valid(&bufref))
          return NULL;
      }
    }
    return buf;
  }

  /*
     * If the current buffer has no name and no contents, use the current
     * buffer.	Otherwise: Need to allocate a new buffer structure.
     *
     * This is the ONLY place where a new buffer structure is allocated!
     * (A spell file buffer is allocated in spell.c, but that's not a normal
     * buffer.)
     */
  buf = NULL;
  if ((flags & BLN_CURBUF) && curbuf_reusable())
  {
    buf = curbuf;
    /* It's like this buffer is deleted.  Watch out for autocommands that
	 * change curbuf!  If that happens, allocate a new buffer anyway. */
    if (curbuf->b_p_bl)
      apply_autocmds(EVENT_BUFDELETE, NULL, NULL, FALSE, curbuf);
    if (buf == curbuf)
      apply_autocmds(EVENT_BUFWIPEOUT, NULL, NULL, FALSE, curbuf);
#ifdef FEAT_EVAL
    if (aborting()) /* autocmds may abort script processing */
      return NULL;
#endif
    if (buf == curbuf)
    {
      /* Make sure 'bufhidden' and 'buftype' are empty */
      clear_string_option(&buf->b_p_bh);
      clear_string_option(&buf->b_p_bt);
    }
  }
  if (buf != curbuf || curbuf == NULL)
  {
    buf = ALLOC_CLEAR_ONE(buf_T);
    if (buf == NULL)
    {
      vim_free(ffname);
      return NULL;
    }
#ifdef FEAT_EVAL
    /* init b: variables */
    buf->b_vars = dict_alloc();
    if (buf->b_vars == NULL)
    {
      vim_free(ffname);
      vim_free(buf);
      return NULL;
    }
    init_var_dict(buf->b_vars, &buf->b_bufvar, VAR_SCOPE);
#endif
    init_changedtick(buf);
  }

  if (ffname != NULL)
  {
    buf->b_ffname = ffname;
    buf->b_sfname = vim_strsave(sfname);
  }

  clear_wininfo(buf);
  buf->b_wininfo = ALLOC_CLEAR_ONE(wininfo_T);

  if ((ffname != NULL && (buf->b_ffname == NULL || buf->b_sfname == NULL)) || buf->b_wininfo == NULL)
  {
    if (buf->b_sfname != buf->b_ffname)
      VIM_CLEAR(buf->b_sfname);
    else
      buf->b_sfname = NULL;
    VIM_CLEAR(buf->b_ffname);
    if (buf != curbuf)
      free_buffer(buf);
    return NULL;
  }

  if (buf == curbuf)
  {
    /* free all things allocated for this buffer */
    buf_freeall(buf, 0);
    if (buf != curbuf) /* autocommands deleted the buffer! */
      return NULL;
#if defined(FEAT_EVAL)
    if (aborting()) /* autocmds may abort script processing */
      return NULL;
#endif
    free_buffer_stuff(buf, FALSE); /* delete local variables et al. */

    /* Init the options. */
    buf->b_p_initialized = FALSE;
    buf_copy_options(buf, BCO_ENTER);

#ifdef FEAT_KEYMAP
    /* need to reload lmaps and set b:keymap_name */
    curbuf->b_kmap_state |= KEYMAP_INIT;
#endif
  }
  else
  {
    /*
	 * put new buffer at the end of the buffer list
	 */
    buf->b_next = NULL;
    if (firstbuf == NULL) /* buffer list is empty */
    {
      buf->b_prev = NULL;
      firstbuf = buf;
    }
    else /* append new buffer at end of list */
    {
      lastbuf->b_next = buf;
      buf->b_prev = lastbuf;
    }
    lastbuf = buf;

    buf->b_fnum = top_file_num++;
    if (top_file_num < 0) /* wrap around (may cause duplicates) */
    {
      emsg(_("W14: Warning: List of file names overflow"));
      top_file_num = 1;
    }
    buf_hashtab_add(buf);

    /*
	 * Always copy the options from the current buffer.
	 */
    buf_copy_options(buf, BCO_ALWAYS);
  }

  buf->b_wininfo->wi_fpos.lnum = lnum;
  buf->b_wininfo->wi_win = curwin;

  buf->b_fname = buf->b_sfname;
#ifdef UNIX
  if (st.st_dev == (dev_T)-1)
    buf->b_dev_valid = FALSE;
  else
  {
    buf->b_dev_valid = TRUE;
    buf->b_dev = st.st_dev;
    buf->b_ino = st.st_ino;
  }
#endif
  buf->b_u_synced = TRUE;
  buf->b_flags = BF_CHECK_RO | BF_NEVERLOADED;
  if (flags & BLN_DUMMY)
    buf->b_flags |= BF_DUMMY;
  buf_clear_file(buf);
  clrallmarks(buf);                                  /* clear marks */
  fmarks_check_names(buf);                           /* check file marks for this file */
  buf->b_p_bl = (flags & BLN_LISTED) ? TRUE : FALSE; /* init 'buflisted' */
  if (!(flags & BLN_DUMMY))
  {
    bufref_T bufref;

    /* Tricky: these autocommands may change the buffer list.  They could
	 * also split the window with re-using the one empty buffer. This may
	 * result in unexpectedly losing the empty buffer. */
    set_bufref(&bufref, buf);
    if (apply_autocmds(EVENT_BUFNEW, NULL, NULL, FALSE, buf) && !bufref_valid(&bufref))
      return NULL;
    if (flags & BLN_LISTED)
    {
      if (apply_autocmds(EVENT_BUFADD, NULL, NULL, FALSE, buf) && !bufref_valid(&bufref))
        return NULL;
    }
#ifdef FEAT_EVAL
    if (aborting()) /* autocmds may abort script processing */
      return NULL;
#endif
  }

  return buf;
}

/*
 * Free the memory for the options of a buffer.
 * If "free_p_ff" is TRUE also free 'fileformat', 'buftype' and
 * 'fileencoding'.
 */
void free_buf_options(
    buf_T *buf,
    int free_p_ff)
{
  if (free_p_ff)
  {
    clear_string_option(&buf->b_p_fenc);
    clear_string_option(&buf->b_p_ff);
    clear_string_option(&buf->b_p_bh);
    clear_string_option(&buf->b_p_bt);
  }
#ifdef FEAT_FIND_ID
  clear_string_option(&buf->b_p_def);
  clear_string_option(&buf->b_p_inc);
#ifdef FEAT_EVAL
  clear_string_option(&buf->b_p_inex);
#endif
#endif
#if defined(FEAT_BEVAL) && defined(FEAT_EVAL)
  clear_string_option(&buf->b_p_bexpr);
#endif
#if defined(FEAT_CRYPT)
  clear_string_option(&buf->b_p_cm);
#endif
  clear_string_option(&buf->b_p_fp);
#if defined(FEAT_EVAL)
  clear_string_option(&buf->b_p_fex);
#endif
#ifdef FEAT_CRYPT
  clear_string_option(&buf->b_p_key);
#endif
  clear_string_option(&buf->b_p_kp);
  clear_string_option(&buf->b_p_mps);
  clear_string_option(&buf->b_p_fo);
  clear_string_option(&buf->b_p_flp);
  clear_string_option(&buf->b_p_isk);
#ifdef FEAT_VARTABS
  clear_string_option(&buf->b_p_vsts);
  vim_free(buf->b_p_vsts_nopaste);
  buf->b_p_vsts_nopaste = NULL;
  vim_free(buf->b_p_vsts_array);
  buf->b_p_vsts_array = NULL;
  clear_string_option(&buf->b_p_vts);
  VIM_CLEAR(buf->b_p_vts_array);
#endif
#ifdef FEAT_KEYMAP
  clear_string_option(&buf->b_p_keymap);
  keymap_clear(&buf->b_kmap_ga);
  ga_clear(&buf->b_kmap_ga);
#endif
#ifdef FEAT_COMMENTS
  clear_string_option(&buf->b_p_com);
#endif
#ifdef FEAT_FOLDING
  clear_string_option(&buf->b_p_cms);
#endif
  clear_string_option(&buf->b_p_nf);
#ifdef FEAT_SEARCHPATH
  clear_string_option(&buf->b_p_sua);
#endif
  clear_string_option(&buf->b_p_ft);
#if defined(FEAT_SMARTINDENT)
  clear_string_option(&buf->b_p_cinw);
#endif
#ifdef FEAT_QUICKFIX
  clear_string_option(&buf->b_p_gp);
  clear_string_option(&buf->b_p_mp);
  clear_string_option(&buf->b_p_efm);
#endif
  clear_string_option(&buf->b_p_ep);
  clear_string_option(&buf->b_p_path);
  clear_string_option(&buf->b_p_tags);
  clear_string_option(&buf->b_p_tc);
#ifdef FEAT_EVAL
  clear_string_option(&buf->b_p_tfu);
#endif
#ifdef FEAT_TEXTOBJ
  clear_string_option(&buf->b_p_qe);
#endif
  buf->b_p_ar = -1;
  buf->b_p_ul = NO_LOCAL_UNDOLEVEL;
  clear_string_option(&buf->b_p_bkc);
  clear_string_option(&buf->b_p_menc);
}

/*
 * Get alternate file "n".
 * Set linenr to "lnum" or altfpos.lnum if "lnum" == 0.
 *	Also set cursor column to altfpos.col if 'startofline' is not set.
 * if (options & GETF_SETMARK) call setpcmark()
 * if (options & GETF_ALT) we are jumping to an alternate file.
 * if (options & GETF_SWITCH) respect 'switchbuf' settings when jumping
 *
 * Return FAIL for failure, OK for success.
 */
int buflist_getfile(
    int n,
    linenr_T lnum,
    int options,
    int forceit)
{
  buf_T *buf;
  win_T *wp = NULL;
  pos_T *fpos;
  colnr_T col;

  buf = buflist_findnr(n);
  if (buf == NULL)
  {
    if ((options & GETF_ALT) && n == 0)
      emsg(_(e_noalt));
    else
      semsg(_("E92: Buffer %d not found"), n);
    return FAIL;
  }

  /* if alternate file is the current buffer, nothing to do */
  if (buf == curbuf)
    return OK;

  if (text_locked())
  {
    text_locked_msg();
    return FAIL;
  }
  if (curbuf_locked())
    return FAIL;

  /* altfpos may be changed by getfile(), get it now */
  if (lnum == 0)
  {
    fpos = buflist_findfpos(buf);
    lnum = fpos->lnum;
    col = fpos->col;
  }
  else
    col = 0;

  if (options & GETF_SWITCH)
  {
    /* If 'switchbuf' contains "useopen": jump to first window containing
	 * "buf" if one exists */
    if (swb_flags & SWB_USEOPEN)
      wp = buf_jump_open_win(buf);

    /* If 'switchbuf' contains "usetab": jump to first window in any tab
	 * page containing "buf" if one exists */
    if (wp == NULL && (swb_flags & SWB_USETAB))
      wp = buf_jump_open_tab(buf);

    /* If 'switchbuf' contains "split", "vsplit" or "newtab" and the
	 * current buffer isn't empty: open new tab or window */
    if (wp == NULL && (swb_flags & (SWB_VSPLIT | SWB_SPLIT | SWB_NEWTAB)) && !BUFEMPTY())
    {
      if (swb_flags & SWB_NEWTAB)
        tabpage_new();
      else if (win_split(0, (swb_flags & SWB_VSPLIT) ? WSP_VERT : 0) == FAIL)
        return FAIL;
      RESET_BINDING(curwin);
    }
  }

  ++RedrawingDisabled;
  if (GETFILE_SUCCESS(getfile(buf->b_fnum, NULL, NULL,
                              (options & GETF_SETMARK), lnum, forceit)))
  {
    --RedrawingDisabled;

    /* cursor is at to BOL and w_cursor.lnum is checked due to getfile() */
    if (!p_sol && col != 0)
    {
      curwin->w_cursor.col = col;
      check_cursor_col();
      curwin->w_cursor.coladd = 0;
      curwin->w_set_curswant = TRUE;
    }
    return OK;
  }
  --RedrawingDisabled;
  return FAIL;
}

/*
 * go to the last know line number for the current buffer
 */
void buflist_getfpos(void)
{
  pos_T *fpos;

  fpos = buflist_findfpos(curbuf);

  curwin->w_cursor.lnum = fpos->lnum;
  check_cursor_lnum();

  if (p_sol)
    curwin->w_cursor.col = 0;
  else
  {
    curwin->w_cursor.col = fpos->col;
    check_cursor_col();
    curwin->w_cursor.coladd = 0;
    curwin->w_set_curswant = TRUE;
  }
}

#if defined(FEAT_QUICKFIX) || defined(FEAT_EVAL) || defined(PROTO)
/*
 * Find file in buffer list by name (it has to be for the current window).
 * Returns NULL if not found.
 */
buf_T *
buflist_findname_exp(char_u *fname)
{
  char_u *ffname;
  buf_T *buf = NULL;

  /* First make the name into a full path name */
  ffname = FullName_save(fname,
#ifdef UNIX
                         TRUE /* force expansion, get rid of symbolic links */
#else
                         FALSE
#endif
  );
  if (ffname != NULL)
  {
    buf = buflist_findname(ffname);
    vim_free(ffname);
  }
  return buf;
}
#endif

/*
 * Find file in buffer list by name (it has to be for the current window).
 * "ffname" must have a full path.
 * Skips dummy buffers.
 * Returns NULL if not found.
 */
buf_T *
buflist_findname(char_u *ffname)
{
#ifdef UNIX
  stat_T st;

  if (mch_stat((char *)ffname, &st) < 0)
    st.st_dev = (dev_T)-1;
  return buflist_findname_stat(ffname, &st);
}

/*
 * Same as buflist_findname(), but pass the stat structure to avoid getting it
 * twice for the same file.
 * Returns NULL if not found.
 */
static buf_T *
buflist_findname_stat(
    char_u *ffname,
    stat_T *stp)
{
#endif
  buf_T *buf;

  /* Start at the last buffer, expect to find a match sooner. */
  for (buf = lastbuf; buf != NULL; buf = buf->b_prev)
    if ((buf->b_flags & BF_DUMMY) == 0 && !otherfile_buf(buf, ffname
#ifdef UNIX
                                                         ,
                                                         stp
#endif
                                                         ))
      return buf;
  return NULL;
}

/*
 * Find file in buffer list by a regexp pattern.
 * Return fnum of the found buffer.
 * Return < 0 for error.
 */
int buflist_findpat(
    char_u *pattern,
    char_u *pattern_end, /* pointer to first char after pattern */
    int unlisted,        /* find unlisted buffers */
    int diffmode UNUSED, /* find diff-mode buffers only */
    int curtab_only)     /* find buffers in current tab only */
{
  buf_T *buf;
  int match = -1;
  int find_listed;
  char_u *pat;
  char_u *patend;
  int attempt;
  char_u *p;
  int toggledollar;

  if (pattern_end == pattern + 1 && (*pattern == '%' || *pattern == '#'))
  {
    if (*pattern == '%')
      match = curbuf->b_fnum;
    else
      match = curwin->w_alt_fnum;
#ifdef FEAT_DIFF
    if (diffmode && !diff_mode_buf(buflist_findnr(match)))
      match = -1;
#endif
  }

  /*
     * Try four ways of matching a listed buffer:
     * attempt == 0: without '^' or '$' (at any position)
     * attempt == 1: with '^' at start (only at position 0)
     * attempt == 2: with '$' at end (only match at end)
     * attempt == 3: with '^' at start and '$' at end (only full match)
     * Repeat this for finding an unlisted buffer if there was no matching
     * listed buffer.
     */
  else
  {
    pat = file_pat_to_reg_pat(pattern, pattern_end, NULL, FALSE);
    if (pat == NULL)
      return -1;
    patend = pat + STRLEN(pat) - 1;
    toggledollar = (patend > pat && *patend == '$');

    /* First try finding a listed buffer.  If not found and "unlisted"
	 * is TRUE, try finding an unlisted buffer. */
    find_listed = TRUE;
    for (;;)
    {
      for (attempt = 0; attempt <= 3; ++attempt)
      {
        regmatch_T regmatch;

        /* may add '^' and '$' */
        if (toggledollar)
          *patend = (attempt < 2) ? NUL : '$'; /* add/remove '$' */
        p = pat;
        if (*p == '^' && !(attempt & 1)) /* add/remove '^' */
          ++p;
        regmatch.regprog = vim_regcomp(p, p_magic ? RE_MAGIC : 0);
        if (regmatch.regprog == NULL)
        {
          vim_free(pat);
          return -1;
        }

        for (buf = lastbuf; buf != NULL; buf = buf->b_prev)
          if (buf->b_p_bl == find_listed
#ifdef FEAT_DIFF
              && (!diffmode || diff_mode_buf(buf))
#endif
              && buflist_match(&regmatch, buf, FALSE) != NULL)
          {
            if (curtab_only)
            {
              /* Ignore the match if the buffer is not open in
			     * the current tab. */
              win_T *wp;

              FOR_ALL_WINDOWS(wp)
              if (wp->w_buffer == buf)
                break;
              if (wp == NULL)
                continue;
            }
            if (match >= 0) /* already found a match */
            {
              match = -2;
              break;
            }
            match = buf->b_fnum; /* remember first match */
          }

        vim_regfree(regmatch.regprog);
        if (match >= 0) /* found one match */
          break;
      }

      /* Only search for unlisted buffers if there was no match with
	     * a listed buffer. */
      if (!unlisted || !find_listed || match != -1)
        break;
      find_listed = FALSE;
    }

    vim_free(pat);
  }

  if (match == -2)
    semsg(_("E93: More than one match for %s"), pattern);
  else if (match < 0)
    semsg(_("E94: No matching buffer for %s"), pattern);
  return match;
}

#if defined(FEAT_CMDL_COMPL) || defined(PROTO)

/*
 * Find all buffer names that match.
 * For command line expansion of ":buf" and ":sbuf".
 * Return OK if matches found, FAIL otherwise.
 */
int ExpandBufnames(
    char_u *pat,
    int *num_file,
    char_u ***file,
    int options)
{
  int count = 0;
  buf_T *buf;
  int round;
  char_u *p;
  int attempt;
  char_u *patc;

  *num_file = 0; /* return values in case of FAIL */
  *file = NULL;

  /* Make a copy of "pat" and change "^" to "\(^\|[\/]\)". */
  if (*pat == '^')
  {
    patc = alloc(STRLEN(pat) + 11);
    if (patc == NULL)
      return FAIL;
    STRCPY(patc, "\\(^\\|[\\/]\\)");
    STRCPY(patc + 11, pat + 1);
  }
  else
    patc = pat;

  /*
     * attempt == 0: try match with    '\<', match at start of word
     * attempt == 1: try match without '\<', match anywhere
     */
  for (attempt = 0; attempt <= 1; ++attempt)
  {
    regmatch_T regmatch;

    if (attempt > 0 && patc == pat)
      break; /* there was no anchor, no need to try again */
    regmatch.regprog = vim_regcomp(patc + attempt * 11, RE_MAGIC);
    if (regmatch.regprog == NULL)
    {
      if (patc != pat)
        vim_free(patc);
      return FAIL;
    }

    /*
	 * round == 1: Count the matches.
	 * round == 2: Build the array to keep the matches.
	 */
    for (round = 1; round <= 2; ++round)
    {
      count = 0;
      FOR_ALL_BUFFERS(buf)
      {
        if (!buf->b_p_bl) /* skip unlisted buffers */
          continue;
        p = buflist_match(&regmatch, buf, p_wic);
        if (p != NULL)
        {
          if (round == 1)
            ++count;
          else
          {
            if (options & WILD_HOME_REPLACE)
              p = home_replace_save(buf, p);
            else
              p = vim_strsave(p);
            (*file)[count++] = p;
          }
        }
      }
      if (count == 0) /* no match found, break here */
        break;
      if (round == 1)
      {
        *file = ALLOC_MULT(char_u *, count);
        if (*file == NULL)
        {
          vim_regfree(regmatch.regprog);
          if (patc != pat)
            vim_free(patc);
          return FAIL;
        }
      }
    }
    vim_regfree(regmatch.regprog);
    if (count) /* match(es) found, break here */
      break;
  }

  if (patc != pat)
    vim_free(patc);

  *num_file = count;
  return (count == 0 ? FAIL : OK);
}

#endif /* FEAT_CMDL_COMPL */

/*
 * Check for a match on the file name for buffer "buf" with regprog "prog".
 */
static char_u *
buflist_match(
    regmatch_T *rmp,
    buf_T *buf,
    int ignore_case) /* when TRUE ignore case, when FALSE use 'fic' */
{
  char_u *match;

  /* First try the short file name, then the long file name. */
  match = fname_match(rmp, buf->b_sfname, ignore_case);
  if (match == NULL)
    match = fname_match(rmp, buf->b_ffname, ignore_case);

  return match;
}

/*
 * Try matching the regexp in "prog" with file name "name".
 * Return "name" when there is a match, NULL when not.
 */
static char_u *
fname_match(
    regmatch_T *rmp,
    char_u *name,
    int ignore_case) /* when TRUE ignore case, when FALSE use 'fic' */
{
  char_u *match = NULL;
  char_u *p;

  if (name != NULL)
  {
    /* Ignore case when 'fileignorecase' or the argument is set. */
    rmp->rm_ic = p_fic || ignore_case;
    if (vim_regexec(rmp, name, (colnr_T)0))
      match = name;
    else
    {
      /* Replace $(HOME) with '~' and try matching again. */
      p = home_replace_save(NULL, name);
      if (p != NULL && vim_regexec(rmp, p, (colnr_T)0))
        match = name;
      vim_free(p);
    }
  }

  return match;
}

/*
 * Find a file in the buffer list by buffer number.
 */
buf_T *
buflist_findnr(int nr)
{
  char_u key[VIM_SIZEOF_INT * 2 + 1];
  hashitem_T *hi;

  if (nr == 0)
    nr = curwin->w_alt_fnum;
  sprintf((char *)key, "%x", nr);
  hi = hash_find(&buf_hashtab, key);

  if (!HASHITEM_EMPTY(hi))
    return (buf_T *)(hi->hi_key - ((unsigned)(curbuf->b_key - (char_u *)curbuf)));
  return NULL;
}

/*
 * Get name of file 'n' in the buffer list.
 * When the file has no name an empty string is returned.
 * home_replace() is used to shorten the file name (used for marks).
 * Returns a pointer to allocated memory, of NULL when failed.
 */
char_u *
buflist_nr2name(
    int n,
    int fullname,
    int helptail) /* for help buffers return tail only */
{
  buf_T *buf;

  buf = buflist_findnr(n);
  if (buf == NULL)
    return NULL;
  return home_replace_save(helptail ? buf : NULL,
                           fullname ? buf->b_ffname : buf->b_fname);
}

/*
 * Set the "lnum" and "col" for the buffer "buf" and the current window.
 * When "copy_options" is TRUE save the local window option values.
 * When "lnum" is 0 only do the options.
 */
static void
buflist_setfpos(
    buf_T *buf,
    win_T *win,
    linenr_T lnum,
    colnr_T col,
    int copy_options)
{
  wininfo_T *wip;

  for (wip = buf->b_wininfo; wip != NULL; wip = wip->wi_next)
    if (wip->wi_win == win)
      break;
  if (wip == NULL)
  {
    /* allocate a new entry */
    wip = ALLOC_CLEAR_ONE(wininfo_T);
    if (wip == NULL)
      return;
    wip->wi_win = win;
    if (lnum == 0) /* set lnum even when it's 0 */
      lnum = 1;
  }
  else
  {
    /* remove the entry from the list */
    if (wip->wi_prev)
      wip->wi_prev->wi_next = wip->wi_next;
    else
      buf->b_wininfo = wip->wi_next;
    if (wip->wi_next)
      wip->wi_next->wi_prev = wip->wi_prev;
    if (copy_options && wip->wi_optset)
    {
      clear_winopt(&wip->wi_opt);
#ifdef FEAT_FOLDING
      deleteFoldRecurse(&wip->wi_folds);
#endif
    }
  }
  if (lnum != 0)
  {
    wip->wi_fpos.lnum = lnum;
    wip->wi_fpos.col = col;
  }
  if (copy_options)
  {
    /* Save the window-specific option values. */
    copy_winopt(&win->w_onebuf_opt, &wip->wi_opt);
#ifdef FEAT_FOLDING
    wip->wi_fold_manual = win->w_fold_manual;
    cloneFoldGrowArray(&win->w_folds, &wip->wi_folds);
#endif
    wip->wi_optset = TRUE;
  }

  /* insert the entry in front of the list */
  wip->wi_next = buf->b_wininfo;
  buf->b_wininfo = wip;
  wip->wi_prev = NULL;
  if (wip->wi_next)
    wip->wi_next->wi_prev = wip;

  return;
}

#ifdef FEAT_DIFF
/*
 * Return TRUE when "wip" has 'diff' set and the diff is only for another tab
 * page.  That's because a diff is local to a tab page.
 */
static int
wininfo_other_tab_diff(wininfo_T *wip)
{
  win_T *wp;

  if (wip->wi_opt.wo_diff)
  {
    FOR_ALL_WINDOWS(wp)
    /* return FALSE when it's a window in the current tab page, thus
	     * the buffer was in diff mode here */
    if (wip->wi_win == wp)
      return FALSE;
    return TRUE;
  }
  return FALSE;
}
#endif

/*
 * Find info for the current window in buffer "buf".
 * If not found, return the info for the most recently used window.
 * When "skip_diff_buffer" is TRUE avoid windows with 'diff' set that is in
 * another tab page.
 * Returns NULL when there isn't any info.
 */
static wininfo_T *
find_wininfo(
    buf_T *buf,
    int skip_diff_buffer UNUSED)
{
  wininfo_T *wip;

  for (wip = buf->b_wininfo; wip != NULL; wip = wip->wi_next)
    if (wip->wi_win == curwin
#ifdef FEAT_DIFF
        && (!skip_diff_buffer || !wininfo_other_tab_diff(wip))
#endif
    )
      break;

  /* If no wininfo for curwin, use the first in the list (that doesn't have
     * 'diff' set and is in another tab page). */
  if (wip == NULL)
  {
#ifdef FEAT_DIFF
    if (skip_diff_buffer)
    {
      for (wip = buf->b_wininfo; wip != NULL; wip = wip->wi_next)
        if (!wininfo_other_tab_diff(wip))
          break;
    }
    else
#endif
      wip = buf->b_wininfo;
  }
  return wip;
}

/*
 * Reset the local window options to the values last used in this window.
 * If the buffer wasn't used in this window before, use the values from
 * the most recently used window.  If the values were never set, use the
 * global values for the window.
 */
void get_winopts(buf_T *buf)
{
  wininfo_T *wip;

  clear_winopt(&curwin->w_onebuf_opt);
#ifdef FEAT_FOLDING
  clearFolding(curwin);
#endif

  wip = find_wininfo(buf, TRUE);
  if (wip != NULL && wip->wi_win != NULL && wip->wi_win != curwin && wip->wi_win->w_buffer == buf)
  {
    /* The buffer is currently displayed in the window: use the actual
	 * option values instead of the saved (possibly outdated) values. */
    win_T *wp = wip->wi_win;

    copy_winopt(&wp->w_onebuf_opt, &curwin->w_onebuf_opt);
#ifdef FEAT_FOLDING
    curwin->w_fold_manual = wp->w_fold_manual;
    curwin->w_foldinvalid = TRUE;
    cloneFoldGrowArray(&wp->w_folds, &curwin->w_folds);
#endif
  }
  else if (wip != NULL && wip->wi_optset)
  {
    /* the buffer was displayed in the current window earlier */
    copy_winopt(&wip->wi_opt, &curwin->w_onebuf_opt);
#ifdef FEAT_FOLDING
    curwin->w_fold_manual = wip->wi_fold_manual;
    curwin->w_foldinvalid = TRUE;
    cloneFoldGrowArray(&wip->wi_folds, &curwin->w_folds);
#endif
  }
  else
    copy_winopt(&curwin->w_allbuf_opt, &curwin->w_onebuf_opt);

#ifdef FEAT_FOLDING
  /* Set 'foldlevel' to 'foldlevelstart' if it's not negative. */
  if (p_fdls >= 0)
    curwin->w_p_fdl = p_fdls;
#endif
}

/*
 * Find the position (lnum and col) for the buffer 'buf' for the current
 * window.
 * Returns a pointer to no_position if no position is found.
 */
pos_T *
buflist_findfpos(buf_T *buf)
{
  wininfo_T *wip;
  static pos_T no_position = {1, 0, 0};

  wip = find_wininfo(buf, FALSE);
  if (wip != NULL)
    return &(wip->wi_fpos);
  else
    return &no_position;
}

/*
 * Find the lnum for the buffer 'buf' for the current window.
 */
linenr_T
buflist_findlnum(buf_T *buf)
{
  return buflist_findfpos(buf)->lnum;
}

/*
 * List all known file names (for :files and :buffers command).
 */
void buflist_list(exarg_T *eap)
{
  buf_T *buf;
  int len;
  int i;
  int ro_char;
  int changed_char;
#ifdef FEAT_TERMINAL
  int job_running;
  int job_none_open;
#endif

  for (buf = firstbuf; buf != NULL && !got_int; buf = buf->b_next)
  {
#ifdef FEAT_TERMINAL
    job_running = term_job_running(buf->b_term);
    job_none_open = job_running && term_none_open(buf->b_term);
#endif
    /* skip unlisted buffers, unless ! was used */
    if ((!buf->b_p_bl && !eap->forceit && !vim_strchr(eap->arg, 'u')) || (vim_strchr(eap->arg, 'u') && buf->b_p_bl) || (vim_strchr(eap->arg, '+') && ((buf->b_flags & BF_READERR) || !bufIsChanged(buf))) || (vim_strchr(eap->arg, 'a') && (buf->b_ml.ml_mfp == NULL || buf->b_nwindows == 0)) || (vim_strchr(eap->arg, 'h') && (buf->b_ml.ml_mfp == NULL || buf->b_nwindows != 0))
#ifdef FEAT_TERMINAL
        || (vim_strchr(eap->arg, 'R') && (!job_running || (job_running && job_none_open))) || (vim_strchr(eap->arg, '?') && (!job_running || (job_running && !job_none_open))) || (vim_strchr(eap->arg, 'F') && (job_running || buf->b_term == NULL))
#endif
        || (vim_strchr(eap->arg, '-') && buf->b_p_ma) || (vim_strchr(eap->arg, '=') && !buf->b_p_ro) || (vim_strchr(eap->arg, 'x') && !(buf->b_flags & BF_READERR)) || (vim_strchr(eap->arg, '%') && buf != curbuf) || (vim_strchr(eap->arg, '#') && (buf == curbuf || curwin->w_alt_fnum != buf->b_fnum)))
      continue;
    if (buf_spname(buf) != NULL)
      vim_strncpy(NameBuff, buf_spname(buf), MAXPATHL - 1);
    else
      home_replace(buf, buf->b_fname, NameBuff, MAXPATHL, TRUE);
    if (message_filtered(NameBuff))
      continue;

    changed_char = (buf->b_flags & BF_READERR) ? 'x'
                                               : (bufIsChanged(buf) ? '+' : ' ');
#ifdef FEAT_TERMINAL
    if (term_job_running(buf->b_term))
    {
      if (term_none_open(buf->b_term))
        ro_char = '?';
      else
        ro_char = 'R';
      changed_char = ' '; /* bufIsChanged() returns TRUE to avoid
				  * closing, but it's not actually changed. */
    }
    else if (buf->b_term != NULL)
      ro_char = 'F';
    else
#endif
      ro_char = !buf->b_p_ma ? '-' : (buf->b_p_ro ? '=' : ' ');

    msg_T *msg = msg2_create(MSG_INFO);

    msg2_put("\n", msg);
    len = vim_snprintf((char *)IObuff, IOSIZE - 20, "%3d%c%c%c%c%c \"%s\"",
                       buf->b_fnum,
                       buf->b_p_bl ? ' ' : 'u',
                       buf == curbuf ? '%' : (curwin->w_alt_fnum == buf->b_fnum ? '#' : ' '),
                       buf->b_ml.ml_mfp == NULL ? ' ' : (buf->b_nwindows == 0 ? 'h' : 'a'),
                       ro_char,
                       changed_char,
                       NameBuff);
    if (len > IOSIZE - 20)
      len = IOSIZE - 20;

    /* put "line 999" in column 40 or after the file name */
    i = 40 - vim_strsize(IObuff);
    do
      IObuff[len++] = ' ';
    while (--i > 0 && len < IOSIZE - 18);
    vim_snprintf((char *)IObuff + len, (size_t)(IOSIZE - len),
                 _("line %ld"), buf == curbuf ? curwin->w_cursor.lnum : (long)buflist_findlnum(buf));
    msg2_put(IObuff, msg);
    msg2_send(msg);
    msg2_free(msg);
  }
}

/*
 * Get file name and line number for file 'fnum'.
 * Used by DoOneCmd() for translating '%' and '#'.
 * Used by insert_reg() and cmdline_paste() for '#' register.
 * Return FAIL if not found, OK for success.
 */
int buflist_name_nr(
    int fnum,
    char_u **fname,
    linenr_T *lnum)
{
  buf_T *buf;

  buf = buflist_findnr(fnum);
  if (buf == NULL || buf->b_fname == NULL)
    return FAIL;

  *fname = buf->b_fname;
  *lnum = buflist_findlnum(buf);

  return OK;
}

/*
 * Set the file name for "buf"' to "ffname_arg", short file name to
 * "sfname_arg".
 * The file name with the full path is also remembered, for when :cd is used.
 * Returns FAIL for failure (file name already in use by other buffer)
 *	OK otherwise.
 */
int setfname(
    buf_T *buf,
    char_u *ffname_arg,
    char_u *sfname_arg,
    int message) /* give message when buffer already exists */
{
  char_u *ffname = ffname_arg;
  char_u *sfname = sfname_arg;
  buf_T *obuf = NULL;
#ifdef UNIX
  stat_T st;
#endif

  if (ffname == NULL || *ffname == NUL)
  {
    /* Removing the name. */
    if (buf->b_sfname != buf->b_ffname)
      VIM_CLEAR(buf->b_sfname);
    else
      buf->b_sfname = NULL;
    VIM_CLEAR(buf->b_ffname);
#ifdef UNIX
    st.st_dev = (dev_T)-1;
#endif
  }
  else
  {
    fname_expand(buf, &ffname, &sfname); /* will allocate ffname */
    if (ffname == NULL)                  /* out of memory */
      return FAIL;

      /*
	 * if the file name is already used in another buffer:
	 * - if the buffer is loaded, fail
	 * - if the buffer is not loaded, delete it from the list
	 */
#ifdef UNIX
    if (mch_stat((char *)ffname, &st) < 0)
      st.st_dev = (dev_T)-1;
#endif
    if (!(buf->b_flags & BF_DUMMY))
#ifdef UNIX
      obuf = buflist_findname_stat(ffname, &st);
#else
      obuf = buflist_findname(ffname);
#endif
    if (obuf != NULL && obuf != buf)
    {
      if (obuf->b_ml.ml_mfp != NULL) /* it's loaded, fail */
      {
        if (message)
          emsg(_("E95: Buffer with this name already exists"));
        vim_free(ffname);
        return FAIL;
      }
      /* delete from the list */
      close_buffer(NULL, obuf, DOBUF_WIPE, FALSE);
    }
    sfname = vim_strsave(sfname);
    if (ffname == NULL || sfname == NULL)
    {
      vim_free(sfname);
      vim_free(ffname);
      return FAIL;
    }
#ifdef USE_FNAME_CASE
    fname_case(sfname, 0); /* set correct case for short file name */
#endif
    if (buf->b_sfname != buf->b_ffname)
      vim_free(buf->b_sfname);
    vim_free(buf->b_ffname);
    buf->b_ffname = ffname;
    buf->b_sfname = sfname;
  }
  buf->b_fname = buf->b_sfname;
#ifdef UNIX
  if (st.st_dev == (dev_T)-1)
    buf->b_dev_valid = FALSE;
  else
  {
    buf->b_dev_valid = TRUE;
    buf->b_dev = st.st_dev;
    buf->b_ino = st.st_ino;
  }
#endif

  buf->b_shortname = FALSE;

  buf_name_changed(buf);
  return OK;
}

/*
 * Crude way of changing the name of a buffer.  Use with care!
 * The name should be relative to the current directory.
 */
void buf_set_name(int fnum, char_u *name)
{
  buf_T *buf;

  buf = buflist_findnr(fnum);
  if (buf != NULL)
  {
    if (buf->b_sfname != buf->b_ffname)
      vim_free(buf->b_sfname);
    vim_free(buf->b_ffname);
    buf->b_ffname = vim_strsave(name);
    buf->b_sfname = NULL;
    /* Allocate ffname and expand into full path.  Also resolves .lnk
	 * files on Win32. */
    fname_expand(buf, &buf->b_ffname, &buf->b_sfname);
    buf->b_fname = buf->b_sfname;
  }
}

/*
 * Take care of what needs to be done when the name of buffer "buf" has
 * changed.
 */
void buf_name_changed(buf_T *buf)
{
  /*
     * If the file name changed, also change the name of the swapfile
     */
  if (buf->b_ml.ml_mfp != NULL)
    ml_setname(buf);

  if (curwin->w_buffer == buf)
    check_arg_idx(curwin); /* check file name for arg list */
  status_redraw_all();     /* status lines need to be redrawn */
  fmarks_check_names(buf); /* check named file marks */
  ml_timestamp(buf);       /* reset timestamp */
}

/*
 * set alternate file name for current window
 *
 * Used by do_one_cmd(), do_write() and do_ecmd().
 * Return the buffer.
 */
buf_T *
setaltfname(
    char_u *ffname,
    char_u *sfname,
    linenr_T lnum)
{
  buf_T *buf;

  /* Create a buffer.  'buflisted' is not set if it's a new buffer */
  buf = buflist_new(ffname, sfname, lnum, 0);
  if (buf != NULL && !cmdmod.keepalt)
    curwin->w_alt_fnum = buf->b_fnum;
  return buf;
}

/*
 * Get alternate file name for current window.
 * Return NULL if there isn't any, and give error message if requested.
 */
char_u *
getaltfname(
    int errmsg) /* give error message */
{
  char_u *fname;
  linenr_T dummy;

  if (buflist_name_nr(0, &fname, &dummy) == FAIL)
  {
    if (errmsg)
      emsg(_(e_noalt));
    return NULL;
  }
  return fname;
}

/*
 * Add a file name to the buflist and return its number.
 * Uses same flags as buflist_new(), except BLN_DUMMY.
 *
 * used by qf_init(), main() and doarglist()
 */
int buflist_add(char_u *fname, int flags)
{
  buf_T *buf;

  buf = buflist_new(fname, NULL, (linenr_T)0, flags);
  if (buf != NULL)
    return buf->b_fnum;
  return 0;
}

#if defined(BACKSLASH_IN_FILENAME) || defined(PROTO)
/*
 * Adjust slashes in file names.  Called after 'shellslash' was set.
 */
void buflist_slash_adjust(void)
{
  buf_T *bp;

  FOR_ALL_BUFFERS(bp)
  {
    if (bp->b_ffname != NULL)
      slash_adjust(bp->b_ffname);
    if (bp->b_sfname != NULL)
      slash_adjust(bp->b_sfname);
  }
}
#endif

/*
 * Set alternate cursor position for the current buffer and window "win".
 * Also save the local window option values.
 */
void buflist_altfpos(win_T *win)
{
  buflist_setfpos(curbuf, win, win->w_cursor.lnum, win->w_cursor.col, TRUE);
}

/*
 * Return TRUE if 'ffname' is not the same file as current file.
 * Fname must have a full path (expanded by mch_FullName()).
 */
int otherfile(char_u *ffname)
{
  return otherfile_buf(curbuf, ffname
#ifdef UNIX
                       ,
                       NULL
#endif
  );
}

static int
otherfile_buf(
    buf_T *buf,
    char_u *ffname
#ifdef UNIX
    ,
    stat_T *stp
#endif
)
{
  /* no name is different */
  if (ffname == NULL || *ffname == NUL || buf->b_ffname == NULL)
    return TRUE;
  if (fnamecmp(ffname, buf->b_ffname) == 0)
    return FALSE;
#ifdef UNIX
  {
    stat_T st;

    /* If no stat_T given, get it now */
    if (stp == NULL)
    {
      if (!buf->b_dev_valid || mch_stat((char *)ffname, &st) < 0)
        st.st_dev = (dev_T)-1;
      stp = &st;
    }
    /* Use dev/ino to check if the files are the same, even when the names
	 * are different (possible with links).  Still need to compare the
	 * name above, for when the file doesn't exist yet.
	 * Problem: The dev/ino changes when a file is deleted (and created
	 * again) and remains the same when renamed/moved.  We don't want to
	 * mch_stat() each buffer each time, that would be too slow.  Get the
	 * dev/ino again when they appear to match, but not when they appear
	 * to be different: Could skip a buffer when it's actually the same
	 * file. */
    if (buf_same_ino(buf, stp))
    {
      buf_setino(buf);
      if (buf_same_ino(buf, stp))
        return FALSE;
    }
  }
#endif
  return TRUE;
}

#if defined(UNIX) || defined(PROTO)
/*
 * Set inode and device number for a buffer.
 * Must always be called when b_fname is changed!.
 */
void buf_setino(buf_T *buf)
{
  stat_T st;

  if (buf->b_fname != NULL && mch_stat((char *)buf->b_fname, &st) >= 0)
  {
    buf->b_dev_valid = TRUE;
    buf->b_dev = st.st_dev;
    buf->b_ino = st.st_ino;
  }
  else
    buf->b_dev_valid = FALSE;
}

/*
 * Return TRUE if dev/ino in buffer "buf" matches with "stp".
 */
static int
buf_same_ino(
    buf_T *buf,
    stat_T *stp)
{
  return (buf->b_dev_valid && stp->st_dev == buf->b_dev && stp->st_ino == buf->b_ino);
}
#endif

/*
 * Print info about the current buffer.
 */
void fileinfo(
    int fullname, /* when non-zero print full path */
    int shorthelp,
    int dont_truncate)
{
  char_u *name;
  int n;
  char *p;
  char *buffer;
  size_t len;

  buffer = alloc(IOSIZE);
  if (buffer == NULL)
    return;

  if (fullname > 1) /* 2 CTRL-G: include buffer number */
  {
    vim_snprintf(buffer, IOSIZE, "buf %d: ", curbuf->b_fnum);
    p = buffer + STRLEN(buffer);
  }
  else
    p = buffer;

  *p++ = '"';
  if (buf_spname(curbuf) != NULL)
    vim_strncpy((char_u *)p, buf_spname(curbuf), IOSIZE - (p - buffer) - 1);
  else
  {
    if (!fullname && curbuf->b_fname != NULL)
      name = curbuf->b_fname;
    else
      name = curbuf->b_ffname;
    home_replace(shorthelp ? curbuf : NULL, name, (char_u *)p,
                 (int)(IOSIZE - (p - buffer)), TRUE);
  }

  vim_snprintf_add(buffer, IOSIZE, "\"%s%s%s%s%s%s",
                   curbufIsChanged() ? (shortmess(SHM_MOD)
                                            ? " [+]"
                                            : _(" [Modified]"))
                                     : " ",
                   (curbuf->b_flags & BF_NOTEDITED)
#ifdef FEAT_QUICKFIX
                           && !bt_dontwrite(curbuf)
#endif
                       ? _("[Not edited]")
                       : "",
                   (curbuf->b_flags & BF_NEW)
#ifdef FEAT_QUICKFIX
                           && !bt_dontwrite(curbuf)
#endif
                       ? _("[New file]")
                       : "",
                   (curbuf->b_flags & BF_READERR) ? _("[Read errors]") : "",
                   curbuf->b_p_ro ? (shortmess(SHM_RO) ? _("[RO]")
                                                       : _("[readonly]"))
                                  : "",
                   (curbufIsChanged() || (curbuf->b_flags & BF_WRITE_MASK) || curbuf->b_p_ro) ? " " : "");
  /* With 32 bit longs and more than 21,474,836 lines multiplying by 100
     * causes an overflow, thus for large numbers divide instead. */
  if (curwin->w_cursor.lnum > 1000000L)
    n = (int)(((long)curwin->w_cursor.lnum) /
              ((long)curbuf->b_ml.ml_line_count / 100L));
  else
    n = (int)(((long)curwin->w_cursor.lnum * 100L) /
              (long)curbuf->b_ml.ml_line_count);
  if (curbuf->b_ml.ml_flags & ML_EMPTY)
    vim_snprintf_add(buffer, IOSIZE, "%s", _(no_lines_msg));
  else
  {
    vim_snprintf_add(buffer, IOSIZE,
                     _("line %ld of %ld --%d%%-- col "),
                     (long)curwin->w_cursor.lnum,
                     (long)curbuf->b_ml.ml_line_count,
                     n);
    validate_virtcol();
    len = STRLEN(buffer);
    col_print((char_u *)buffer + len, IOSIZE - len,
              (int)curwin->w_cursor.col + 1, (int)curwin->w_virtcol + 1);
  }

  (void)append_arg_number(curwin, (char_u *)buffer, IOSIZE,
                          !shortmess(SHM_FILE));

  if (dont_truncate)
  {
    /* Temporarily set msg_scroll to avoid the message being truncated.
	 * First call msg_start() to get the message in the right place. */
    msg_start();
    n = msg_scroll;
    msg_scroll = TRUE;
    msg(buffer);
    msg_scroll = n;
  }
  else
  {
    p = (char *)msg_trunc_attr(buffer, FALSE, 0);
    if (restart_edit != 0 || (msg_scrolled && !need_wait_return))
      /* Need to repeat the message after redrawing when:
	     * - When restart_edit is set (otherwise there will be a delay
	     *   before redrawing).
	     * - When the screen was scrolled but there is no wait-return
	     *   prompt. */
      set_keep_msg((char_u *)p, 0);
  }

  vim_free(buffer);
}

void col_print(
    char_u *buf,
    size_t buflen,
    int col,
    int vcol)
{
  if (col == vcol)
    vim_snprintf((char *)buf, buflen, "%d", col);
  else
    vim_snprintf((char *)buf, buflen, "%d-%d", col, vcol);
}

/*
 * Append (file 2 of 8) to "buf[buflen]", if editing more than one file.
 * Return TRUE if it was appended.
 */
static int
append_arg_number(
    win_T *wp,
    char_u *buf,
    int buflen,
    int add_file) /* Add "file" before the arg number */
{
  char_u *p;

  if (ARGCOUNT <= 1) /* nothing to do */
    return FALSE;

  p = buf + STRLEN(buf);      /* go to the end of the buffer */
  if (p - buf + 35 >= buflen) /* getting too long */
    return FALSE;
  *p++ = ' ';
  *p++ = '(';
  if (add_file)
  {
    STRCPY(p, "file ");
    p += 5;
  }
  vim_snprintf((char *)p, (size_t)(buflen - (p - buf)),
               wp->w_arg_idx_invalid ? "(%d) of %d)"
                                     : "%d of %d)",
               wp->w_arg_idx + 1, ARGCOUNT);
  return TRUE;
}

/*
 * If fname is not a full path, make it a full path.
 * Returns pointer to allocated memory (NULL for failure).
 */
char_u *
fix_fname(char_u *fname)
{
  /*
     * Force expanding the path always for Unix, because symbolic links may
     * mess up the full path name, even though it starts with a '/'.
     * Also expand when there is ".." in the file name, try to remove it,
     * because "c:/src/../README" is equal to "c:/README".
     * Similarly "c:/src//file" is equal to "c:/src/file".
     * For MS-Windows also expand names like "longna~1" to "longname".
     */
#ifdef UNIX
  return FullName_save(fname, TRUE);
#else
  if (!vim_isAbsName(fname) || strstr((char *)fname, "..") != NULL || strstr((char *)fname, "//") != NULL
#ifdef BACKSLASH_IN_FILENAME
      || strstr((char *)fname, "\\\\") != NULL
#endif
#if defined(MSWIN)
      || vim_strchr(fname, '~') != NULL
#endif
  )
    return FullName_save(fname, FALSE);

  fname = vim_strsave(fname);

#ifdef USE_FNAME_CASE
  if (fname != NULL)
    fname_case(fname, 0); /* set correct case for file name */
#endif

  return fname;
#endif
}

/*
 * Make "*ffname" a full file name, set "*sfname" to "*ffname" if not NULL.
 * "*ffname" becomes a pointer to allocated memory (or NULL).
 * When resolving a link both "*sfname" and "*ffname" will point to the same
 * allocated memory.
 * The "*ffname" and "*sfname" pointer values on call will not be freed.
 * Note that the resulting "*ffname" pointer should be considered not allocaed.
 */
void fname_expand(
    buf_T *buf UNUSED,
    char_u **ffname,
    char_u **sfname)
{
  if (*ffname == NULL) // no file name given, nothing to do
    return;
  if (*sfname == NULL) // no short file name given, use ffname
    *sfname = *ffname;
  *ffname = fix_fname(*ffname); // expand to full path

#ifdef FEAT_SHORTCUT
  if (!buf->b_p_bin)
  {
    char_u *rfname;

    // If the file name is a shortcut file, use the file it links to.
    rfname = mch_resolve_path(*ffname, FALSE);
    if (rfname != NULL)
    {
      vim_free(*ffname);
      *ffname = rfname;
      *sfname = rfname;
    }
  }
#endif
}

/*
 * Get the file name for an argument list entry.
 */
char_u *
alist_name(aentry_T *aep)
{
  buf_T *bp;

  /* Use the name from the associated buffer if it exists. */
  bp = buflist_findnr(aep->ae_fnum);
  if (bp == NULL || bp->b_fname == NULL)
    return aep->ae_fname;
  return bp->b_fname;
}

/*
 * do_arg_all(): Open up to 'count' windows, one for each argument.
 */
void do_arg_all(
    int count,
    int forceit,   /* hide buffers in current windows */
    int keep_tabs) /* keep current tabs, for ":tab drop file" */
{
  int i;
  win_T *wp, *wpnext;
  char_u *opened;           /* Array of weight for which args are open:
				 *  0: not opened
				 *  1: opened in other tab
				 *  2: opened in curtab
				 *  3: opened in curtab and curwin
				 */
  int opened_len;           /* length of opened[] */
  int use_firstwin = FALSE; /* use first window for arglist */
  int split_ret = OK;
  int p_ea_save;
  alist_T *alist; /* argument list to be used */
  buf_T *buf;
  tabpage_T *tpnext;
  int had_tab = cmdmod.tab;
  win_T *old_curwin, *last_curwin;
  tabpage_T *old_curtab, *last_curtab;
  win_T *new_curwin = NULL;
  tabpage_T *new_curtab = NULL;

  if (ARGCOUNT <= 0)
  {
    /* Don't give an error message.  We don't want it when the ":all"
	 * command is in the .vimrc. */
    return;
  }
  setpcmark();

  opened_len = ARGCOUNT;
  opened = alloc_clear(opened_len);
  if (opened == NULL)
    return;

  /* Autocommands may do anything to the argument list.  Make sure it's not
     * freed while we are working here by "locking" it.  We still have to
     * watch out for its size to be changed. */
  alist = curwin->w_alist;
  ++alist->al_refcount;

  old_curwin = curwin;
  old_curtab = curtab;

  /*
     * Try closing all windows that are not in the argument list.
     * Also close windows that are not full width;
     * When 'hidden' or "forceit" set the buffer becomes hidden.
     * Windows that have a changed buffer and can't be hidden won't be closed.
     * When the ":tab" modifier was used do this for all tab pages.
     */
  if (had_tab > 0)
    goto_tabpage_tp(first_tabpage, TRUE, TRUE);
  for (;;)
  {
    tpnext = curtab->tp_next;
    for (wp = firstwin; wp != NULL; wp = wpnext)
    {
      wpnext = wp->w_next;
      buf = wp->w_buffer;
      if (buf->b_ffname == NULL || (!keep_tabs && (buf->b_nwindows > 1 || wp->w_width != Columns)))
        i = opened_len;
      else
      {
        /* check if the buffer in this window is in the arglist */
        for (i = 0; i < opened_len; ++i)
        {
          if (i < alist->al_ga.ga_len && (AARGLIST(alist)[i].ae_fnum == buf->b_fnum || fullpathcmp(alist_name(&AARGLIST(alist)[i]),
                                                                                                   buf->b_ffname, TRUE, TRUE) &
                                                                                           FPC_SAME))
          {
            int weight = 1;

            if (old_curtab == curtab)
            {
              ++weight;
              if (old_curwin == wp)
                ++weight;
            }

            if (weight > (int)opened[i])
            {
              opened[i] = (char_u)weight;
              if (i == 0)
              {
                if (new_curwin != NULL)
                  new_curwin->w_arg_idx = opened_len;
                new_curwin = wp;
                new_curtab = curtab;
              }
            }
            else if (keep_tabs)
              i = opened_len;

            if (wp->w_alist != alist)
            {
              /* Use the current argument list for all windows
			     * containing a file from it. */
              alist_unlink(wp->w_alist);
              wp->w_alist = alist;
              ++wp->w_alist->al_refcount;
            }
            break;
          }
        }
      }
      wp->w_arg_idx = i;

      if (i == opened_len && !keep_tabs) /* close this window */
      {
        if (buf_hide(buf) || forceit || buf->b_nwindows > 1 || !bufIsChanged(buf))
        {
          /* If the buffer was changed, and we would like to hide it,
		     * try autowriting. */
          if (!buf_hide(buf) && buf->b_nwindows <= 1 && bufIsChanged(buf))
          {
            bufref_T bufref;

            set_bufref(&bufref, buf);

            (void)autowrite(buf, FALSE);

            /* check if autocommands removed the window */
            if (!win_valid(wp) || !bufref_valid(&bufref))
            {
              wpnext = firstwin; /* start all over... */
              continue;
            }
          }
          /* don't close last window */
          if (ONE_WINDOW && (first_tabpage->tp_next == NULL || !had_tab))
            use_firstwin = TRUE;
          else
          {
            win_close(wp, !buf_hide(buf) && !bufIsChanged(buf));

            /* check if autocommands removed the next window */
            if (!win_valid(wpnext))
              wpnext = firstwin; /* start all over... */
          }
        }
      }
    }

    /* Without the ":tab" modifier only do the current tab page. */
    if (had_tab == 0 || tpnext == NULL)
      break;

    /* check if autocommands removed the next tab page */
    if (!valid_tabpage(tpnext))
      tpnext = first_tabpage; /* start all over...*/

    goto_tabpage_tp(tpnext, TRUE, TRUE);
  }

  /*
     * Open a window for files in the argument list that don't have one.
     * ARGCOUNT may change while doing this, because of autocommands.
     */
  if (count > opened_len || count <= 0)
    count = opened_len;

  /* Don't execute Win/Buf Enter/Leave autocommands here. */
  ++autocmd_no_enter;
  ++autocmd_no_leave;
  last_curwin = curwin;
  last_curtab = curtab;
  win_enter(lastwin, FALSE);
  /* ":drop all" should re-use an empty window to avoid "--remote-tab"
     * leaving an empty tab page when executed locally. */
  if (keep_tabs && BUFEMPTY() && curbuf->b_nwindows == 1 && curbuf->b_ffname == NULL && !curbuf->b_changed)
    use_firstwin = TRUE;

  for (i = 0; i < count && i < opened_len && !got_int; ++i)
  {
    if (alist == &global_alist && i == global_alist.al_ga.ga_len - 1)
      arg_had_last = TRUE;
    if (opened[i] > 0)
    {
      /* Move the already present window to below the current window */
      if (curwin->w_arg_idx != i)
      {
        for (wpnext = firstwin; wpnext != NULL; wpnext = wpnext->w_next)
        {
          if (wpnext->w_arg_idx == i)
          {
            if (keep_tabs)
            {
              new_curwin = wpnext;
              new_curtab = curtab;
            }
            else
              win_move_after(wpnext, curwin);
            break;
          }
        }
      }
    }
    else if (split_ret == OK)
    {
      if (!use_firstwin) /* split current window */
      {
        p_ea_save = p_ea;
        p_ea = TRUE; /* use space from all windows */
        split_ret = win_split(0, WSP_ROOM | WSP_BELOW);
        p_ea = p_ea_save;
        if (split_ret == FAIL)
          continue;
      }
      else /* first window: do autocmd for leaving this buffer */
        --autocmd_no_leave;

      /*
	     * edit file "i"
	     */
      curwin->w_arg_idx = i;
      if (i == 0)
      {
        new_curwin = curwin;
        new_curtab = curtab;
      }
      (void)do_ecmd(0, alist_name(&AARGLIST(alist)[i]), NULL, NULL,
                    ECMD_ONE,
                    ((buf_hide(curwin->w_buffer) || bufIsChanged(curwin->w_buffer)) ? ECMD_HIDE : 0) + ECMD_OLDBUF, curwin);
      if (use_firstwin)
        ++autocmd_no_leave;
      use_firstwin = FALSE;
    }
    ui_breakcheck();

    /* When ":tab" was used open a new tab for a new window repeatedly. */
    if (had_tab > 0 && tabpage_index(NULL) <= p_tpm)
      cmdmod.tab = 9999;
  }

  /* Remove the "lock" on the argument list. */
  alist_unlink(alist);

  --autocmd_no_enter;

  /* restore last referenced tabpage's curwin */
  if (last_curtab != new_curtab)
  {
    if (valid_tabpage(last_curtab))
      goto_tabpage_tp(last_curtab, TRUE, TRUE);
    if (win_valid(last_curwin))
      win_enter(last_curwin, FALSE);
  }
  /* to window with first arg */
  if (valid_tabpage(new_curtab))
    goto_tabpage_tp(new_curtab, TRUE, TRUE);
  if (win_valid(new_curwin))
    win_enter(new_curwin, FALSE);

  --autocmd_no_leave;
  vim_free(opened);
}

/*
 * Open a window for a number of buffers.
 */
void ex_buffer_all(exarg_T *eap)
{
  buf_T *buf;
  win_T *wp, *wpnext;
  int split_ret = OK;
  int p_ea_save;
  int open_wins = 0;
  int r;
  int count; /* Maximum number of windows to open. */
  int all;   /* When TRUE also load inactive buffers. */
  int had_tab = cmdmod.tab;
  tabpage_T *tpnext;

  if (eap->addr_count == 0) /* make as many windows as possible */
    count = 9999;
  else
    count = eap->line2; /* make as many windows as specified */
  if (eap->cmdidx == CMD_unhide || eap->cmdidx == CMD_sunhide)
    all = FALSE;
  else
    all = TRUE;

  setpcmark();

  /*
     * Close superfluous windows (two windows for the same buffer).
     * Also close windows that are not full-width.
     */
  if (had_tab > 0)
    goto_tabpage_tp(first_tabpage, TRUE, TRUE);
  for (;;)
  {
    tpnext = curtab->tp_next;
    for (wp = firstwin; wp != NULL; wp = wpnext)
    {
      wpnext = wp->w_next;
      if ((wp->w_buffer->b_nwindows > 1 || ((cmdmod.split & WSP_VERT) ? wp->w_height + wp->w_status_height < Rows - p_ch - tabline_height() : wp->w_width != Columns) || (had_tab > 0 && wp != firstwin)) && !ONE_WINDOW && !(wp->w_closing || wp->w_buffer->b_locked > 0))
      {
        win_close(wp, FALSE);
        wpnext = firstwin;      /* just in case an autocommand does
					   something strange with windows */
        tpnext = first_tabpage; /* start all over... */
        open_wins = 0;
      }
      else
        ++open_wins;
    }

    /* Without the ":tab" modifier only do the current tab page. */
    if (had_tab == 0 || tpnext == NULL)
      break;
    goto_tabpage_tp(tpnext, TRUE, TRUE);
  }

  /*
     * Go through the buffer list.  When a buffer doesn't have a window yet,
     * open one.  Otherwise move the window to the right position.
     * Watch out for autocommands that delete buffers or windows!
     */
  /* Don't execute Win/Buf Enter/Leave autocommands here. */
  ++autocmd_no_enter;
  win_enter(lastwin, FALSE);
  ++autocmd_no_leave;
  for (buf = firstbuf; buf != NULL && open_wins < count; buf = buf->b_next)
  {
    /* Check if this buffer needs a window */
    if ((!all && buf->b_ml.ml_mfp == NULL) || !buf->b_p_bl)
      continue;

    if (had_tab != 0)
    {
      /* With the ":tab" modifier don't move the window. */
      if (buf->b_nwindows > 0)
        wp = lastwin; /* buffer has a window, skip it */
      else
        wp = NULL;
    }
    else
    {
      /* Check if this buffer already has a window */
      FOR_ALL_WINDOWS(wp)
      if (wp->w_buffer == buf)
        break;
      /* If the buffer already has a window, move it */
      if (wp != NULL)
        win_move_after(wp, curwin);
    }

    if (wp == NULL && split_ret == OK)
    {
      bufref_T bufref;

      set_bufref(&bufref, buf);

      /* Split the window and put the buffer in it */
      p_ea_save = p_ea;
      p_ea = TRUE; /* use space from all windows */
      split_ret = win_split(0, WSP_ROOM | WSP_BELOW);
      ++open_wins;
      p_ea = p_ea_save;
      if (split_ret == FAIL)
        continue;

      /* Open the buffer in this window. */
      swap_exists_action = SEA_DIALOG;
      set_curbuf(buf, DOBUF_GOTO);
      if (!bufref_valid(&bufref))
      {
        /* autocommands deleted the buffer!!! */
        swap_exists_action = SEA_NONE;
        break;
      }
      if (swap_exists_action == SEA_QUIT)
      {
#if defined(FEAT_EVAL)
        cleanup_T cs;

        /* Reset the error/interrupt/exception state here so that
		 * aborting() returns FALSE when closing a window. */
        enter_cleanup(&cs);
#endif

        /* User selected Quit at ATTENTION prompt; close this window. */
        win_close(curwin, TRUE);
        --open_wins;
        swap_exists_action = SEA_NONE;
        swap_exists_did_quit = TRUE;

#if defined(FEAT_EVAL)
        /* Restore the error/interrupt/exception state if not
		 * discarded by a new aborting error, interrupt, or uncaught
		 * exception. */
        leave_cleanup(&cs);
#endif
      }
      else
        handle_swap_exists(NULL);
    }

    ui_breakcheck();
    if (got_int)
    {
      (void)vgetc(); /* only break the file loading, not the rest */
      break;
    }
#ifdef FEAT_EVAL
    /* Autocommands deleted the buffer or aborted script processing!!! */
    if (aborting())
      break;
#endif
    /* When ":tab" was used open a new tab for a new window repeatedly. */
    if (had_tab > 0 && tabpage_index(NULL) <= p_tpm)
      cmdmod.tab = 9999;
  }
  --autocmd_no_enter;
  win_enter(firstwin, FALSE); /* back to first window */
  --autocmd_no_leave;

  /*
     * Close superfluous windows.
     */
  for (wp = lastwin; open_wins > count;)
  {
    r = (buf_hide(wp->w_buffer) || !bufIsChanged(wp->w_buffer) || autowrite(wp->w_buffer, FALSE) == OK);
    if (!win_valid(wp))
    {
      /* BufWrite Autocommands made the window invalid, start over */
      wp = lastwin;
    }
    else if (r)
    {
      win_close(wp, !buf_hide(wp->w_buffer));
      --open_wins;
      wp = lastwin;
    }
    else
    {
      wp = wp->w_prev;
      if (wp == NULL)
        break;
    }
  }
}

static int chk_modeline(linenr_T, int);

/*
 * do_modelines() - process mode lines for the current file
 *
 * "flags" can be:
 * OPT_WINONLY	    only set options local to window
 * OPT_NOWIN	    don't set options local to window
 *
 * Returns immediately if the "ml" option isn't set.
 */
void do_modelines(int flags)
{
  linenr_T lnum;
  int nmlines;
  static int entered = 0;

  if (!curbuf->b_p_ml || (nmlines = (int)p_mls) == 0)
    return;

  /* Disallow recursive entry here.  Can happen when executing a modeline
     * triggers an autocommand, which reloads modelines with a ":do". */
  if (entered)
    return;

  ++entered;
  for (lnum = 1; lnum <= curbuf->b_ml.ml_line_count && lnum <= nmlines;
       ++lnum)
    if (chk_modeline(lnum, flags) == FAIL)
      nmlines = 0;

  for (lnum = curbuf->b_ml.ml_line_count; lnum > 0 && lnum > nmlines && lnum > curbuf->b_ml.ml_line_count - nmlines; --lnum)
    if (chk_modeline(lnum, flags) == FAIL)
      nmlines = 0;
  --entered;
}

#include "version.h" /* for version number */

/*
 * chk_modeline() - check a single line for a mode string
 * Return FAIL if an error encountered.
 */
static int
chk_modeline(
    linenr_T lnum,
    int flags) /* Same as for do_modelines(). */
{
  char_u *s;
  char_u *e;
  char_u *linecopy; /* local copy of any modeline found */
  int prev;
  int vers;
  int end;
  int retval = OK;
  char_u *save_sourcing_name;
  linenr_T save_sourcing_lnum;
#ifdef FEAT_EVAL
  sctx_T save_current_sctx;
#endif

  prev = -1;
  for (s = ml_get(lnum); *s != NUL; ++s)
  {
    if (prev == -1 || vim_isspace(prev))
    {
      if ((prev != -1 && STRNCMP(s, "ex:", (size_t)3) == 0) || STRNCMP(s, "vi:", (size_t)3) == 0)
        break;
      /* Accept both "vim" and "Vim". */
      if ((s[0] == 'v' || s[0] == 'V') && s[1] == 'i' && s[2] == 'm')
      {
        if (s[3] == '<' || s[3] == '=' || s[3] == '>')
          e = s + 4;
        else
          e = s + 3;
        vers = getdigits(&e);
        if (*e == ':' && (s[0] != 'V' || STRNCMP(skipwhite(e + 1), "set", 3) == 0) && (s[3] == ':' || (VIM_VERSION_100 >= vers && isdigit(s[3])) || (VIM_VERSION_100 < vers && s[3] == '<') || (VIM_VERSION_100 > vers && s[3] == '>') || (VIM_VERSION_100 == vers && s[3] == '=')))
          break;
      }
    }
    prev = *s;
  }

  if (*s)
  {
    do /* skip over "ex:", "vi:" or "vim:" */
      ++s;
    while (s[-1] != ':');

    s = linecopy = vim_strsave(s); /* copy the line, it will change */
    if (linecopy == NULL)
      return FAIL;

    save_sourcing_lnum = sourcing_lnum;
    save_sourcing_name = sourcing_name;
    sourcing_lnum = lnum; /* prepare for emsg() */
    sourcing_name = (char_u *)"modelines";

    end = FALSE;
    while (end == FALSE)
    {
      s = skipwhite(s);
      if (*s == NUL)
        break;

      /*
	     * Find end of set command: ':' or end of line.
	     * Skip over "\:", replacing it with ":".
	     */
      for (e = s; *e != ':' && *e != NUL; ++e)
        if (e[0] == '\\' && e[1] == ':')
          STRMOVE(e, e + 1);
      if (*e == NUL)
        end = TRUE;

      /*
	     * If there is a "set" command, require a terminating ':' and
	     * ignore the stuff after the ':'.
	     * "vi:set opt opt opt: foo" -- foo not interpreted
	     * "vi:opt opt opt: foo" -- foo interpreted
	     * Accept "se" for compatibility with Elvis.
	     */
      if (STRNCMP(s, "set ", (size_t)4) == 0 || STRNCMP(s, "se ", (size_t)3) == 0)
      {
        if (*e != ':') /* no terminating ':'? */
          break;
        end = TRUE;
        s = vim_strchr(s, ' ') + 1;
      }
      *e = NUL; /* truncate the set command */

      if (*s != NUL) /* skip over an empty "::" */
      {
        int secure_save = secure;
#ifdef FEAT_EVAL
        save_current_sctx = current_sctx;
        current_sctx.sc_sid = SID_MODELINE;
        current_sctx.sc_seq = 0;
        current_sctx.sc_lnum = 0;
        current_sctx.sc_version = 1;
#endif
        // Make sure no risky things are executed as a side effect.
        secure = 1;

        retval = do_set(s, OPT_MODELINE | OPT_LOCAL | flags);

        secure = secure_save;
#ifdef FEAT_EVAL
        current_sctx = save_current_sctx;
#endif
        if (retval == FAIL) /* stop if error found */
          break;
      }
      s = e + 1; /* advance to next part */
    }

    sourcing_lnum = save_sourcing_lnum;
    sourcing_name = save_sourcing_name;

    vim_free(linecopy);
  }
  return retval;
}

#if defined(FEAT_VIMINFO) || defined(PROTO)
int read_viminfo_bufferlist(
    vir_T *virp,
    int writing)
{
  char_u *tab;
  linenr_T lnum;
  colnr_T col;
  buf_T *buf;
  char_u *sfname;
  char_u *xline;

  /* Handle long line and escaped characters. */
  xline = viminfo_readstring(virp, 1, FALSE);

  /* don't read in if there are files on the command-line or if writing: */
  if (xline != NULL && !writing && ARGCOUNT == 0 && find_viminfo_parameter('%') != NULL)
  {
    /* Format is: <fname> Tab <lnum> Tab <col>.
	 * Watch out for a Tab in the file name, work from the end. */
    lnum = 0;
    col = 0;
    tab = vim_strrchr(xline, '\t');
    if (tab != NULL)
    {
      *tab++ = '\0';
      col = (colnr_T)atoi((char *)tab);
      tab = vim_strrchr(xline, '\t');
      if (tab != NULL)
      {
        *tab++ = '\0';
        lnum = atol((char *)tab);
      }
    }

    /* Expand "~/" in the file name at "line + 1" to a full path.
	 * Then try shortening it by comparing with the current directory */
    expand_env(xline, NameBuff, MAXPATHL);
    sfname = shorten_fname1(NameBuff);

    buf = buflist_new(NameBuff, sfname, (linenr_T)0, BLN_LISTED);
    if (buf != NULL) /* just in case... */
    {
      buf->b_last_cursor.lnum = lnum;
      buf->b_last_cursor.col = col;
      buflist_setfpos(buf, curwin, lnum, col, FALSE);
    }
  }
  vim_free(xline);

  return viminfo_readline(virp);
}

void write_viminfo_bufferlist(FILE *fp)
{
  buf_T *buf;
  win_T *win;
  tabpage_T *tp;
  char_u *line;
  int max_buffers;

  if (find_viminfo_parameter('%') == NULL)
    return;

  /* Without a number -1 is returned: do all buffers. */
  max_buffers = get_viminfo_parameter('%');

  /* Allocate room for the file name, lnum and col. */
#define LINE_BUF_LEN (MAXPATHL + 40)
  line = alloc(LINE_BUF_LEN);
  if (line == NULL)
    return;

  FOR_ALL_TAB_WINDOWS(tp, win)
  set_last_cursor(win);

  fputs(_("\n# Buffer list:\n"), fp);
  FOR_ALL_BUFFERS(buf)
  {
    if (buf->b_fname == NULL || !buf->b_p_bl
#ifdef FEAT_QUICKFIX
        || bt_quickfix(buf)
#endif
#ifdef FEAT_TERMINAL
        || bt_terminal(buf)
#endif
        || removable(buf->b_ffname))
      continue;

    if (max_buffers-- == 0)
      break;
    putc('%', fp);
    home_replace(NULL, buf->b_ffname, line, MAXPATHL, TRUE);
    vim_snprintf_add((char *)line, LINE_BUF_LEN, "\t%ld\t%d",
                     (long)buf->b_last_cursor.lnum,
                     buf->b_last_cursor.col);
    viminfo_writestring(fp, line);
  }
  vim_free(line);
}
#endif

/*
 * Return TRUE if "buf" is a normal buffer, 'buftype' is empty.
 */
int bt_normal(buf_T *buf)
{
  return buf != NULL && buf->b_p_bt[0] == NUL;
}

#if defined(FEAT_QUICKFIX) || defined(PROTO)
/*
 * Return TRUE if "buf" is the quickfix buffer.
 */
int bt_quickfix(buf_T *buf)
{
  return buf != NULL && buf->b_p_bt[0] == 'q';
}
#endif

#if defined(FEAT_TERMINAL) || defined(PROTO)
/*
 * Return TRUE if "buf" is a terminal buffer.
 */
int bt_terminal(buf_T *buf)
{
  return buf != NULL && buf->b_p_bt[0] == 't';
}
#endif

/*
 * Return TRUE if "buf" is a help buffer.
 */
int bt_help(buf_T *buf)
{
  return buf != NULL && buf->b_help;
}

/*
 * Return TRUE if "buf" is a prompt buffer.
 */
int bt_prompt(buf_T *buf)
{
  return buf != NULL && buf->b_p_bt[0] == 'p' && buf->b_p_bt[1] == 'r';
}

/*
 * Return TRUE if "buf" is a buffer for a popup window.
 */
int bt_popup(buf_T *buf)
{
  return buf != NULL && buf->b_p_bt != NULL && buf->b_p_bt[0] == 'p' && buf->b_p_bt[1] == 'o';
}

/*
 * Return TRUE if "buf" is a "nofile", "acwrite", "terminal" or "prompt"
 * buffer.  This means the buffer name is not a file name.
 */
int bt_nofile(buf_T *buf)
{
  return buf != NULL && ((buf->b_p_bt[0] == 'n' && buf->b_p_bt[2] == 'f') || buf->b_p_bt[0] == 'a' || buf->b_p_bt[0] == 't' || buf->b_p_bt[0] == 'p');
}

/*
 * Return TRUE if "buf" is a "nowrite", "nofile", "terminal" or "prompt"
 * buffer.
 */
int bt_dontwrite(buf_T *buf)
{
  return buf != NULL && (buf->b_p_bt[0] == 'n' || buf->b_p_bt[0] == 't' || buf->b_p_bt[0] == 'p');
}

#if defined(FEAT_QUICKFIX) || defined(PROTO)
int bt_dontwrite_msg(buf_T *buf)
{
  if (bt_dontwrite(buf))
  {
    emsg(_("E382: Cannot write, 'buftype' option is set"));
    return TRUE;
  }
  return FALSE;
}
#endif

/*
 * Return TRUE if the buffer should be hidden, according to 'hidden', ":hide"
 * and 'bufhidden'.
 */
int buf_hide(buf_T *buf)
{
  /* 'bufhidden' overrules 'hidden' and ":hide", check it first */
  switch (buf->b_p_bh[0])
  {
  case 'u': /* "unload" */
  case 'w': /* "wipe" */
  case 'd':
    return FALSE; /* "delete" */
  case 'h':
    return TRUE; /* "hide" */
  }
  return (p_hid || cmdmod.hide);
}

/*
 * Return special buffer name.
 * Returns NULL when the buffer has a normal file name.
 */
char_u *
buf_spname(buf_T *buf)
{
#if defined(FEAT_QUICKFIX)
  if (bt_quickfix(buf))
  {
    /*
	 * Differentiate between the quickfix and location list buffers using
	 * the buffer number stored in the global quickfix stack.
	 */
    if (buf->b_fnum == qf_stack_get_bufnr())
      return (char_u *)_(msg_qflist);
    else
      return (char_u *)_(msg_loclist);
  }
#endif

  /* There is no _file_ when 'buftype' is "nofile", b_sfname
     * contains the name as specified by the user. */
  if (bt_nofile(buf))
  {
#ifdef FEAT_TERMINAL
    if (buf->b_term != NULL)
      return term_get_status_text(buf->b_term);
#endif
    if (buf->b_fname != NULL)
      return buf->b_fname;
#ifdef FEAT_JOB_CHANNEL
    if (bt_prompt(buf))
      return (char_u *)_("[Prompt]");
#endif
    return (char_u *)_("[Scratch]");
  }

  if (buf->b_fname == NULL)
    return (char_u *)_("[No Name]");
  return NULL;
}

#if defined(FEAT_JOB_CHANNEL) || defined(FEAT_PYTHON) || defined(FEAT_PYTHON3) || defined(PROTO)
#define SWITCH_TO_WIN

/*
 * Find a window that contains "buf" and switch to it.
 * If there is no such window, use the current window and change "curbuf".
 * Caller must initialize save_curbuf to NULL.
 * restore_win_for_buf() MUST be called later!
 */
void switch_to_win_for_buf(
    buf_T *buf,
    win_T **save_curwinp,
    tabpage_T **save_curtabp,
    bufref_T *save_curbuf)
{
  win_T *wp;
  tabpage_T *tp;

  if (find_win_for_buf(buf, &wp, &tp) == FAIL)
    switch_buffer(save_curbuf, buf);
  else if (switch_win(save_curwinp, save_curtabp, wp, tp, TRUE) == FAIL)
  {
    restore_win(*save_curwinp, *save_curtabp, TRUE);
    switch_buffer(save_curbuf, buf);
  }
}

void restore_win_for_buf(
    win_T *save_curwin,
    tabpage_T *save_curtab,
    bufref_T *save_curbuf)
{
  if (save_curbuf->br_buf == NULL)
    restore_win(save_curwin, save_curtab, TRUE);
  else
    restore_buffer(save_curbuf);
}
#endif

#if defined(FEAT_QUICKFIX) || defined(SWITCH_TO_WIN) || defined(PROTO)
/*
 * Find a window for buffer "buf".
 * If found OK is returned and "wp" and "tp" are set to the window and tabpage.
 * If not found FAIL is returned.
 */
int find_win_for_buf(
    buf_T *buf,
    win_T **wp,
    tabpage_T **tp)
{
  FOR_ALL_TAB_WINDOWS(*tp, *wp)
  if ((*wp)->w_buffer == buf)
    goto win_found;
  return FAIL;
win_found:
  return OK;
}
#endif

/*
 * Set 'buflisted' for curbuf to "on" and trigger autocommands if it changed.
 */
void set_buflisted(int on)
{
  if (on != curbuf->b_p_bl)
  {
    curbuf->b_p_bl = on;
    if (on)
      apply_autocmds(EVENT_BUFADD, NULL, NULL, FALSE, curbuf);
    else
      apply_autocmds(EVENT_BUFDELETE, NULL, NULL, FALSE, curbuf);
  }
}

/*
 * Read the file for "buf" again and check if the contents changed.
 * Return TRUE if it changed or this could not be checked.
 */
int buf_contents_changed(buf_T *buf)
{
  buf_T *newbuf;
  int differ = TRUE;
  linenr_T lnum;
  aco_save_T aco;
  exarg_T ea;

  /* Allocate a buffer without putting it in the buffer list. */
  newbuf = buflist_new(NULL, NULL, (linenr_T)1, BLN_DUMMY);
  if (newbuf == NULL)
    return TRUE;

  /* Force the 'fileencoding' and 'fileformat' to be equal. */
  if (prep_exarg(&ea, buf) == FAIL)
  {
    wipe_buffer(newbuf, FALSE);
    return TRUE;
  }

  /* set curwin/curbuf to buf and save a few things */
  aucmd_prepbuf(&aco, newbuf);

  if (ml_open(curbuf) == OK && readfile(buf->b_ffname, buf->b_fname,
                                        (linenr_T)0, (linenr_T)0, (linenr_T)MAXLNUM,
                                        &ea, READ_NEW | READ_DUMMY) == OK)
  {
    /* compare the two files line by line */
    if (buf->b_ml.ml_line_count == curbuf->b_ml.ml_line_count)
    {
      differ = FALSE;
      for (lnum = 1; lnum <= curbuf->b_ml.ml_line_count; ++lnum)
        if (STRCMP(ml_get_buf(buf, lnum, FALSE), ml_get(lnum)) != 0)
        {
          differ = TRUE;
          break;
        }
    }
  }
  vim_free(ea.cmd);

  /* restore curwin/curbuf and a few other things */
  aucmd_restbuf(&aco);

  if (curbuf != newbuf) /* safety check */
    wipe_buffer(newbuf, FALSE);

  return differ;
}

/*
 * Wipe out a buffer and decrement the last buffer number if it was used for
 * this buffer.  Call this to wipe out a temp buffer that does not contain any
 * marks.
 */
void wipe_buffer(
    buf_T *buf,
    int aucmd UNUSED) /* When TRUE trigger autocommands. */
{
  if (buf->b_fnum == top_file_num - 1)
    --top_file_num;

  if (!aucmd) /* Don't trigger BufDelete autocommands here. */
    block_autocmds();

  close_buffer(NULL, buf, DOBUF_WIPE, FALSE);

  if (!aucmd)
    unblock_autocmds();
}
