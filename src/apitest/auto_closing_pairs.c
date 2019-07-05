#include "libvim.h"
#include "minunit.h"
#include "vim.h"

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
  vimInput("0");

  vimExecute("set acp");

  autoClosingPair_T *pairs =
      (autoClosingPair_T *)(alloc(sizeof(autoClosingPair_T) * 3));
  pairs[0].open = '{';
  pairs[0].close = '}';
  pairs[1].open = '[';
  pairs[1].close = ']';
  pairs[2].open = '"';
  pairs[2].close = '"';

  acp_set_pairs(pairs, 3);
  vim_free(pairs);
}

void test_teardown(void) {}

MU_TEST(test_matching_pair_undo_redo)
{
  vimInput("i");
  vimInput("{");
  vimInput("[");
  vimInput("<esc>");

  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "{[]}This is the first line of a test file") == 0);

  vimInput("u");
  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "This is the first line of a test file") == 0);

  vimInput("<c-r>");
  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "{[]}This is the first line of a test file") == 0);
}

MU_TEST(test_matching_pair_dot)
{
  vimInput("A");
  vimInput("a");
  vimInput("b");
  vimInput("c");
  vimInput("{");
  vimInput("[");
  vimInput("{");
  vimInput("d");
  vimInput("<esc>");

  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "This is the first line of a test fileabc{[{d}]}") == 0);

  vimInput("j");
  vimInput(".");

  mu_check(strcmp(vimBufferGetLine(curbuf, 2),
                  "This is the second line of a test fileabc{[{d}]}") == 0);

  vimInput("j");
  vimInput(".");

  mu_check(strcmp(vimBufferGetLine(curbuf, 3),
                  "This is the third line of a test fileabc{[{d}]}") == 0);
}

MU_TEST(test_matching_pair_macro)
{
  vimInput("q");
  vimInput("a");
  vimInput("I");
  vimInput("{");
  vimInput("[");
  vimInput("{");
  vimInput("<bs>");
  vimInput("d");
  vimInput("<esc>");
  vimInput("q");

  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "{[d]}This is the first line of a test file") == 0);

  vimInput("j");
  vimInput("@");
  vimInput("a");

  mu_check(strcmp(vimBufferGetLine(curbuf, 2),
                  "{[d]}This is the second line of a test file") == 0);

  vimInput("j");
  vimInput("@");
  vimInput("@");

  mu_check(strcmp(vimBufferGetLine(curbuf, 3),
                  "{[d]}This is the third line of a test file") == 0);
}

MU_TEST(test_backspace_matching_pair)
{
  vimInput("i");
  vimInput("{");
  vimInput("[");

  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "{[]}This is the first line of a test file") == 0);

  vimInput("<bs>");
  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "{}This is the first line of a test file") == 0);

  vimInput("<bs>");
  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "This is the first line of a test file") == 0);
}

MU_TEST(test_enter_between_pairs)
{
  vimInput("I");
  vimInput("{");
  vimInput("<cr>");
  vimInput("a");
  vimInput("b");
  vimInput("<esc>");

  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "{") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 2), "\tab") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 3),
                  "}This is the first line of a test file") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 4),
                  "This is the second line of a test file") == 0);
}

MU_TEST(test_enter_between_pairs_undo)
{
  vimInput("I");
  vimInput("{");
  vimInput("<cr>");
  vimInput("a");
  vimInput("b");
  vimInput("<esc>");

  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "{") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 2), "\tab") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 3),
                  "}This is the first line of a test file") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 4),
                  "This is the second line of a test file") == 0);

  vimInput("u");
  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "This is the first line of a test file") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 2),
                  "This is the second line of a test file") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 3),
                  "This is the third line of a test file") == 0);

  vimInput("<c-r>");

  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "{") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 2), "\tab") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 3),
                  "}This is the first line of a test file") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 4),
                  "This is the second line of a test file") == 0);
}

MU_TEST(test_enter_between_pairs_dot)
{
  vimInput("I");
  vimInput("{");
  vimInput("<cr>");
  vimInput("a");
  vimInput("b");
  vimInput("<esc>");

  vimInput("4");
  vimInput("G");
  mu_check(vimCursorGetLine() == 4);
  vimInput(".");

  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "{") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 2), "\tab") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 3),
                  "}This is the first line of a test file") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 4), "{") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 5), "\tab") == 0);
  mu_check(strcmp(vimBufferGetLine(curbuf, 6),
                  "}This is the second line of a test file") == 0);
}

MU_TEST(test_pass_through_in_pairs)
{
  vimInput("I");
  vimInput("{");
  vimInput("}");
  vimInput("a");
  vimInput("<esc>");

  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "{}aThis is the first line of a test file") == 0);

  vimInput("j");
  vimInput(".");

  mu_check(strcmp(vimBufferGetLine(curbuf, 2),
                  "{}aThis is the second line of a test file") == 0);
}

MU_TEST(test_pass_through_in_pairs_undo_redo)
{
  vimInput("I");
  vimInput("{");
  vimInput("}");
  vimInput("a");
  vimInput("<esc>");

  vimInput("u");
  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "This is the first line of a test file") == 0);

  vimInput("<c-r>");
  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "{}aThis is the first line of a test file") == 0);
}

MU_TEST(test_matching_pair_double_quotes)
{
  vimInput("I");
  vimInput("\"");
  vimInput("a");

  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "\"a\"This is the first line of a test file") == 0);
  vimInput("\"");
  vimInput("b");
  vimInput("<esc>");

  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "\"a\"bThis is the first line of a test file") == 0);

  vimInput("u");
  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "This is the first line of a test file") == 0);

  vimInput("<c-r>");
  mu_check(strcmp(vimBufferGetLine(curbuf, 1),
                  "\"a\"bThis is the first line of a test file") == 0);
}

MU_TEST(test_setting_acp_option)
{
  vimExecute("set autoclosingpairs");
  mu_check(p_acp == TRUE);

  vimExecute("set noautoclosingpairs");
  mu_check(p_acp == FALSE);

  vimExecute("set acp");
  mu_check(p_acp == TRUE);

  vimExecute("set noacp");
  mu_check(p_acp == FALSE);
}

MU_TEST(test_acp_should_pass_through)
{
  mu_check(acp_should_pass_through('a') == FALSE);
}

MU_TEST(test_pass_through_last_character)
{
  vimInput("o");
  vimInput("{");
  vimInput("}");
  vimInput("a");
  vimInput("<esc>");

  mu_check(strcmp(vimBufferGetLine(curbuf, 2),
                  "{}a") == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_setting_acp_option);
  MU_RUN_TEST(test_acp_should_pass_through);
  MU_RUN_TEST(test_matching_pair_undo_redo);
  MU_RUN_TEST(test_matching_pair_dot);
  MU_RUN_TEST(test_matching_pair_macro);
  MU_RUN_TEST(test_backspace_matching_pair);
  MU_RUN_TEST(test_enter_between_pairs);
  MU_RUN_TEST(test_enter_between_pairs_undo);
  MU_RUN_TEST(test_enter_between_pairs_dot);
  MU_RUN_TEST(test_pass_through_in_pairs);
  MU_RUN_TEST(test_pass_through_in_pairs_undo_redo);
  MU_RUN_TEST(test_matching_pair_double_quotes);
  MU_RUN_TEST(test_pass_through_last_character);
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
