#include "libvim.h"
#include "minunit.h"

static int updateCount = 0;
static int lastLnum = 0;
static int lastLnume = 0;
static long lastXtra = 0;

static int macroStartCallbackCount = 0;
static int macroStopCallbackCount = 0;

static int lastStartRegname = -1;
static int lastStopRegname = -1;
static char_u *lastRegvalue = NULL;

void onMacroStartRecord(int regname)
{
  macroStartCallbackCount++;
  lastStartRegname = regname;
}

void onMacroStopRecord(int regname, char_u *regvalue)
{
  macroStopCallbackCount++;
  lastStopRegname = regname;
  if (lastRegvalue != NULL)
  {
    vim_free(lastRegvalue);
  }

  lastRegvalue = vim_strsave(regvalue);
}

void onBufferUpdate(bufferUpdate_T update)
{
  lastLnum = update.lnum;
  lastLnume = update.lnume;
  lastXtra = update.xtra;
  updateCount++;
}

void test_setup(void)
{
  vimKey("<esc>");
  vimKey("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");

  updateCount = 0;
  lastLnum = 0;
  lastLnume = 0;
  lastXtra = 0;

  macroStartCallbackCount = 0;
  macroStopCallbackCount = 0;
  lastStartRegname = -1;
  lastStopRegname = -1;

  if (lastRegvalue != NULL)
  {
    vim_free(lastRegvalue);
  }
  lastRegvalue = NULL;
}

void test_teardown(void)
{
  if (lastRegvalue != NULL)
  {
    vim_free(lastRegvalue);
  }
}

MU_TEST(test_macro_saves_register)
{
  /* Record a macro into the 'a' register */
  vimInput("q");
  vimInput("a");

  mu_check(macroStartCallbackCount == 1);
  mu_check(lastStartRegname == 'a');

  vimInput("j");
  vimInput("j");
  vimInput("j");
  vimInput("k");
  vimInput("k");

  /* Stop recording */

  vimInput("q");
  mu_check(macroStopCallbackCount == 1);
  mu_check(lastStopRegname == 'a');
  mu_check(strcmp(lastRegvalue, "jjjkk") == 0);

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
  vimMacroSetStartRecordCallback(&onMacroStartRecord);
  vimMacroSetStopRecordCallback(&onMacroStopRecord);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen((char_u *)"collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
