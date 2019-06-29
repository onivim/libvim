/* autocmd.c */
void aubuflocal_remove(buf_T *buf);
int au_has_group(char_u *name);
void do_augroup(char_u *arg, int del_group);
void free_all_autocmds(void);
int check_ei(void);
char_u *au_event_disable(char *what);
void au_event_restore(char_u *old_ei);
void do_autocmd(char_u *arg_in, int forceit);
int do_doautocmd(char_u *arg, int do_msg, int *did_something);
void ex_doautoall(exarg_T *eap);
int check_nomodeline(char_u **argp);
void aucmd_prepbuf(aco_save_T *aco, buf_T *buf);
void aucmd_restbuf(aco_save_T *aco);
int apply_autocmds(event_T event, char_u *fname, char_u *fname_io, int force,
                   buf_T *buf);
int apply_autocmds_exarg(event_T event, char_u *fname, char_u *fname_io,
                         int force, buf_T *buf, exarg_T *eap);
int apply_autocmds_retval(event_T event, char_u *fname, char_u *fname_io,
                          int force, buf_T *buf, int *retval);
int has_cursorhold(void);
int trigger_cursorhold(void);
int has_cursormoved(void);
int has_cursormovedI(void);
int has_textchanged(void);
int has_textchangedI(void);
int has_textchangedP(void);
int has_insertcharpre(void);
int has_cmdundefined(void);
int has_funcundefined(void);
int has_textyankpost(void);
int has_completechanged(void);
void block_autocmds(void);
void unblock_autocmds(void);
int is_autocmd_blocked(void);
char_u *getnextac(int c, void *cookie, int indent);
int has_autocmd(event_T event, char_u *sfname, buf_T *buf);
char_u *get_augroup_name(expand_T *xp, int idx);
char_u *set_context_in_autocmd(expand_T *xp, char_u *arg, int doautocmd);
char_u *get_event_name(expand_T *xp, int idx);
int autocmd_supported(char_u *name);
int au_exists(char_u *arg);
/* vim: set ft=c : */
