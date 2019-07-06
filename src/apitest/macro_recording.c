#include "libvim.h"
#include "minunit.h"

static int updateCount = 0;
static int lastLnum = 0;
static int lastLnume = 0;
static long lastXtra = 0;

void onBufferUpdate(bufferUpdate_T update)
{
  lastLnum = update.lnum;
  lastLnume = update.lnume;
  lastXtra = update.xtra;
  updateCount++;
}

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");

  updateCount = 0;
  lastLnum = 0;
  lastLnume = 0;
  lastXtra = 0;
}

void test_teardown(void) {}

MU_TEST(test_macro_saves_register)
{
  /* Record a macro into the 'a' register */
  vimInput("q");
  vimInput("a");

  vimInput("j");
  vimInput("j");
  vimInput("j");
  vimInput("k");
  vimInput("k");

  /* Stop recording */
  vimInput("q");

  /* Validate register */

  int num_lines;
  char_u **lines;

  vimRegisterGet('a', &num_lines, &lines);

  mu_check(num_lines == 1);
  mu_check(strcmp(lines[0], ("jjjkk")) == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_macro_saves_register);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetBufferUpdateCallback(&onBufferUpdate);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen((char_u *)"collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
