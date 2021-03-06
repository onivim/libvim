#include "libvim.h"
#include "minunit.h"

static int updateCount = 0;
static int lastLnum = 0;
static int lastLnume = 0;
static long lastXtra = 0;
static long lastVersionAtUpdateTime = 0;

void onBufferUpdate(bufferUpdate_T update)
{
  lastLnum = update.lnum;
  lastLnume = update.lnume;
  lastXtra = update.xtra;
  lastVersionAtUpdateTime = vimBufferGetLastChangedTick(curbuf);

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
}

void test_teardown(void) {}

MU_TEST(test_single_line_update)
{
  vimInput("x");

  mu_check(updateCount == 1);
  mu_check(lastLnum == 1);
  mu_check(lastLnume == 2);
  mu_check(lastXtra == 0);
  mu_check(lastVersionAtUpdateTime == vimBufferGetLastChangedTick(curbuf));
}

MU_TEST(test_add_line)
{
  vimInput("y");
  vimInput("y");
  vimInput("p");

  mu_check(updateCount == 1);
  mu_check(lastLnum == 2);
  mu_check(lastLnume == 2);
  mu_check(lastXtra == 1);
  mu_check(lastVersionAtUpdateTime == vimBufferGetLastChangedTick(curbuf));
}

MU_TEST(test_add_multiple_lines)
{
  vimInput("y");
  vimInput("y");
  vimInput("2");
  vimInput("p");

  mu_check(updateCount == 1);
  mu_check(lastLnum == 2);
  mu_check(lastLnume == 2);
  mu_check(lastXtra == 2);
  mu_check(lastVersionAtUpdateTime == vimBufferGetLastChangedTick(curbuf));
}

MU_TEST(test_delete_line)
{
  vimInput("d");
  vimInput("d");

  mu_check(updateCount == 1);
  mu_check(lastLnum == 1);
  mu_check(lastLnume == 2);
  mu_check(lastXtra == -1);
  mu_check(lastVersionAtUpdateTime == vimBufferGetLastChangedTick(curbuf));
}

MU_TEST(test_delete_multiple_lines)
{
  vimInput("d");
  vimInput("2");
  vimInput("j");

  mu_check(updateCount == 1);
  mu_check(lastLnum == 1);
  mu_check(lastLnume == 4);
  mu_check(lastXtra == -3);
  mu_check(lastVersionAtUpdateTime == vimBufferGetLastChangedTick(curbuf));
}

MU_TEST(test_delete_n_lines)
{
  vimBufferOpen("collateral/lines_100.txt", 1, 0);
  vimInput("5");
  vimInput("d");
  vimInput("d");

  mu_check(updateCount == 1);
  mu_check((lastLnume - lastLnum) == 5);
  mu_check(lastXtra == -5);
  mu_check(lastVersionAtUpdateTime == vimBufferGetLastChangedTick(curbuf));
}

MU_TEST(test_delete_large_n_lines)
{
  vimBufferOpen("collateral/lines_100.txt", 1, 0);
  vimInput("5");
  vimInput("5");
  vimInput("d");
  vimInput("d");

  mu_check(updateCount == 1);
  mu_check((lastLnume - lastLnum) == 55);
  mu_check(lastXtra == -55);
  mu_check(lastVersionAtUpdateTime == vimBufferGetLastChangedTick(curbuf));
}

MU_TEST(test_delete_mn_lines)
{
  vimBufferOpen("collateral/lines_100.txt", 1, 0);
  vimInput("5");
  vimInput("d");
  vimInput("5");
  vimInput("d");

  mu_check(updateCount == 1);
  mu_check((lastLnume - lastLnum) == 25);
  mu_check(lastXtra == -25);
  mu_check(lastVersionAtUpdateTime == vimBufferGetLastChangedTick(curbuf));
}

MU_TEST(test_set_lines)
{
  vimBufferOpen("collateral/lines_100.txt", 1, 0);
  char_u *lines[] = {"one"};
  vimBufferSetLines(curbuf, 0, -1, lines, 1);

  mu_check(updateCount == 1);
  mu_check(lastLnum == 1);
  mu_check(lastLnume == 101);
  mu_check(lastXtra == -99);
  mu_check(lastVersionAtUpdateTime == vimBufferGetLastChangedTick(curbuf));

  mu_check(vimBufferGetLineCount(curbuf) == 1);
}

MU_TEST(test_insert)
{
  vimInput("i");
  vimInput("a");
  vimInput("b");

  mu_check(updateCount == 2);
  mu_check(lastLnum == 1);
  mu_check(lastLnume == 2);
  mu_check(lastXtra == 0);
  mu_check(lastVersionAtUpdateTime == vimBufferGetLastChangedTick(curbuf));
}

MU_TEST(test_modified)
{
  vimInput("i");
  vimInput("a");

  mu_check(vimBufferGetModified(curbuf) == TRUE);
}

MU_TEST(test_reset_modified_after_reload)
{
  vimInput("i");
  vimInput("a");

  vimExecute("e!");

  mu_check(vimBufferGetModified(curbuf) == FALSE);
}

MU_TEST(test_reset_modified_after_undo)
{
  vimExecute("e!");
  mu_check(vimBufferGetModified(curbuf) == FALSE);

  vimInput("O");
  vimInput("a");
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "a") == 0);

  vimKey("<esc>");
  vimInput("u");
  mu_check(vimBufferGetModified(curbuf) == FALSE);

  vimKey("<c-r>");
  mu_check(strcmp(vimBufferGetLine(curbuf, 1), "a") == 0);
  mu_check(vimBufferGetModified(curbuf) == TRUE);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_single_line_update);
  MU_RUN_TEST(test_add_line);
  MU_RUN_TEST(test_add_multiple_lines);
  MU_RUN_TEST(test_delete_line);
  MU_RUN_TEST(test_delete_multiple_lines);
  MU_RUN_TEST(test_insert);
  MU_RUN_TEST(test_modified);
  MU_RUN_TEST(test_reset_modified_after_reload);
  MU_RUN_TEST(test_reset_modified_after_undo);
  MU_RUN_TEST(test_delete_n_lines);
  MU_RUN_TEST(test_delete_large_n_lines);
  MU_RUN_TEST(test_delete_mn_lines);
  MU_RUN_TEST(test_set_lines);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetBufferUpdateCallback(&onBufferUpdate);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
