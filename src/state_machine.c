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

void sm_push(int mode, void *context, state_execute executeFn, state_cleanup cleanupFn) {
  sm_T *lastState = state_current;

  sm_T *newState = (sm_T *)alloc(sizeof(sm_T));

  newState->prev = lastState;
  newState->execute_fn = executeFn;
  newState->cleanup_fn = cleanupFn;
  newState->context = context;
  newState->mode = mode;

  state_current = newState;
}

void sm_push_normal() {
    sm_push(NORMAL, state_normal_cmd_initialize(), state_normal_cmd_execute,
	    state_normal_cmd_cleanup);
}

void sm_push_insert(int cmdchar, int startln, long count) {
    sm_push(INSERT, state_edit_initialize(cmdchar, startln, count), state_edit_execute,
	    state_edit_cleanup);
}

/*
 * sm_execute_normal
 *
 * Like sm_execute, but if there is no activate state,
 * defaults to normal mode.
 */
void sm_execute_normal(char_u *keys) {

  if (state_current == NULL) {
      sm_push_normal();
  }

  sm_execute(keys);
}

void sm_execute(char_u *keys) {
  char_u *keys_esc = vim_strsave_escape_csi(keys);
  ins_typebuf(keys_esc, REMAP_YES, 0, FALSE, FALSE);

  if (state_current != NULL) {
    while (vpeekc() != NUL) {
      char_u c = vgetc();

      if (state_current == NULL) {
	  sm_push_normal();
      }

      sm_T *current = state_current;
      executionStatus_T result = current->execute_fn(state_current->context, c);

      switch (result) {
      case HANDLED:
        break;
      case UNHANDLED:
        vungetc(c);
        return;
        break;
      case COMPLETED_UNHANDLED:
        vungetc(c);
        current->cleanup_fn(state_current->context);
        state_current = current->prev;
	vim_free(current);
        break;
      case COMPLETED:
        current->cleanup_fn(state_current->context);
        state_current = current->prev;
	vim_free(current);
        break;
      }
    }
  }
}
