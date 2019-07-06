/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * state_machine
 */

/*
 * Manage current input state
 */

#include "vim.h"

typedef struct
{
  int cc;
  int nc;
  int i;
  int hex;
  int octal;
  int unicode;
  int *ret;
} insertLiteral_T;

void *state_insert_literal_initialize(int *ret)
{

  insertLiteral_T *context = (insertLiteral_T *)alloc(sizeof(insertLiteral_T));
  context->hex = FALSE;
  context->octal = FALSE;
  context->unicode = 0;

  ++no_mapping;

  context->cc = 0;
  context->i = 0;
  context->nc = 0;
  context->ret = ret;

  return context;
}

executionStatus_T state_insert_literal_execute(void *ctx, int nc)
{
  insertLiteral_T *context = (insertLiteral_T *)ctx;
  context->nc = nc;

  if (context->nc == 'x' || context->nc == 'X')
  {
    context->hex = TRUE;
  }
  else if (context->nc == 'o' || context->nc == 'O')
  {
    context->octal = TRUE;
  }
  else if (context->nc == 'u' || context->nc == 'U')
  {
    context->unicode = context->nc;
  }
  else
  {
    if (context->hex || context->unicode != 0)
    {
      /* We return COMPLETED_UNHANDLED here so that the last key press
       * can be processed by insert mode. The insert mode state machine will
       * pick it up, insert whatever it needs to from `context->ret`, and
       * take care of returning HANDLED */
      if (!vim_isxdigit(context->nc))
        return COMPLETED_UNHANDLED;

      context->cc = context->cc * 16 + hex2nr(context->nc);
    }
    else if (context->octal)
    {
      if (context->nc < '0' || context->nc > '7')
      {
        return COMPLETED_UNHANDLED;
      }

      context->cc = context->cc * 8 + context->nc - '0';
    }
    else
    {
      if (!VIM_ISDIGIT(context->nc))
      {
        return COMPLETED_UNHANDLED;
      }

      context->cc = context->cc * 10 + context->nc - '0';
    }

    context->i++;
  }

  if (context->cc > 255 && context->unicode == 0)
  {
    context->cc = 255; /* limit range to 0-255 */
  }

  context->nc = 0;

  if (context->hex)
  {
    if (context->i >= 2)
      return COMPLETED_UNHANDLED;
  }
  else if (context->unicode)
  { /* Unicode: up to four or eight chars */
    if ((context->unicode == 'u' && context->i >= 4) || (context->unicode == 'U' && context->i >= 8))
      return COMPLETED_UNHANDLED;
  }
  else if (context->i >= 3) /* decimal or octal: up to three chars */
  {
    return COMPLETED_UNHANDLED;
  }

  return HANDLED;
}

void state_insert_literal_cleanup(void *ctx)
{
  insertLiteral_T *context = (insertLiteral_T *)ctx;

  if (context->i == 0) /* no number entered */
  {
    if (context->nc == K_ZERO) /* NUL is stored as NL */
    {
      context->cc = '\n';
      context->nc = 0;
    }
    else
    {
      context->cc = context->nc;
      context->nc = 0;
    }
  }

  *(context->ret) = context->cc;

  --no_mapping;
  vim_free(context);
}
