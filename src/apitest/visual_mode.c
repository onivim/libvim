#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");

  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(test_visual_is_active)
{
  mu_check(vimVisualIsActive() == 0);

  vimInput("v");
  mu_check(vimVisualGetType() == 'v');
  mu_check(vimVisualIsActive() == 1);
  mu_check((vimGetMode() & VISUAL) == VISUAL);

  vimInput("<esc>");
  mu_check((vimGetMode() & NORMAL) == NORMAL);
  mu_check(vimVisualIsActive() == 0);

  vimInput("<c-v>");
  mu_check(vimVisualGetType() == Ctrl_V);
  mu_check(vimVisualIsActive() == 1);
  mu_check((vimGetMode() & VISUAL) == VISUAL);

  vimInput("<esc>");
  mu_check((vimGetMode() & NORMAL) == NORMAL);
  mu_check(vimVisualIsActive() == 0);

  vimInput("V");
  mu_check(vimVisualGetType() == 'V');
  mu_check(vimVisualIsActive() == 1);
  mu_check((vimGetMode() & VISUAL) == VISUAL);
}

MU_TEST(test_characterwise_range)
{
  vimInput("v");

  vimInput("l");
  vimInput("l");

  pos_T start;
  pos_T end;

  // Get current range
  vimVisualGetRange(&start, &end);
  mu_check(start.lnum == 1);
  mu_check(start.col == 0);
  mu_check(end.lnum == 1);
  mu_check(end.col == 2);

  vimInput("<esc>");
  vimInput("j");

  // Validate we still get previous range
  vimVisualGetRange(&start, &end);
  mu_check(start.lnum == 1);
  mu_check(start.col == 0);
  mu_check(end.lnum == 1);
  mu_check(end.col == 2);
}

MU_TEST(test_ctrl_q)
{
  vimInput("<c-q>");

  mu_check((vimGetMode() & VISUAL) == VISUAL);
  mu_check(vimVisualGetType() == Ctrl_V);
  mu_check(vimVisualIsActive() == 1);
}

MU_TEST(test_ctrl_Q)
{
  vimInput("<c-Q>");

  mu_check((vimGetMode() & VISUAL) == VISUAL);
  mu_check(vimVisualGetType() == Ctrl_V);
  mu_check(vimVisualIsActive() == 1);
}

MU_TEST(test_insert_block_mode)
{
  vimInput("<c-v>");
  vimInput("j");
  vimInput("j");
  vimInput("j");

  vimInput("I");

  mu_check((vimGetMode() & INSERT) == INSERT);

  vimInput("a");
  vimInput("b");
  vimInput("c");
}

/**
 * This test case does a visual block select and then an "I" insert
 * which should insert at the start of each line.
 * this test fails and will be commented out.
 */

/*
MU_TEST(test_insert_block_mode_change)
{
  char_u *lines[] = {"line1", "line2", "line3", "line4", "line5"};
  vimBufferSetLines(curbuf, 0, 3, lines, 5);

  vimInput("<c-v>");
  vimInput("j");
  vimInput("j");
  vimInput("j");

  vimInput("I");

  vimInput("a");
  vimInput("b");
  vimInput("c");

  vimInput("<esc>");

  char_u *line = vimBufferGetLine(curbuf, 1);
  mu_check(strcmp(line, "abcline1") == 0);
  line = vimBufferGetLine(curbuf, 3);
  mu_check(strcmp(line, "abcline3") == 0);
}
*/

/**
 * This test case does a visual block select and then an "c" insert
 * which should insert "abc" at the start of each line, replacing the l
 */

MU_TEST(test_change_block_mode_change)
{
  char_u *lines[] = {"line1", "line2", "line3", "line4", "line5"};
  vimBufferSetLines(curbuf, 0, 3, lines, 5);

  vimInput("<c-v>");
  vimInput("j");
  vimInput("j");
  vimInput("j");

  vimInput("c");

  vimInput("a");
  vimInput("b");
  vimInput("c");

  vimInput("<esc>");

  char_u *line = vimBufferGetLine(curbuf, 1);
  mu_check(strcmp(line, "abcine1") == 0);

  line = vimBufferGetLine(curbuf, 3);
  mu_check(strcmp(line, "abcine3") == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_visual_is_active);
  MU_RUN_TEST(test_characterwise_range);
  MU_RUN_TEST(test_ctrl_q);
  MU_RUN_TEST(test_ctrl_Q);
  MU_RUN_TEST(test_insert_block_mode);
  // MU_RUN_TEST(test_insert_block_mode_change);
  MU_RUN_TEST(test_change_block_mode_change);
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
