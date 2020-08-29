#include "libvim.h"
#include "minunit.h"

static int colorSchemeChangedCount = 0;
static char_u *lastColorScheme = NULL;

int onColorSchemeChanged(char_u *colorScheme)
{
  colorSchemeChangedCount++;
  if (lastColorScheme != NULL) {
    vim_free(lastColorScheme);
  }
  lastColorScheme = vim_strsave(colorScheme);
  return OK;
};

void test_setup(void)
{
  vimColorSchemeSetChangedCallback(&onColorSchemeChanged);
  if (lastColorScheme != NULL) {
    vim_free(lastColorScheme);
    lastColorScheme = NULL;
  }
  lastColorScheme = NULL;
  colorSchemeChangedCount = 0;

  // Reset formatexpr, formatprg, and equalprg to defaults
  vimKey("<esc>");
  vimKey("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {
  if (lastColorScheme != NULL) {
    vim_free(lastColorScheme);
    lastColorScheme = NULL;
  }
}

MU_TEST(test_colorscheme_changed)
{
  vimExecute("colorscheme test");

  mu_check(colorSchemeChangedCount == 1);
  mu_check(strcmp(lastColorScheme, "test") == 0);

  vimExecute("colorscheme Multi Word Scheme");
  mu_check(colorSchemeChangedCount == 2);
  mu_check(strcmp(lastColorScheme, "Multi Word Scheme") == 0);
  
  vimExecute("colorscheme");
  mu_check(colorSchemeChangedCount == 2);
}

MU_TEST(test_colorscheme_changed_no_callback)
{
  vimColorSchemeSetChangedCallback(NULL);
  
  vimExecute("colorscheme test");

  mu_check(colorSchemeChangedCount == 0);

  vimExecute("colorscheme");
  mu_check(colorSchemeChangedCount == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_colorscheme_changed);
  MU_RUN_TEST(test_colorscheme_changed_no_callback);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
