/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * state_machine
 */

/*
 * Manage current input state
 */

#include "vim.h"

sm_T *sm_get_current() { return state_current; }

int sm_get_current_mode() { return state_current->mode; }

int no_pending_operator(void *ctx, pendingOp_T *cmdarg)
{
  return FALSE;
}

subMode_T sm_get_current_sub_mode()
{
  if (state_current == NULL)
  {
    return SM_NONE;
  }
  else
  {
    return state_current->subMode;
  }
};

void sm_push(int mode, subMode_T subMode, void *context, state_execute executeFn,
             state_pending_operator pendingOperatorFn,
             state_cleanup cleanupFn)
{
  sm_T *lastState = state_current;

  sm_T *newState = (sm_T *)alloc(sizeof(sm_T));

  newState->prev = (sm_T *)lastState;
  newState->execute_fn = executeFn;
  newState->cleanup_fn = cleanupFn;
  newState->pending_operator_fn = pendingOperatorFn;
  newState->context = context;
  newState->mode = mode;
  newState->subMode = subMode;

  state_current = newState;
}

int sm_get_pending_operator(pendingOp_T *pendingOp)
{
  if (state_current == NULL)
  {
    return FALSE;
  }

  return state_current->pending_operator_fn(state_current->context, pendingOp);
}

void sm_push_normal()
{
  sm_push(NORMAL, SM_NONE, state_normal_cmd_initialize(), state_normal_cmd_execute,
          state_normal_pending_operator,
          state_normal_cmd_cleanup);
}

void sm_push_insert(int cmdchar, int startln, long count)
{
  sm_push(INSERT, SM_NONE, state_edit_initialize(cmdchar, startln, count),
          state_edit_execute, no_pending_operator, state_edit_cleanup);
}

void sm_push_insert_literal(int *ret)
{
  sm_push(INSERT, SM_INSERT_LITERAL, state_insert_literal_initialize(ret), state_insert_literal_execute, no_pending_operator, state_insert_literal_cleanup);
}

void sm_push_cmdline(int cmdchar, long count, int indent)
{
  sm_push(CMDLINE, SM_NONE, state_cmdline_initialize(cmdchar, count, indent),
          state_cmdline_execute, no_pending_operator, state_cmdline_cleanup);
}

void sm_push_change(oparg_T *oap)
{
  sm_push(INSERT, SM_NONE, state_change_initialize(oap), state_change_execute,
          no_pending_operator,
          state_change_cleanup);
}

/*
 * sm_execute_normal
 *
 * Like sm_execute, but if there is no active state,
 * defaults to normal mode.
 */
void sm_execute_normal(char_u *cmd, int preserveState)
{
  sm_T *previousState = state_current;
  if (preserveState)
  {
    state_current = NULL;
  }

  if (state_current == NULL)
  {
    sm_push_normal();
  }

  char_u *keys_esc = vim_strsave_escape_csi(cmd);
  ins_typebuf(keys_esc, REMAP_YES, 0, FALSE, FALSE);
  vim_free(keys_esc);

  if (state_current != NULL)
  {
    while (vpeekc() != NUL && typebuf.tb_len > 0)
    {
      int c = vgetc();
      if (state_current == NULL)
      {
        sm_push_normal();
      }

      sm_T *current = state_current;
      executionStatus_T result = current->execute_fn(state_current->context, c);

      switch (result)
      {
      case HANDLED:
        break;
      case UNHANDLED:
        vungetc(c);
        return;
        break;
      case COMPLETED_UNHANDLED:
        vungetc(c);
        current->cleanup_fn(state_current->context);
        state_current = (sm_T *)current->prev;
        vim_free(current);
        break;
      case COMPLETED:
        current->cleanup_fn(state_current->context);
        state_current = (sm_T *)current->prev;
        vim_free(current);
        break;
      }
    }
  }

  if (preserveState)
  {

    sm_T *stateToCleanup = state_current;
    while (stateToCleanup != NULL)
    {
      stateToCleanup->cleanup_fn(stateToCleanup->context);
      sm_T *prev = stateToCleanup;
      stateToCleanup = prev->prev;
      vim_free(prev);
    }
    state_current = previousState;
  }
}
void sm_execute(char_u *keys)
{
  char_u *keys_esc = vim_strsave_escape_csi(keys);
  ins_typebuf(keys_esc, REMAP_YES, 0, FALSE, FALSE);
  vim_free(keys_esc);

  // Reset abbr_cnt after each input here,
  // to enable correct cabbrev expansions
  typebuf.tb_no_abbr_cnt = 0;

  if (state_current == NULL)
  {
    sm_push_normal();
  }

  if (state_current != NULL)
  {
    while (vpeekc() != NUL)
    {
      int c = vgetc();

      if (state_current == NULL)
      {
        sm_push_normal();
      }

      sm_T *current = state_current;
      executionStatus_T result = current->execute_fn(state_current->context, c);

      switch (result)
      {
      case HANDLED:
        break;
      case UNHANDLED:
        vungetc(c);
        return;
        break;
      case COMPLETED_UNHANDLED:
        vungetc(c);
        current->cleanup_fn(state_current->context);
        state_current = (sm_T *)current->prev;
        vim_free(current);
        break;
      case COMPLETED:
        current->cleanup_fn(state_current->context);
        state_current = (sm_T *)current->prev;
        vim_free(current);
        break;
      }
    }
  }
}
