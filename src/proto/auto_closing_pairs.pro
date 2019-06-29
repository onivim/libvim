/* state_machine.c */

void acp_set_pairs(autoClosingPair_T *pairs, int count);

int acp_should_pass_through(char_u c);
char_u acp_get_closing_character(char_u c);
int acp_is_opening_pair(char_u c);
int acp_is_closing_pair(char_u c);
int acp_is_between_pair(); 
autoClosingPair_T *acp_is_between_pair(char_u c);

/* vim: set ft=c : */
