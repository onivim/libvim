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

MU_TEST(test_digraph_doesnt_hang)
{
  vimInput("i");

  // Start inserting digraph... should be no-op right now
  // until we bring the feature back
  vimInput("<c-k>");
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_digraph_doesnt_hang);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimBufferOpen("collateral/lines_100.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
