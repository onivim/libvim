#include "libvim.h"
#include "minunit.h"

static int stopSearchHighlightCount = 0;

void onStopSearchHighlight(void)
{
  stopSearchHighlightCount++;
}

static int errorCount = 0;

void onMessage(char_u *title, char_u *msg, msgPriority_T priority)
{
  printf("onMessage - title: |%s| contents: |%s|", title, msg);

  if (priority == MSG_ERROR)
  {
    errorCount++;
  }
};

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");

  vimExecute("e!");
  vimInput("g");
  vimInput("g");
  vimInput("0");
  errorCount = 0;
  stopSearchHighlightCount = 0;
}

void test_teardown(void) {}

MU_TEST(test_no_highlights_initially)
{
  int num;
  searchHighlight_T *highlights;
  vimSearchGetHighlights(0, 0, &num, &highlights);

  mu_check(num == 0);
  mu_check(highlights == NULL);
}

MU_TEST(test_get_highlights)
{

  vimInput("/");
  vimInput("o");
  vimInput("f");

  int num;
  searchHighlight_T *highlights;
  vimSearchGetHighlights(0, 0, &num, &highlights);

  mu_check(num == 3);
  mu_check(highlights[0].start.lnum == 1);
  mu_check(highlights[0].start.col == 23);
  mu_check(highlights[0].end.lnum == 1);
  mu_check(highlights[0].end.col == 25);

  mu_check(highlights[1].start.col == 24);

  mu_check(highlights[2].start.lnum == 3);
  mu_check(highlights[2].start.col == 23);
  mu_check(highlights[2].end.lnum == 3);
  mu_check(highlights[2].end.col == 25);

  vim_free(highlights);
  mu_check(num == 3);
}

MU_TEST(test_nohlsearch)
{
  mu_check(stopSearchHighlightCount == 0);
  vimExecute("nohlsearch");
  mu_check(stopSearchHighlightCount == 1);
}

MU_TEST(test_no_matching_highlights)
{
  vimInput("/");
  vimInput("a");
  vimInput("b");
  vimInput("c");

  int num;
  searchHighlight_T *highlights;
  vimSearchGetHighlights(0, 0, &num, &highlights);

  mu_check(num == 0);
  mu_check(errorCount == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_no_highlights_initially);
  MU_RUN_TEST(test_get_highlights);
  MU_RUN_TEST(test_nohlsearch);
  MU_RUN_TEST(test_no_matching_highlights);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  vimSetStopSearchHighlightCallback(&onStopSearchHighlight);
  vimSetMessageCallback(&onMessage);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
