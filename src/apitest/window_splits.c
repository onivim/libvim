#include "libvim.h"
#include "minunit.h"
#include "vim.h"

#define MAX_FNAME 8192

char_u lastFilename[MAX_FNAME];
windowSplit_T lastSplitType;

void onWindowSplit(windowSplit_T splitType, char_u *filename)
{
  printf("onWindowSplit - type: |%d| file: |%s|\n", splitType, filename);

  assert(strlen(filename) < MAX_FNAME);

  strcpy(lastFilename, filename);
  lastSplitType = splitType;
};

windowMovement_T lastMovement;
int lastMovementCount;

void onWindowMovement(windowMovement_T movementType, int count)
{
  printf("onWindowMovement - type: |%d| count: |%d|\n", movementType, count);

  lastMovement = movementType;
  lastMovementCount = count;
};

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
}

void test_teardown(void) {}

MU_TEST(test_vsplit)
{
  vimExecute("vsp test-file.txt");

  mu_check(strcmp(lastFilename, "test-file.txt") == 0);
  mu_check(lastSplitType == VERTICAL_SPLIT);
}

MU_TEST(test_hsplit)
{
  vimExecute("sp test-h-file.txt");

  mu_check(strcmp(lastFilename, "test-h-file.txt") == 0);
  mu_check(lastSplitType == HORIZONTAL_SPLIT);
}

MU_TEST(test_vsplit_ctrl_w)
{
  vimBufferOpen("collateral/testfile.txt", 1, 0);

  vimInput("<c-w>");
  vimInput("v");

  mu_check(lastSplitType == VERTICAL_SPLIT);
  mu_check(strstr(lastFilename, "testfile.txt") != NULL);
}

MU_TEST(test_hsplit_ctrl_w)
{
  vimBufferOpen("collateral/testfile.txt", 1, 0);

  vimInput("<c-w>");
  vimInput("s");

  mu_check(lastSplitType == HORIZONTAL_SPLIT);
  mu_check(strstr(lastFilename, "testfile.txt") != NULL);
}

MU_TEST(test_tabnew)
{
  vimExecute("tabnew test-tabnew-file.txt");

  mu_check(strcmp(lastFilename, "test-tabnew-file.txt") == 0);
  mu_check(lastSplitType == TAB_PAGE);
}

MU_TEST(test_win_movements)
{

  printf("Entering <c-w>\n");
  vimInput("<c-w>");
  printf("Entering <c-j>\n");
  vimInput("<c-j>");

  mu_check(lastMovement == WIN_CURSOR_DOWN);
  mu_check(lastMovementCount == 1);

  vimInput("<c-w>");
  vimInput("k");

  mu_check(lastMovement == WIN_CURSOR_UP);
  mu_check(lastMovementCount == 1);

  vimInput("<c-w>");
  vimInput("h");

  mu_check(lastMovement == WIN_CURSOR_LEFT);
  mu_check(lastMovementCount == 1);

  vimInput("<c-w>");
  vimInput("l");

  mu_check(lastMovement == WIN_CURSOR_RIGHT);
  mu_check(lastMovementCount == 1);

  vimInput("<c-w>");
  vimInput("t");

  mu_check(lastMovement == WIN_CURSOR_TOP_LEFT);
  mu_check(lastMovementCount == 1);

  vimInput("<c-w>");
  vimInput("b");

  mu_check(lastMovement == WIN_CURSOR_BOTTOM_RIGHT);
  mu_check(lastMovementCount == 1);

  vimInput("<c-w>");
  vimInput("p");

  mu_check(lastMovement == WIN_CURSOR_PREVIOUS);
  mu_check(lastMovementCount == 1);
}

MU_TEST(test_win_move_count_before)
{
  vimInput("2");
  vimInput("<c-w>");
  vimInput("k");

  mu_check(lastMovement == WIN_CURSOR_UP);
  mu_check(lastMovementCount == 2);
}

MU_TEST(test_win_move_count_after)
{
  vimInput("<c-w>");
  vimInput("4");
  vimInput("k");

  mu_check(lastMovement == WIN_CURSOR_UP);
  mu_check(lastMovementCount == 4);
}

MU_TEST(test_win_move_count_before_and_after)
{
  vimInput("3");
  vimInput("<c-w>");
  vimInput("5");
  vimInput("k");

  mu_check(lastMovement == WIN_CURSOR_UP);
  mu_check(lastMovementCount == 35);
}

MU_TEST(test_move_commands)
{
  vimInput("<c-w>");
  vimInput("H");
  mu_check(lastMovement == WIN_MOVE_FULL_LEFT);
  mu_check(lastMovementCount == 1);

  vimInput("<c-w>");
  vimInput("L");

  mu_check(lastMovement == WIN_MOVE_FULL_RIGHT);
  mu_check(lastMovementCount == 1);

  vimInput("<c-w>");
  vimInput("K");

  mu_check(lastMovement == WIN_MOVE_FULL_UP);
  mu_check(lastMovementCount == 1);

  vimInput("<c-w>");
  vimInput("J");

  mu_check(lastMovement == WIN_MOVE_FULL_DOWN);
  mu_check(lastMovementCount == 1);

  vimInput("<c-w>");
  vimInput("r");

  mu_check(lastMovement == WIN_MOVE_ROTATE_DOWNWARDS);
  mu_check(lastMovementCount == 1);

  vimInput("<c-w>");
  vimInput("R");

  mu_check(lastMovement == WIN_MOVE_ROTATE_UPWARDS);
  mu_check(lastMovementCount == 1);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_vsplit);
  MU_RUN_TEST(test_hsplit);
  MU_RUN_TEST(test_vsplit_ctrl_w);
  MU_RUN_TEST(test_hsplit_ctrl_w);
  MU_RUN_TEST(test_tabnew);
  MU_RUN_TEST(test_win_movements);
  MU_RUN_TEST(test_win_move_count_before);
  MU_RUN_TEST(test_win_move_count_after);
  MU_RUN_TEST(test_win_move_count_before_and_after);
  MU_RUN_TEST(test_move_commands);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetWindowSplitCallback(&onWindowSplit);
  vimSetWindowMovementCallback(&onWindowMovement);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
