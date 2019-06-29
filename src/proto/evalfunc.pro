/* evalfunc.c */
char_u *get_function_name(expand_T *xp, int idx);
char_u *get_expr_name(expand_T *xp, int idx);
int find_internal_func(char_u *name);
int call_internal_func(char_u *name, int argcount, typval_T *argvars,
                       typval_T *rettv);
buf_T *buflist_find_by_name(char_u *name, int curtab_only);
buf_T *tv_get_buf(typval_T *tv, int curtab_only);
buf_T *get_buf_arg(typval_T *arg);
void execute_redir_str(char_u *value, int value_len);
void mzscheme_call_vim(char_u *name, typval_T *args, typval_T *rettv);
float_T vim_round(float_T f);
long do_searchpair(char_u *spat, char_u *mpat, char_u *epat, int dir,
                   typval_T *skip, int flags, pos_T *match_pos,
                   linenr_T lnum_stop, long time_limit);
void f_string(typval_T *argvars, typval_T *rettv);
callback_T get_callback(typval_T *arg);
void put_callback(callback_T *cb, typval_T *tv);
void set_callback(callback_T *dest, callback_T *src);
void free_callback(callback_T *callback);
/* vim: set ft=c : */
