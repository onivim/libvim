/* vi:set ts=8 sts=4 sw=4 noet:
 */

/*
 * message2.c: functions for displaying externalized messages
 */

#include "vim.h"
#include "sds.h"

msg_T* msg2_create(msgPriority_T priority) {
    printf("msg2_create called\n");
}

void msg2_send(msg_T *msg) {
    printf("sending message!\n");
};

void msg2_free(msg_T *msg) {
    printf("freeing message!\n");
};

void msg2_put(char_u *s, msg_T *msg) {
    printf("putting message: %s\n", s);
};
