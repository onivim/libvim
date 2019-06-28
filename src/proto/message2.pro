/* message2.c */


msg_T* msg2_create(msgPriority_T priority);
void msg2_put(char_u* s, msg_T *msg);
void msg2_send(msg_T *msg);
void msg2_free(msg_T *msg);

/* vim: set ft=c : */
