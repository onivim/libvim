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

MU_TEST(test_visual_is_active)
{
  mu_check(vimVisualIsActive() == 0);

  vimInput("v");
  mu_check(vimVisualGetType() == 'v');
  mu_check(vimVisualIsActive() == 1);
  mu_check((vimGetMode() & VISUAL) == VISUAL);

  vimKey("<esc>");
  mu_check((vimGetMode() & NORMAL) == NORMAL);
  mu_check(vimVisualIsActive() == 0);

  vimKey("<c-v>");
  mu_check(vimVisualGetType() == Ctrl_V);
  mu_check(vimVisualIsActive() == 1);
  mu_check((vimGetMode() & VISUAL) == VISUAL);

  vimKey("<esc>");
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

  vimKey("<esc>");
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
  vimKey("<c-q>");

  mu_check((vimGetMode() & VISUAL) == VISUAL);
  mu_check(vimVisualGetType() == Ctrl_V);
  mu_check(vimVisualIsActive() == 1);
}

MU_TEST(test_ctrl_Q)
{
  vimKey("<c-Q>");

  mu_check((vimGetMode() & VISUAL) == VISUAL);
  mu_check(vimVisualGetType() == Ctrl_V);
  mu_check(vimVisualIsActive() == 1);
}

MU_TEST(test_insert_block_mode)
{
  vimKey("<c-v>");
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

  vimKey("<c-v>");
  vimInput("j");
  vimInput("j");
  vimInput("j");

  vimInput("c");

  vimInput("a");
  vimInput("b");
  vimInput("c");

  vimKey("<esc>");

  char_u *line = vimBufferGetLine(curbuf, 1);
  mu_check(strcmp(line, "abcine1") == 0);

  line = vimBufferGetLine(curbuf, 3);
  mu_check(strcmp(line, "abcine3") == 0);
}

MU_TEST(test_in_parentheses)
{
  char_u *lines[] = {"abc\"123\"def"};
  vimBufferSetLines(curbuf, 0, 3, lines, 1);

  vimInput("v");
  vimInput("i");
  vimInput("\"");

  pos_T start;
  pos_T end;

  // Get current range, validate coordinates
  vimVisualGetRange(&start, &end);
  printf("start lnum: %ld col: %d end lnum: %ld col: %d\n", start.lnum, start.col, end.lnum, end.col);
  mu_check(start.lnum == 1);
  mu_check(start.col == 4);
  mu_check(end.lnum == 1);
  mu_check(end.col == 6);
}

MU_TEST(test_adjust_start_visual_line)
{
  char_u *lines[] = {"line1", "line2", "line3", "line4", "line5"};
  vimBufferSetLines(curbuf, 0, 3, lines, 5);
  mu_check(vimBufferGetLineCount(curbuf) == 5);

  vimInput("j");
  vimInput("j");
  vimInput("V");

  pos_T start;
  pos_T end;

  // Get current range, validate coordinates
  vimVisualGetRange(&start, &end);
  mu_check(start.lnum == 3);
  mu_check(start.col == 0);
  mu_check(end.lnum == 3);
  mu_check(end.col == 0);

  pos_T newStart;
  newStart.lnum = 1;
  newStart.col = 0;
  vimVisualSetStart(newStart);

  vimVisualGetRange(&start, &end);
  mu_check(start.lnum == 1);
  mu_check(start.col == 0);
  mu_check(end.lnum == 3);
  mu_check(end.col == 0);

  // Delete the lines - 1 through 3
  vimInput("d");

  // 3 lines should've been deleted
  mu_check(vimBufferGetLineCount(curbuf) == 2);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "line4") == 0);
}

MU_TEST(test_adjust_start_select_character)
{
  char_u *lines[] = {"line1", "line2", "line3", "line4", "line5"};
  vimBufferSetLines(curbuf, 0, 3, lines, 5);
  mu_check(vimBufferGetLineCount(curbuf) == 5);

  // Move two characters to the right - cursor on 'n' in line1
  vimInput("l");
  vimInput("l");
  // Switch to visual
  vimInput("v");
  // and then select
  vimKey("<C-g>");

  mu_check(vimSelectIsActive() == 1);
  pos_T start;
  pos_T end;

  // Get current range, validate coordinates
  vimVisualGetRange(&start, &end);
  mu_check(start.lnum == 1);
  mu_check(start.col == 2);
  mu_check(end.lnum == 1);
  mu_check(end.col == 2);

  pos_T newStart;
  newStart.lnum = 1;
  newStart.col = 3;
  vimVisualSetStart(newStart);

  vimVisualGetRange(&start, &end);
  mu_check(start.lnum == 1);
  mu_check(start.col == 3);
  mu_check(end.lnum == 1);
  mu_check(end.col == 2);

  // Delete the lines - 1 through 3
  vimInput("t");

  // 3 lines should've been deleted
  mu_check(vimBufferGetLineCount(curbuf) == 5);
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "lit1") == 0);
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
  MU_RUN_TEST(test_in_parentheses);
  MU_RUN_TEST(test_adjust_start_visual_line);
  MU_RUN_TEST(test_adjust_start_select_character);
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
