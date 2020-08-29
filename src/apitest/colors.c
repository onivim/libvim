#include "libvim.h"
#include "minunit.h"

static int colorSchemeChangedCount = 0;
static char_u *lastColorScheme = NULL;

int onColorSchemeChanged(char_u *colorScheme)
{
  colorSchemeChangedCount++;
  if (lastColorScheme != NULL)
  {
    vim_free(lastColorScheme);
  }
  lastColorScheme = NULL;
  if (colorScheme != NULL)
  {
    lastColorScheme = vim_strsave(colorScheme);
  }
  return OK;
};

char_u *acopy(char_u *str)
{
  char_u *sz = malloc(sizeof(char_u) * (strlen(str) + 1));
  strcpy(sz, str);
  sz[strlen(str)] = 0;
  return sz;
};

int onColorSchemeCompletion(char_u *pat, int *numSchemes, char_u ***schemes)
{
  *numSchemes = 3;

  *schemes = ALLOC_MULT(char_u *, 3);
  (*schemes)[0] = acopy("scheme1");
  (*schemes)[1] = acopy("scheme2");
  (*schemes)[2] = acopy("scheme3");

  return OK;
}

void test_setup(void)
{
  vimColorSchemeSetChangedCallback(&onColorSchemeChanged);
  vimColorSchemeSetCompletionCallback(&onColorSchemeCompletion);
  if (lastColorScheme != NULL)
  {
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

void test_teardown(void)
{
  if (lastColorScheme != NULL)
  {
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
  mu_check(colorSchemeChangedCount == 3);
  mu_check(lastColorScheme == NULL);
}

MU_TEST(test_colorscheme_changed_no_callback)
{
  vimColorSchemeSetChangedCallback(NULL);

  vimExecute("colorscheme test");

  mu_check(colorSchemeChangedCount == 0);

  vimExecute("colorscheme");
  mu_check(colorSchemeChangedCount == 0);
}

MU_TEST(test_colorscheme_get_completions)
{
  char_u *pat;
  expand_T xpc;
  ExpandInit(&xpc);
  xpc.xp_pattern = "";
  xpc.xp_pattern_len = (int)STRLEN(xpc.xp_pattern);
  xpc.xp_context = EXPAND_COLORS;

  pat = addstar(xpc.xp_pattern, xpc.xp_pattern_len, xpc.xp_context);

  mu_check(pat != NULL);

  int options = WILD_SILENT | WILD_USE_NL | WILD_ADD_SLASH | WILD_NO_BEEP;
  ExpandOne(&xpc, pat, NULL, options, WILD_ALL_KEEP);

  mu_check(xpc.xp_numfiles == 3);
  mu_check(strcmp(xpc.xp_files[0], "scheme1") == 0);
  mu_check(strcmp(xpc.xp_files[1], "scheme2") == 0);
  mu_check(strcmp(xpc.xp_files[2], "scheme3") == 0);

  vim_free(pat);
  ExpandCleanup(&xpc);
}

MU_TEST(test_colorscheme_get_completions_no_provider)
{
  vimColorSchemeSetCompletionCallback(NULL);

  char_u *pat;
  expand_T xpc;
  ExpandInit(&xpc);
  xpc.xp_pattern = "";
  xpc.xp_pattern_len = (int)STRLEN(xpc.xp_pattern);
  xpc.xp_context = EXPAND_COLORS;

  pat = addstar(xpc.xp_pattern, xpc.xp_pattern_len, xpc.xp_context);

  mu_check(pat != NULL);

  int options = WILD_SILENT | WILD_USE_NL | WILD_ADD_SLASH | WILD_NO_BEEP;
  ExpandOne(&xpc, pat, NULL, options, WILD_ALL_KEEP);

  mu_check(xpc.xp_numfiles == 0);

  vim_free(pat);
  ExpandCleanup(&xpc);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_colorscheme_changed);
  MU_RUN_TEST(test_colorscheme_changed_no_callback);
  MU_RUN_TEST(test_colorscheme_get_completions);
  MU_RUN_TEST(test_colorscheme_get_completions_no_provider);
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
