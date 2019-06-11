#include "libvim.h"
#include "minunit.h"

static int cmdLineEnterCount = 0;
static int cmdLineLeaveCount = 0;
static int cmdLineChangedCount = 0;

void test_setup(void) {
  vimInput("<esc>");
  vimInput("<esc>");

  vimExecute("e!");
  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(test_no_highlights_initially) {
  int num;
  searchHighlight_T *highlights;
  vimSearchGetHighlights(0, 0, &num, &highlights);

  mu_check(num == 0);
  mu_check(highlights == NULL);
}

MU_TEST(test_get_highlights) {

  vimInput("/");
  vimInput("e");

  int num;
  searchHighlight_T *highlights;
  vimSearchGetHighlights(0, 0, &num, &highlights);

  int v = 1;
  int count = 0;
      pos_T lastPos;
      pos_T startPos;
      startPos.lnum = 2;
      startPos.col = 0;
      lastPos = startPos;

      pos_T endPos;

  char_u* pattern = vimSearchGetPattern();
  printf("Pattern: %s\n", pattern);

  while (v == 1) {
      v = searchit(
              NULL,
              curbuf,
              &startPos,
              &endPos,
              FORWARD,
              pattern,
              1,
              SEARCH_KEEP,
              RE_SEARCH,
              2,
              NULL,
              NULL);

      if (v == 0) {
          break;
      }

      if (startPos.lnum < lastPos.lnum || (startPos.lnum == lastPos.lnum && startPos.col <= lastPos.col)) {
          break;
      }

      printf("Match number: %d Return value: %d startPos: %d, %d endPos: %d, %d lastPos: %d, %d\n", count, v, startPos.lnum, startPos.col, endPos.lnum, endPos.col, lastPos.lnum, lastPos.col);




      lastPos = startPos;

      startPos = endPos;
      startPos.col = startPos.col + 1;
      count++;
  }

  mu_check(num == 3);
}

MU_TEST_SUITE(test_suite) {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_no_highlights_initially);
  MU_RUN_TEST(test_get_highlights);
}

int main(int argc, char **argv) {
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  buf_T *buf = vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
