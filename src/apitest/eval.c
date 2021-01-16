
#include "libvim.h"
#include "minunit.h"

static int getCharLastMode = -2;
static char getCharReturn = 0;
static int getCharReturnMod = 0;

int onGetchar(int mode, char *c, int *modMask)
{
  getCharLastMode = mode;
  *c = getCharReturn;
  *modMask = getCharReturnMod;
  printf("onGetChar called with mode: %d\n", mode);
  return OK;
};

void test_setup(void)
{
  vimKey("<esc>");
  vimKey("<esc>");

  vimExecute("e!");
  vimInput("g");
  vimInput("g");
  vimInput("0");

  getCharLastMode = -2;
  getCharReturn = 0;
  getCharReturnMod = 0;
}

void test_teardown(void) {}

void onMessage(char_u *title, char_u *msg, msgPriority_T priority)
{
  printf("onMessage - title: |%s| contents: |%s|", title, msg);
};

MU_TEST(test_simple_addition)
{
  char_u *result = vimEval("2+2");

  mu_check(strcmp(result, "4") == 0);
  vim_free(result);
}

MU_TEST(test_empty)
{
  char_u *result = vimEval("");

  mu_check(result == NULL);
}

MU_TEST(test_exe_norm_delete_line)
{
  mu_check(vimBufferGetLineCount(curbuf) == 3);
  vimExecute("source collateral/ex_normal.vim");
  vimExecute("call NormDeleteLine()");
  mu_check(vimBufferGetLineCount(curbuf) == 2);
}

MU_TEST(test_exe_norm_insert_character)
{
  mu_check(vimBufferGetLineCount(curbuf) == 3);
  vimExecute("source collateral/ex_normal.vim");
  vimExecute("call NormInsertCharacter()");
  mu_check(vimBufferGetLineCount(curbuf) == 3);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "aThis is the first line of a test file") == 0);
}

MU_TEST(test_exe_norm_insert_character_both_sides)
{
  mu_check(vimBufferGetLineCount(curbuf) == 3);
  vimExecute("source collateral/ex_normal.vim");
  vimExecute("call NormInsertCharacterBothSides()");
  mu_check(vimBufferGetLineCount(curbuf) == 3);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "aThis is the first line of a test fileb") == 0);
}

MU_TEST(test_exe_norm_insert_character_both_sides_multiple_lines)
{
  mu_check(vimBufferGetLineCount(curbuf) == 3);
  vimExecute("source collateral/ex_normal.vim");
  vimExecute("call NormInsertCharacterBothSidesMultipleLines()");
  mu_check(vimBufferGetLineCount(curbuf) == 3);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "aThis is the first line of a test fileb") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 2), "aThis is the second line of a test fileb") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 3), "aThis is the third line of a test fileb") == 0);
}

MU_TEST(test_range_norm_insert_all_lines)
{
  mu_check(vimBufferGetLineCount(curbuf) == 3);
  vimExecute("g/line/norm! Ia");
  mu_check(vimBufferGetLineCount(curbuf) == 3);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "aThis is the first line of a test file") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 2), "aThis is the second line of a test file") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 3), "aThis is the third line of a test file") == 0);
}

MU_TEST(test_range_norm_insert_single_line)
{
  mu_check(vimBufferGetLineCount(curbuf) == 3);
  vimExecute("g/second/norm! Ia");
  mu_check(vimBufferGetLineCount(curbuf) == 3);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "This is the first line of a test file") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 2), "aThis is the second line of a test file") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 3), "This is the third line of a test file") == 0);
}

MU_TEST(test_inverse_range_norm)
{
  mu_check(vimBufferGetLineCount(curbuf) == 3);
  vimExecute("g!/second/norm! Ia");
  mu_check(vimBufferGetLineCount(curbuf) == 3);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "aThis is the first line of a test file") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 2), "This is the second line of a test file") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 3), "aThis is the third line of a test file") == 0);
}

MU_TEST(test_getchar)
{
  // Getchar with no arguments
  getCharReturn = 'a';
  char_u *szNoArgs = vimEval("getchar()");
  mu_check(getCharLastMode == -1);
  mu_check(strcmp(szNoArgs, "97") == 0);
  vim_free(szNoArgs);

  getCharReturn = 0;
  char_u *szOne = vimEval("getchar(1)");
  mu_check(getCharLastMode == 1);
  mu_check(strcmp(szOne, "0") == 0);
  vim_free(szOne);

  getCharReturn = 'b';
  char_u *szZero = vimEval("getchar(0)");
  mu_check(getCharLastMode == 0);
  mu_check(strcmp(szZero, "98") == 0);
  vim_free(szZero);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_simple_addition);
  MU_RUN_TEST(test_empty);
  MU_RUN_TEST(test_exe_norm_delete_line);
  MU_RUN_TEST(test_exe_norm_insert_character);
  MU_RUN_TEST(test_exe_norm_insert_character_both_sides);
  MU_RUN_TEST(test_exe_norm_insert_character_both_sides_multiple_lines);
  MU_RUN_TEST(test_range_norm_insert_all_lines);
  MU_RUN_TEST(test_range_norm_insert_single_line);
  MU_RUN_TEST(test_inverse_range_norm);

  MU_RUN_TEST(test_getchar);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);
  vimSetMessageCallback(&onMessage);
  vimSetFunctionGetCharCallback(&onGetchar);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
