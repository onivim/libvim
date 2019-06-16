/* state_machine.c */

void sm_push(int mode, void *context, state_execute executeFn, state_cleanup cleanupFn);

void sm_push_insert(int cmdchar, int startln, long count);
void sm_push_normal();
void sm_push_change(oparg_T *oap);
void sm_push_cmdline(int cmdchar, long count, int indent);

void sm_execute_normal(char_u *keys);
void sm_execute(char_u* key);

int sm_get_current_mode(void);

sm_T* sm_get_current(void);

/* vim: set ft=c : */
