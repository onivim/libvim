#include "libvim.h"
#include "minunit.h"

static scrollDirection_T lastScrollDirection;
static long lastScrollCount = 1;
static int scrollRequestCount = 0;

void onScroll(scrollDirection_T dir, long count)
{
  lastScrollDirection = dir;
  lastScrollCount = count;
  scrollRequestCount++;
}

void test_setup(void)
{
  vimKey("<esc>");
  vimKey("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");

  vimInput(":");
  vimInput("5");
  vimInput("0");

  vimKey("<cr>");
  scrollRequestCount = 0;
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
  mu_check(vimWindowGetHeight() == 101);
}

MU_TEST(test_zz_zb_zt)
{
  vimInput("z");
  vimInput("z");

  mu_check(scrollRequestCount == 1);
  mu_check(lastScrollDirection == SCROLL_CURSOR_CENTERV);
  mu_check(lastScrollCount == 1);

  vimInput("z");
  vimInput("b");

  mu_check(scrollRequestCount == 2);
  mu_check(lastScrollDirection == SCROLL_CURSOR_BOTTOM);
  mu_check(lastScrollCount == 1);

  vimInput("z");
  vimInput("t");
  mu_check(scrollRequestCount == 3);
  mu_check(lastScrollDirection == SCROLL_CURSOR_TOP);
  mu_check(lastScrollCount == 1);
}

MU_TEST(test_zs_ze)
{
  vimInput("z");
  vimInput("s");

  mu_check(scrollRequestCount == 1);
  mu_check(lastScrollDirection == SCROLL_CURSOR_LEFT);
  mu_check(lastScrollCount == 1);

  vimInput("z");
  vimInput("e");

  mu_check(scrollRequestCount == 2);
  mu_check(lastScrollDirection == SCROLL_CURSOR_RIGHT);
  mu_check(lastScrollCount == 1);
}

MU_TEST(test_zh_zl)
{
  vimInput("z");
  vimInput("h");

  mu_check(scrollRequestCount == 1);
  mu_check(lastScrollDirection == SCROLL_COLUMN_RIGHT);
  mu_check(lastScrollCount == 1);

  vimInput("5");
  vimInput("z");
  vimInput("h");

  mu_check(scrollRequestCount == 2);
  mu_check(lastScrollDirection == SCROLL_COLUMN_RIGHT);
  mu_check(lastScrollCount == 5);

  vimInput("2");
  vimInput("z");
  vimInput("H");
  mu_check(scrollRequestCount == 3);
  mu_check(lastScrollDirection == SCROLL_HALFPAGE_RIGHT);
  mu_check(lastScrollCount == 2);

  vimInput("3");
  vimInput("z");
  vimInput("L");
  mu_check(scrollRequestCount == 4);
  mu_check(lastScrollDirection == SCROLL_HALFPAGE_LEFT);
  mu_check(lastScrollCount == 3);

  vimInput("z");
  vimInput("l");
  mu_check(scrollRequestCount == 5);
  mu_check(lastScrollDirection == SCROLL_COLUMN_LEFT);
}

//MU_TEST(test_small_screen_scroll)
//{
//  vimWindowSetWidth(80);
//  vimWindowSetHeight(3);
//
//  mu_check(vimCursorGetLine() == 50);
//
//  vimInput("z");
//  vimInput("z");
//  mu_check(vimWindowGetTopLine() == 49);
//  mu_check(vimCursorGetLine() == 50);
//
//  vimInput("z");
//  vimInput("b");
//  mu_check(vimWindowGetTopLine() == 48);
//  mu_check(vimCursorGetLine() == 50);
//
//  vimInput("z");
//  vimInput("t");
//  mu_check(vimWindowGetTopLine() == 50);
//  mu_check(vimCursorGetLine() == 50);
//}

//MU_TEST(test_h_m_l)
//{
//  vimWindowSetWidth(80);
//  vimWindowSetHeight(40);
//
//  mu_check(vimCursorGetLine() == 50);
//
//  vimInput("z");
//  vimInput("z");
//
//  vimInput("H");
//  mu_check(vimCursorGetLine() == 31);
//
//  vimInput("L");
//  mu_check(vimCursorGetLine() == 70);
//
//  vimInput("M");
//  mu_check(vimCursorGetLine() == 50);
//}

