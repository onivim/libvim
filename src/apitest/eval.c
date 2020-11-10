
#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimKey("<esc>");
  vimKey("<esc>");

  vimExecute("e!");
  vimInput("g");
  vimInput("g");
  vimInput("0");
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
  char_u* line = vimBufferGetLine(curbuf, 1);
  printf("LINE: %s\n", line);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "aThis is the first line of a test file") == 0);
}

MU_TEST(test_exe_norm_insert_character_both_sides)
{
  mu_check(vimBufferGetLineCount(curbuf) == 3);
  vimExecute("source collateral/ex_normal.vim");
  vimExecute("call NormInsertCharacterBothSides()");
  mu_check(vimBufferGetLineCount(curbuf) == 3);
  char_u* line = vimBufferGetLine(curbuf, 1);
  printf("LINE: %s\n", line);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "aThis is the first line of a test fileb") == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_simple_addition);
  MU_RUN_TEST(test_empty);
  MU_RUN_TEST(test_exe_norm_delete_line);
  MU_RUN_TEST(test_exe_norm_insert_character);
  MU_RUN_TEST(test_exe_norm_insert_character_both_sides);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);
  vimSetMessageCallback(&onMessage);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
