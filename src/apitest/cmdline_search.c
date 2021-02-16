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

MU_TEST(test_search_forward_esc)
{
  vimInput("/");
  vimInput("s");
  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 4);
  mu_check(strcmp(vimSearchGetPattern(), "s") == 0);

  vimInput("t");
  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 17);
  mu_check(strcmp(vimSearchGetPattern(), "st") == 0);
  vimKey("<cr>");

  // Note - while in `incsearch`, the positions
  // returned match the END of the match.
  // That's why there is a difference in the column when pressing <CR>
  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 15);

  vimInput("n");
  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 30);

  vimInput("n");
  mu_check(vimCursorGetLine() == 2);
  mu_check(vimCursorGetColumn() == 31);

  vimInput("n");
  mu_check(vimCursorGetLine() == 3);
  mu_check(vimCursorGetColumn() == 30);

  vimInput("N");
  mu_check(vimCursorGetLine() == 2);
  mu_check(vimCursorGetColumn() == 31);

  vimInput("N");
  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 30);

  mu_check(strcmp(vimSearchGetPattern(), "st") == 0);
}

MU_TEST(test_cancel_inc_search)
{
  vimInput("/");
  vimInput("s");
  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 4);
  mu_check(strcmp(vimSearchGetPattern(), "s") == 0);

  vimInput("t");
  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 17);
  mu_check(strcmp(vimSearchGetPattern(), "st") == 0);
  vimInput("<c-c>");

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);
}

MU_TEST(test_cancel_n)
{
  // Start a query
  vimInput("/");
  vimInput("e");
  vimInput("s");
  vimKey("<cr>");

  // Create a new query, then cancel
  vimInput("/");
  vimInput("a");
  vimKey("<c-c>");

  // n / N should use the previous query
  vimInput("n");
  mu_check(vimCursorGetLine() == 2);
  mu_check(vimCursorGetColumn() == 30);

  vimInput("n");
  mu_check(vimCursorGetLine() == 3);
  mu_check(vimCursorGetColumn() == 29);
}

MU_TEST(test_get_search_highlights_during_visual)
{
  int vim_num_search_highlights;
  searchHighlight_T *vim_search_highlights;

  vimInput("V");
  vimKey("<down>");
  vimKey("<down>");
  vimInput(":s/vvvv");
  vimKey("<esc>");

  vimSearchGetHighlights(curbuf, 1, 3, &vim_num_search_highlights, &vim_search_highlights);
}

MU_TEST(test_insert_literal_ctrl_v)
{
  vimInput("/");
  vimKey("<C-v>");
  vimInput("1");
  vimInput("2");
  vimInput("3");

  mu_check(strcmp(vimSearchGetPattern(), "{") == 0);
}

MU_TEST(test_insert_literal_ctrl_q)
{
  vimInput("/");
  vimKey("<C-q>");
  vimInput("1");
  vimInput("2");
  vimInput("6");
  // Tack a number after, just to make sure it gets input
  // and not swallowed by insert_literal
  vimInput("7");

  mu_check(strcmp(vimSearchGetPattern(), "~7") == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_cancel_inc_search);
  MU_RUN_TEST(test_search_forward_esc);
  MU_RUN_TEST(test_cancel_n);
  MU_RUN_TEST(test_get_search_highlights_during_visual);
  MU_RUN_TEST(test_insert_literal_ctrl_v);
  MU_RUN_TEST(test_insert_literal_ctrl_q);
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
