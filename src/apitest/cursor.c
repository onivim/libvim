#include "libvim.h"
#include "minunit.h"

static int onCursorAddCount = 0;
static pos_T cursors[128];

void test_setup(void)
{
  vimExecute("e!");
  vimKey("<esc>");
  vimKey("<esc>");

  vimInput("g");
  vimInput("g");
  vimInput("0");

  onCursorAddCount = 0;
}

void onCursorAdd(pos_T cursor)
{
  printf("TEST: onCursorAdd - Adding cursor at line: %ld col: %d\n", cursor.lnum, cursor.col);
  cursors[onCursorAddCount] = cursor;
  onCursorAddCount++;
}

void test_teardown(void) {}

MU_TEST(test_set_cursor)
{
  pos_T pos;
  pos.lnum = 5;
  pos.col = 4;
  vimCursorSetPosition(pos);

  mu_check(vimCursorGetLine() == 5);
  mu_check(vimCursorGetColumn() == 4);
}

MU_TEST(test_set_cursor_invalid_line)
{
  pos_T pos;
  pos.lnum = 500;
  pos.col = 4;
  vimCursorSetPosition(pos);

  mu_check(vimCursorGetLine() == 100);
  mu_check(vimCursorGetColumn() == 4);
}

MU_TEST(test_set_cursor_doesnt_move_topline)
{
  vimWindowSetTopLeft(71, 1);
  pos_T pos;
  pos.lnum = 90;
  pos.col = 4;
  vimCursorSetPosition(pos);

  mu_check(vimCursorGetLine() == 90);
  mu_check(vimCursorGetColumn() == 4);
  printf("window topline: %d\n", vimWindowGetTopLine());
  mu_check(vimWindowGetTopLine() == 71);
}

MU_TEST(test_set_cursor_invalid_column)
{
  pos_T pos;
  pos.lnum = 7;
  pos.col = 500;
  vimCursorSetPosition(pos);

  mu_check(vimCursorGetLine() == 7);
  mu_check(vimCursorGetColumn() == 5);
}

MU_TEST(test_add_cursors_visual_I)
{
  vimKey("<c-v>");
  vimInput("j");
  vimInput("j");
  vimInput("I");

  mu_check(cursors[0].lnum == 1);
  mu_check(cursors[0].col == 0);

  mu_check(cursors[1].lnum == 2);
  mu_check(cursors[1].col == 0);

  mu_check(onCursorAddCount == 2);

  mu_check(vimCursorGetLine() == 3);
  mu_check(vimCursorGetColumn() == 0);

  // Verify we switch to insert mode
  mu_check((vimGetMode() & INSERT) == INSERT);
}

MU_TEST(test_add_cursors_visual_reverse_I)
{
  vimInput("j");
  vimInput("j");
  vimKey("<c-v>");
  vimInput("k");
  vimInput("k");

  pos_T startPos;
  pos_T endPos;

  vimVisualGetRange(&startPos, &endPos);

  vimInput("I");

  mu_check(cursors[0].lnum == 2);
  mu_check(cursors[0].col == 0);

  mu_check(cursors[1].lnum == 3);
  mu_check(cursors[1].col == 0);

  mu_check(onCursorAddCount == 2);

  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);

  // Verify we switch to insert mode
  mu_check((vimGetMode() & INSERT) == INSERT);
}

MU_TEST(test_add_cursors_visual_after)
{
  vimKey("<c-v>");
  vimInput("j");
  vimInput("j");
  vimInput("A");

  mu_check(cursors[0].lnum == 1);
  mu_check(cursors[0].col == 1);

  mu_check(cursors[1].lnum == 2);
  mu_check(cursors[1].col == 1);

  mu_check(onCursorAddCount == 2);

  mu_check(vimCursorGetLine() == 3);
  mu_check(vimCursorGetColumn() == 1);

  // Verify we switch to insert mode
  mu_check((vimGetMode() & INSERT) == INSERT);
}

MU_TEST(test_add_cursors_visual_skip_empty_line)
{
  // Add an empty line up top
  char_u *lines[] = {"abc", "", "def"};

  vimBufferSetLines(curbuf, 0, 0, lines, 3);
  vimKey("<c-v>");
  vimInput("j");
  vimInput("j");
  vimInput("l");
  vimInput("I");

  mu_check(cursors[0].lnum == 1);
  mu_check(cursors[0].col == 1);

  mu_check(onCursorAddCount == 1);

  mu_check(vimCursorGetLine() == 3);
  mu_check(vimCursorGetColumn() == 1);

  // Verify we switch to insert mode
  mu_check((vimGetMode() & INSERT) == INSERT);
}

MU_TEST(test_add_cursors_visual_utf8_vcol)
{
  // Add an empty line up top
  char_u *lines[] = {"abc", "κόσμε", "def"};

  vimBufferSetLines(curbuf, 0, 0, lines, 3);
  vimKey("<c-v>");
  // Move two lines down
  vimInput("j");
  vimInput("j");
  //  Move two characters to the right (`de|f`)
  vimInput("l");
  vimInput("l");
  vimInput("I");

  mu_check(cursors[0].lnum == 1);
  mu_check(cursors[0].col == 2);

  // Verify we're on the proper byte...
  mu_check(cursors[1].lnum == 2);
  mu_check(cursors[1].col == 5);

  mu_check(onCursorAddCount == 2);

  mu_check(vimCursorGetLine() == 3);
  mu_check(vimCursorGetColumn() == 2);

  // Verify we switch to insert mode
  mu_check((vimGetMode() & INSERT) == INSERT);
}

// Verify the primary cursor ends up past EOL when transitioning to insert mode
MU_TEST(test_add_cursors_eol)
{
  // Add an empty line up top
  char_u *lines[] = {"abc", "def", "ghi"};

  vimBufferSetLines(curbuf, 0, 0, lines, 3);
  vimKey("<c-v>");
  // Move two lines down
  vimInput("j");
  vimInput("j");
  //  Move two characters to the right (`de|f`)
  vimInput("l");
  vimInput("l");
  vimInput("A");

  mu_check(cursors[0].lnum == 1);
  mu_check(cursors[0].col == 3);

  // Verify we're on the proper byte...
  mu_check(cursors[1].lnum == 2);
  mu_check(cursors[1].col == 3);

  mu_check(onCursorAddCount == 2);

  mu_check(vimCursorGetLine() == 3);
  mu_check(vimCursorGetColumn() == 3);

  // Verify we switch to insert mode
  mu_check((vimGetMode() & INSERT) == INSERT);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_set_cursor);
  MU_RUN_TEST(test_set_cursor_invalid_line);
  MU_RUN_TEST(test_set_cursor_invalid_column);
  MU_RUN_TEST(test_set_cursor_doesnt_move_topline);

  MU_RUN_TEST(test_add_cursors_visual_I);
  MU_RUN_TEST(test_add_cursors_visual_reverse_I);
  MU_RUN_TEST(test_add_cursors_visual_after);
  MU_RUN_TEST(test_add_cursors_visual_skip_empty_line);
  MU_RUN_TEST(test_add_cursors_visual_utf8_vcol);
  MU_RUN_TEST(test_add_cursors_eol);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  win_setwidth(80);
  win_setheight(40);

  vimBufferOpen("collateral/lines_100.txt", 1, 0);
  vimSetCursorAddCallback(&onCursorAdd);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