//MU_TEST(test_no_scroll_after_setting_topline)
//{
//  vimWindowSetWidth(10);
//  vimWindowSetHeight(10);
//
//  pos_T pos;
//  pos.lnum = 95;
//  pos.col = 1;
//
//  vimCursorSetPosition(pos);
//
//  vimWindowSetTopLeft(90, 1);
//
//  mu_check(vimWindowGetTopLine() == 90);
//  vimInput("j");
//
//  mu_check(vimWindowGetTopLine() == 90);
//  mu_check(vimCursorGetLine() == 96);
//}
//
//MU_TEST(test_scroll_left_at_boundary)
//{
//  vimWindowSetWidth(4);
//  vimWindowSetHeight(10);
//
//  vimInput("l");
//  mu_check(vimWindowGetLeftColumn() == 0);
//
//  vimInput("l");
//  mu_check(vimWindowGetLeftColumn() == 0);
//
//  vimInput("l");
//  mu_check(vimWindowGetLeftColumn() == 0);
//
//  vimInput("l");
//  mu_check(vimWindowGetLeftColumn() == 1);
//
//  vimInput("l");
//  mu_check(vimWindowGetLeftColumn() == 2);
//}

//MU_TEST(test_no_scroll_after_setting_left)
//{
//  vimWindowSetWidth(4);
//  vimWindowSetHeight(10);
//
//  pos_T pos;
//  pos.lnum = 99;
//  pos.col = 2;
//  vimCursorSetPosition(pos);
//
//  vimWindowSetTopLeft(1, 2);
//
//  vimInput("l");
//  mu_check(vimWindowGetLeftColumn() == 2);
//
//  vimInput("l");
//  mu_check(vimWindowGetLeftColumn() == 2);
//
//  vimInput("l");
//  mu_check(vimWindowGetLeftColumn() == 2);
//
//  vimInput("l");
//  mu_check(vimWindowGetLeftColumn() == 3);
//}

MU_TEST(test_ctrl_d)
{
  vimKey("<c-d>");
  mu_check(scrollRequestCount == 1);
  mu_check(lastScrollDirection == SCROLL_HALFPAGE_DOWN);
  mu_check(lastScrollCount == 0);
}

MU_TEST(test_ctrl_u)
{
  vimKey("<c-u>");
  mu_check(scrollRequestCount == 1);
  mu_check(lastScrollDirection == SCROLL_HALFPAGE_UP);
  mu_check(lastScrollCount == 0);
}

MU_TEST(test_ctrl_e)
{
  vimInput("g");
  vimInput("g");

  vimKey("<c-e>");

  mu_check(scrollRequestCount == 1);
  mu_check(lastScrollDirection == SCROLL_LINE_UP);
  mu_check(lastScrollCount == 1);

  vimKey("5<c-e>");

  mu_check(scrollRequestCount == 2);
  mu_check(lastScrollDirection == SCROLL_LINE_UP);
  mu_check(lastScrollCount == 5);
}

MU_TEST(test_ctrl_y)
{
  vimWindowSetHeight(49);

  vimKey("<c-y>");

  mu_check(scrollRequestCount == 1);
  mu_check(lastScrollDirection == SCROLL_LINE_DOWN);
  mu_check(lastScrollCount == 1);

  vimKey("5<c-y>");

  mu_check(scrollRequestCount == 2);
  mu_check(lastScrollDirection == SCROLL_LINE_DOWN);
  mu_check(lastScrollCount == 5);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_set_get_metrics);
  MU_RUN_TEST(test_zz_zb_zt);
  MU_RUN_TEST(test_zs_ze);
  MU_RUN_TEST(test_ctrl_d);
  MU_RUN_TEST(test_ctrl_u);
  //MU_RUN_TEST(test_ctrl_f);
  MU_RUN_TEST(test_ctrl_e);
  MU_RUN_TEST(test_ctrl_y);
  MU_RUN_TEST(test_zh_zl);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetScrollCallback(&onScroll);

  vimBufferOpen("collateral/lines_100.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
