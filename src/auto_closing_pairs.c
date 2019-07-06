/* vi:set ts=8 sts=4 sw=4 noet:
 */

/*
 * auto_closing_pairs.c: functions for working with auto-closing pairs
 */

#include "vim.h"

typedef struct
{
  char_u open;
  char_u close;
} pairInfo_T;

#define PAIR_COUNT 256

/*
 * For now, only support ASCII auto-closing pairs -
 * so we can create an array to store the pair info
 */
pairInfo_T *openCharacter[PAIR_COUNT];
pairInfo_T *closeCharacter[PAIR_COUNT];

void clear_acp_info(void)
{
  for (int i = 0; i < PAIR_COUNT; i++)
  {

    if (openCharacter[i] != NULL)
    {
      pairInfo_T *pair = openCharacter[i];
      closeCharacter[pair->close] = NULL;
      openCharacter[i] = NULL;
      vim_free(pair);
    }
  }

  for (int i = 0; i < PAIR_COUNT; i++)
  {
    if (closeCharacter[i] != NULL)
    {
      vim_free(closeCharacter[i]);
      closeCharacter[i] = NULL;
    }
  }
}

void acp_set_pairs(autoClosingPair_T *pairs, int count)
{
  clear_acp_info();

  for (int i = 0; i < count; i++)
  {
    autoClosingPair_T pair = pairs[i];

    pairInfo_T *localPair = (pairInfo_T *)(alloc(sizeof(pairInfo_T)));
    localPair->open = pair.open;
    localPair->close = pair.close;

    openCharacter[localPair->open] = localPair;
    closeCharacter[localPair->close] = localPair;
  }
}

/*
 * Return TRUE if the cursor should 'pass-through' this character
 * (if it is the closing character of an auto-closing pair)
 */
int acp_should_pass_through(char_u c)
{
  if (!p_acp)
  {
    return FALSE;
  }

  return closeCharacter[c] != NULL;
}

char_u acp_get_closing_character(char_u c)
{
  if (!p_acp)
  {
    return NUL;
  }

  pairInfo_T *pair = openCharacter[c];

  if (!pair)
  {
    return NUL;
  }

  return pair->close;
}

int acp_is_opening_pair(char_u c)
{
  if (!p_acp)
  {
    return FALSE;
  }

  return openCharacter[c] != NULL;
}

int acp_is_closing_pair(char_u c)
{
  if (!p_acp)
  {
    return FALSE;
  }

  return closeCharacter[c] != NULL;
}

int acp_is_cursor_between_pair(void)
{
  if (!p_acp)
  {
    return FALSE;
  }

  char_u charBefore = *(ml_get_cursor() - 1);

  if (!openCharacter[charBefore])
  {
    return FALSE;
  }

  char_u charAfter = *(ml_get_cursor());

  return charAfter == openCharacter[charBefore]->close;
}
