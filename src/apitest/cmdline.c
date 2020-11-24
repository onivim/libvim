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

MU_TEST(test_insert_literal_ctrl_v)
{
  vimInput(":");
  vimInput("a");
  vimKey("<c-v>");
  vimInput("1");
  vimInput("2");
  vimInput("6");
  vimInput("b");
  mu_check(strcmp(vimCommandLineGetText(), "a~b") == 0);
}

MU_TEST(test_insert_literal_ctrl_q)
{
  vimInput(":");
  vimInput("a");
  vimKey("<c-q>");
  vimInput("1");
  vimInput("2");
  vimInput("6");
  vimInput("b");
  mu_check(strcmp(vimCommandLineGetText(), "a~b") == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

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
