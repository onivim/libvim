#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimExecute("e!");
  vimKey("<esc>");
  vimKey("<esc>");
  vimInput("g");
  vimInput("g");
}

void test_teardown(void) {}

MU_TEST(test_G_gg)
{
  mu_check(vimCursorGetLine() == 1);

  vimInput("G");

  mu_check(vimCursorGetLine() == 3);

  vimInput("g");
  vimInput("g");

  mu_check(vimCursorGetLine() == 1);
}

MU_TEST(test_j_k)
{
  mu_check(vimCursorGetLine() == 1);

  vimInput("j");

  mu_check(vimCursorGetLine() == 2);

  vimInput("k");

  mu_check(vimCursorGetLine() == 1);
}

MU_TEST(test_2j_2k)
{
  mu_check(vimCursorGetLine() == 1);

  vimInput("2");
  vimInput("j");

  mu_check(vimCursorGetLine() == 3);

  vimInput("2");
  vimInput("k");

  mu_check(vimCursorGetLine() == 1);
}

MU_TEST(test_forward_search)
{
  // Move to very beginning
  vimKey("g");
  vimKey("g");
  vimKey("0");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);

  // Search forwards to first 'line'
  vimKey("/");
  vimInput("line");
  vimKey("<cr>");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 18);

  // Search again from here
  vimKey("<esc>");
  vimKey("<esc>");

  vimKey("/");
  vimInput("line");
  vimKey("<cr>");

  mu_check(vimCursorGetLine() == 2);
  mu_check(vimCursorGetColumn() == 19);
}

MU_TEST(test_reverse_search)
{
  // Move to second line, first byte
  vimKey("j");
  vimKey("0");

  mu_check(vimCursorGetLine() == 2);

  // Search backwards to first
  vimKey("?");
  vimInput("line");
  vimKey("<cr>");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 18);

  // Starting from match, searching backwards again
  vimKey("<esc>");
  vimKey("<esc>");

  vimKey("?");
  vimInput("line");
  vimKey("<cr>");

  // Serach should loop back
  mu_check(vimCursorGetLine() == 3);
  mu_check(vimCursorGetColumn() == 18);
}

MU_TEST(test_forward_search_with_delete_operator)
{
  // Delete, searching forward
  vimInput("d");
  vimKey("/");
  vimInput("line");
  vimKey("<cr>");

  mu_check((vimGetMode() & NORMAL) == NORMAL);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "line of a test file") == 0);
};

MU_TEST(test_backward_search_with_delete_operator)
{
  vimInput("$"); // Go to end of line
  // Delete, searching forward
  vimInput("d");
  vimKey("?");
  vimInput("line");
  vimKey("<cr>");

  mu_check((vimGetMode() & NORMAL) == NORMAL);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "This is the first e") == 0);
};

MU_TEST(test_forward_search_with_change_operator)
{
  // Move to second line, first byte

  // Change forwards, to first
  vimInput("c");
  vimKey("/");
  vimInput("line");
  vimKey("<cr>");
  vimKey("a");

  mu_check((vimGetMode() & INSERT) == INSERT);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "aline of a test file") == 0);

  vimKey("<esc>");
  mu_check((vimGetMode() & NORMAL) == NORMAL);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "aline of a test file") == 0);
};

MU_TEST(test_backward_search_with_change_operator)
{
  // Move to last byte in first line
  vimInput("$");

  // Change forwards, to first
  vimInput("c");
  vimKey("?");
  vimInput("line");
  vimKey("<cr>");
  vimKey("a");

  mu_check((vimGetMode() & INSERT) == INSERT);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "This is the first ae") == 0);

  vimKey("<esc>");
  mu_check((vimGetMode() & NORMAL) == NORMAL);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "This is the first ae") == 0);
};

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_G_gg);
  MU_RUN_TEST(test_j_k);
  MU_RUN_TEST(test_2j_2k);
  MU_RUN_TEST(test_forward_search);
  MU_RUN_TEST(test_reverse_search);
  MU_RUN_TEST(test_forward_search_with_delete_operator);
  MU_RUN_TEST(test_backward_search_with_delete_operator);
  MU_RUN_TEST(test_forward_search_with_change_operator);
  MU_RUN_TEST(test_backward_search_with_change_operator);
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
