/* vi:set ts=8 sts=4 sw=4 noet:
 */

/*
 * message2.c: functions for displaying externalized messages
 */

#include "vim.h"

msg_T *msg2_create(msgPriority_T priority)
{
  msg_T *ret = (msg_T *)alloc(sizeof(msg_T));
  ret->title = sdsempty();
  ret->contents = sdsempty();
  ret->priority = priority;
  return ret;
}

void msg2_send(msg_T *msg)
{
  if (messageCallback != NULL)
  {
    messageCallback((char_u *)msg->title, (char_u *)msg->contents, msg->priority);
  }
};

void msg2_set_title(char_u *title, msg_T *msg)
{
  msg->title = sdscpy(msg->title, title);
}

char_u *msg2_get_contents(msg_T *msg)
{
  return (char_u *)msg->contents;
}

void msg2_free(msg_T *msg)
{
  if (msg != NULL)
  {
    sdsfree(msg->title);
    sdsfree(msg->contents);
    vim_free(msg);
  }
};

void msg2_put(char_u *s, msg_T *msg)
{
  msg->contents = sdscat(msg->contents, s);
};

/*
 * Put name and line number for the source of an error.
 * Remember the file name and line number, so that for the next error the info
 * is only displayed if it changed.
 */
void msg2_source(msg_T *msg)
{
  char_u *p;

  p = get_emsg_source();
  if (p != NULL)
  {
    msg2_put((char *)p, msg);
    vim_free(p);
  }
  p = get_emsg_lnum();
  if (p != NULL)
  {
    msg2_put((char *)p, msg);
    vim_free(p);
  }
}
