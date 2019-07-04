#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(test_cmd_key_insert)
{
  vimInput("o");
  vimInput("<D-A>");

  mu_check(strcmp(vimBufferGetLine(curbuf, 2), "") == 0);
}

MU_TEST(test_cmd_key_binding)
{
  vimExecute("inoremap <D-A> b");

  vimInput("o");
  vimInput("<D-A>");

  mu_check(strcmp(vimBufferGetLine(curbuf, 2), "b") == 0);
}

MU_TEST(test_arrow_keys_normal)
{
  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("<Right>");
  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 1);

  vimInput("<Down>");
  mu_check(vimCursorGetLine() == 2);
  mu_check(vimCursorGetColumn() == 1);

  vimInput("<Left>");
  mu_check(vimCursorGetLine() == 2);
  mu_check(vimCursorGetColumn() == 0);

  vimInput("<Up>");
  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_arrow_keys_normal);
  MU_RUN_TEST(test_cmd_key_insert);
  MU_RUN_TEST(test_cmd_key_binding);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
