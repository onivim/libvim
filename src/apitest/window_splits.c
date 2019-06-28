#include "vim.h"
#include "libvim.h"
#include "minunit.h"

#define MAX_FNAME 8192

char_u lastFilename[MAX_FNAME];
windowSplit_T lastSplitType;

void onWindowSplit(windowSplit_T splitType, char_u* filename) {
  printf("onWindowSplit - type: |%d| file: |%s|\n", splitType, filename);

  assert(strlen(filename) < MAX_FNAME);

  strcpy(lastFilename, filename);
  lastSplitType = splitType;
};

void test_setup(void) {
  vimInput("<esc>");
  vimInput("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
}

void test_teardown(void) {}

MU_TEST(test_vsplit) {
  vimExecute("vsp test-file.txt");

  mu_check(strcmp(lastFilename, "test-file.txt") == 0);
  mu_check(lastSplitType == VERTICAL_SPLIT);
}

MU_TEST(test_hsplit) {
  vimExecute("sp test-h-file.txt");

  mu_check(strcmp(lastFilename, "test-h-file.txt") == 0);
  mu_check(lastSplitType == HORIZONTAL_SPLIT);
}

MU_TEST(test_tabnew) {
  vimExecute("tabnew test-tabnew-file.txt");

  mu_check(strcmp(lastFilename, "test-tabnew-file.txt") == 0);
  mu_check(lastSplitType == TAB_PAGE);
}

MU_TEST_SUITE(test_suite) {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_vsplit);
  MU_RUN_TEST(test_hsplit);
  MU_RUN_TEST(test_tabnew);
}

int main(int argc, char **argv) {
  vimInit(argc, argv);

  vimSetWindowSplitCallback(&onWindowSplit);

  win_setwidth(5);
  win_setheight(100);

  buf_T *buf = vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
