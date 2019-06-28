/* vi:set ts=8 sts=4 sw=4 noet:
 */

/*
 * message2.c: functions for displaying externalized messages
 */

#include "vim.h"

msg_T* msg2_create(msgPriority_T priority) {
    printf("msg2_create called\n");
    msg_T* ret = (msg_T *)alloc(sizeof(msg_T));
    ret->contents = sdsempty();
    ret->priority = priority;
    return ret;
}

void msg2_send(msg_T *msg) {
    printf("sending message: %s\n", msg->contents);
    if (messageCallback != NULL) {
	    messageCallback((char_u*)msg->contents, msg->priority);
    }
};

char_u* msg2_get_contents(msg_T *msg) {
    return (char_u*)msg->contents;
}

void msg2_free(msg_T *msg) {
    printf("freeing message!\n");

    if (msg != NULL) {
        sdsfree(msg->contents);
        vim_free(msg);
    }
};

void msg2_put(char_u *s, msg_T *msg) {
    msg->contents = sdscat(msg->contents, s);
    printf("putting message: %s\n", s);
};
