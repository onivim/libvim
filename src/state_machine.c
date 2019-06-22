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

void sm_push(int mode, void *context, state_execute executeFn,
             state_cleanup cleanupFn) {
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
  sm_push(INSERT, state_edit_initialize(cmdchar, startln, count),
          state_edit_execute, state_edit_cleanup);
}

void sm_push_cmdline(int cmdchar, long count, int indent) {
  sm_push(CMDLINE, state_cmdline_initialize(cmdchar, count, indent),
          state_cmdline_execute, state_cmdline_cleanup);
}

void sm_push_change(oparg_T *oap) {
  sm_push(INSERT, state_change_initialize(oap), state_change_execute,
          state_change_cleanup);
}

/*
 * sm_execute_normal
 *
 * Like sm_execute, but if there is no active state,
 * defaults to normal mode.
 */
void sm_execute_normal(char_u *keys) {

  if (state_current == NULL) {
    sm_push_normal();
  }

  sm_execute(keys);
}

void sm_execute(char_u *keys) {
  printf("sm_execute: 1\n");
  printf("sm_execute - keys: %s\n", keys);
  char_u *keys_esc = vim_strsave_escape_csi(keys);
  ins_typebuf(keys_esc, REMAP_YES, 0, FALSE, FALSE);
  printf("sm_execute - 2\n", keys);

  // Reset abbr_cnt after each input here,
  // to enable correct cabbrev expansions
  typebuf.tb_no_abbr_cnt = 0;

  if (state_current != NULL) {
    while (vpeekc() != NUL) {
      int c = vgetc();

      if (state_current == NULL) {
        sm_push_normal();
      }

      printf("sm_execute - 3 - c is: %c (%d)\n", c, c);
      sm_T *current = state_current;
      executionStatus_T result = current->execute_fn(state_current->context, c);
      printf("sm_execute - 4\n", c, c);

      switch (result) {
      case HANDLED:
        printf("sm_execute - HANDLED\n", c, c);
        break;
      case UNHANDLED:
        printf("sm_execute - UNHANDLED\n", c, c);
        vungetc(c);
        return;
        break;
      case COMPLETED_UNHANDLED:
        printf("sm_execute - COMPLETED_UNHANDLED\n", c, c);
        vungetc(c);
        current->cleanup_fn(state_current->context);
        state_current = current->prev;
        vim_free(current);
        break;
      case COMPLETED:
        printf("sm_execute - COMPLETED\n", c, c);
        current->cleanup_fn(state_current->context);
        state_current = current->prev;
        vim_free(current);
        break;
      }
    }
  }
}
