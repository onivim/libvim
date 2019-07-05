#include "libvim.h"
#include "minunit.h"
#include "vim.h"

#define MAX_SIZE 8192

char_u lastDirectory[MAX_SIZE];
int onDirectoryChangedCount = 0;

void onDirectoryChanged(char_u *path)
{
  printf("onDirectoryChanged - path: |%s|", path);

  assert(strlen(path) < MAX_SIZE);
  onDirectoryChangedCount++;

  strcpy(lastDirectory, path);
};

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");

  onDirectoryChangedCount = 0;
}

void test_teardown(void) {}

MU_TEST(test_chdir)
{
  vimExecute("cd collateral");

  mu_check(onDirectoryChangedCount == 1);

  char cwd[MAX_SIZE];
  getcwd(cwd, sizeof(cwd));

  mu_check(strstr(cwd, "collateral") != cwd);
};

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_chdir);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetDirectoryChangedCallback(&onDirectoryChanged);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
