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

MU_TEST(test_search_in_large_file)
{

  vimInput("/");
  printf("Typing e...\n");
  vimInput("e");
  printf("Typed e! \n");

  int num;
  searchHighlight_T *highlights;
  vimSearchGetHighlights(0, 0, &num, &highlights);
  printf("Got %d highlights\n", num);
  vim_free(highlights);
  mu_check(num == 15420);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_search_in_large_file);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/large-c-file.c", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
