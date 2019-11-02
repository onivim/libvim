#include "libvim.h"
#include "minunit.h"

static int displayVersionCount = 0;
static int displayIntroCount = 0;

void onIntro(void)
{
  displayIntroCount++;
}

void onVersion(void)
{
  displayVersionCount++;
}

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
  vimInput("0");

  displayVersionCount = 0;
  displayIntroCount = 0;
}

void test_teardown(void) {}

MU_TEST(test_intro_command)
{
  mu_check(displayIntroCount == 0);
  vimExecute("intro");
  mu_check(displayIntroCount == 1);
}

MU_TEST(test_version_command)
{
  mu_check(displayVersionCount == 0);
  vimExecute("version");
  mu_check(displayVersionCount == 1);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_intro_command);
  MU_RUN_TEST(test_version_command);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetDisplayIntroCallback(&onIntro);
  vimSetDisplayVersionCallback(&onVersion);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
