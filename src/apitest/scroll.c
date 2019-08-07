#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimExecute("e!");

  vimInput("g");
  vimInput("g");

  vimInput(":");
  vimInput("5");
  vimInput("0");
  vimInput("<cr>");
}

void test_teardown(void) {}

MU_TEST(test_set_get_metrics)
{
  vimWindowSetWidth(80);
  vimWindowSetHeight(10);

  mu_check(vimWindowGetWidth() == 80);
  mu_check(vimWindowGetHeight() == 10);

  vimWindowSetWidth(20);
  vimWindowSetHeight(21);

  mu_check(vimWindowGetWidth() == 20);
  mu_check(vimWindowGetHeight() == 21);

  vimWindowSetWidth(100);
  vimWindowSetHeight(101);

  mu_check(vimWindowGetWidth() == 100);
  printf("HEIGHT: %d\n", vimWindowGetHeight());
  mu_check(vimWindowGetHeight() == 101);
}

MU_TEST(test_simple_scroll)
{
  vimWindowSetWidth(80);
  vimWindowSetHeight(40);

  mu_check(vimCursorGetLine() == 50);

  vimInput("z");
  vimInput("z");
  mu_check(vimWindowGetTopLine() == 31);
  mu_check(vimCursorGetLine() == 50);

  vimInput("z");
  vimInput("b");
  mu_check(vimWindowGetTopLine() == 11);
  mu_check(vimCursorGetLine() == 50);

  vimInput("z");
  vimInput("t");
  mu_check(vimWindowGetTopLine() == 50);
  mu_check(vimCursorGetLine() == 50);
}

MU_TEST(test_small_screen_scroll)
{
  vimWindowSetWidth(80);
  vimWindowSetHeight(3);

  mu_check(vimCursorGetLine() == 50);

  vimInput("z");
  vimInput("z");
  mu_check(vimWindowGetTopLine() == 49);
  mu_check(vimCursorGetLine() == 50);

  vimInput("z");
  vimInput("b");
  mu_check(vimWindowGetTopLine() == 48);
  mu_check(vimCursorGetLine() == 50);

  vimInput("z");
  vimInput("t");
  mu_check(vimWindowGetTopLine() == 50);
  mu_check(vimCursorGetLine() == 50);
}

MU_TEST(test_h_m_l)
{
  vimWindowSetWidth(80);
  vimWindowSetHeight(40);

  mu_check(vimCursorGetLine() == 50);

  vimInput("z");
  vimInput("z");

  vimInput("H");
  mu_check(vimCursorGetLine() == 31);

  vimInput("L");
  mu_check(vimCursorGetLine() == 70);

  vimInput("M");
  mu_check(vimCursorGetLine() == 50);
}

MU_TEST(test_only_scroll_at_boundary)
{

  vimWindowSetWidth(80);
  vimWindowSetHeight(63);

  vimInput("g");
  vimInput("g");
  vimInput("z");
  vimInput("t");

  mu_check(vimWindowGetTopLine() == 1);

  // Verify viewport doesn't scroll even when cursor moves down
  vimInput("6");
  vimInput("2");
  vimInput("j");
  mu_check(vimWindowGetTopLine() == 1);

  // Should scroll now
  vimInput("j");
  mu_check(vimWindowGetTopLine() == 2);

  // Shouldn't scroll moving a single line up
  vimInput("k");
  mu_check(vimWindowGetTopLine() == 2);
}

MU_TEST(test_no_scroll_after_setting_topline)
{
  vimWindowSetWidth(10);
  vimWindowSetHeight(10);

  pos_T pos;
  pos.lnum = 95;
  pos.col = 1;

  vimCursorSetPosition(pos);

  vimWindowSetTopLeft(90, 1);

  mu_check(vimWindowGetTopLine() == 90);
  vimInput("j");

  mu_check(vimWindowGetTopLine() == 90);
  mu_check(vimCursorGetLine() == 96);
}

MU_TEST(test_scroll_left_at_boundary)
{
  vimWindowSetWidth(4);
  vimWindowSetHeight(10);

  vimInput("l");
  mu_check(vimWindowGetLeftColumn() == 0);

  vimInput("l");
  mu_check(vimWindowGetLeftColumn() == 0);

  vimInput("l");
  mu_check(vimWindowGetLeftColumn() == 0);

  vimInput("l");
  mu_check(vimWindowGetLeftColumn() == 1);

  vimInput("l");
  mu_check(vimWindowGetLeftColumn() == 2);
}

MU_TEST(test_no_scroll_after_setting_left)
{
  vimWindowSetWidth(4);
  vimWindowSetHeight(10);

  pos_T pos;
  pos.lnum = 99;
  pos.col = 2;
  vimCursorSetPosition(pos);

  vimWindowSetTopLeft(1, 2);

  vimInput("l");
  mu_check(vimWindowGetLeftColumn() == 2);

  vimInput("l");
  mu_check(vimWindowGetLeftColumn() == 2);

  vimInput("l");
  mu_check(vimWindowGetLeftColumn() == 2);

  vimInput("l");
  mu_check(vimWindowGetLeftColumn() == 3);
}

MU_TEST(test_ctrl_d)
{
  vimWindowSetHeight(50);
  vimInput("g");
  vimInput("g");
  printf("topline: %d\n", 1);

  vimInput("<c-d>");

  printf("topline: %d\n", vimWindowGetTopLine());
  mu_check(vimWindowGetTopLine() == 26);

  vimWindowSetHeight(12);

  vimInput("<c-u>");
  mu_check(vimWindowGetTopLine() == 20);
}

MU_TEST(test_ctrl_f)
{
  vimWindowSetHeight(50);
  vimInput("g");
  vimInput("g");
  printf("topline: %d\n", 1);

  vimInput("<c-f>");

  printf("topline: %d\n", vimWindowGetTopLine());
  mu_check(vimWindowGetTopLine() == 49);

  // When setting the height, the view may not be centered,
  // so the next <c-f> will be a partial scroll
  vimWindowSetHeight(20);

  vimInput("<c-f>");
  // Partial scroll after resize
  printf("topline: %d\n", vimWindowGetTopLine());
  mu_check(vimWindowGetTopLine() == 58);

  // Full scroll
  vimInput("<c-f>");
  printf("topline: %d\n", vimWindowGetTopLine());
  mu_check(vimWindowGetTopLine() == 76);

  // Full scroll
  vimInput("<c-f>");
  printf("topline: %d\n", vimWindowGetTopLine());
  mu_check(vimWindowGetTopLine() == 94);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_set_get_metrics);
  MU_RUN_TEST(test_simple_scroll);
  MU_RUN_TEST(test_small_screen_scroll);
  MU_RUN_TEST(test_h_m_l);
  MU_RUN_TEST(test_only_scroll_at_boundary);
  MU_RUN_TEST(test_no_scroll_after_setting_topline);
  MU_RUN_TEST(test_scroll_left_at_boundary);
  MU_RUN_TEST(test_no_scroll_after_setting_left);
  MU_RUN_TEST(test_ctrl_d);
  MU_RUN_TEST(test_ctrl_f);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimBufferOpen("collateral/lines_100.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
