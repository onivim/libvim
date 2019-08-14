#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");

  vimExecute("e!");
  vimInput("g");
  vimInput("g");
  vimInput("0");

  vimSetClipboardGetCallback(NULL);
}

void test_teardown(void) {}

/* When clipboard is not enabled, the '*' register
 * should just behave like a normal register
 */
MU_TEST(test_clipboard_not_enabled_star)
{
  vimInput("\"");
  vimInput("*");

  vimInput("y");
  vimInput("y");

  int numLines;
  char_u **lines;
  vimRegisterGet(0, &numLines, &lines);

  mu_check(numLines == 1);
  printf("LINE: %s\n", lines[0]);
  mu_check(strcmp(lines[0], "This is the first line of a test file") == 0);
}

// Alloc + copy
char_u *acopy(char_u *str)
{
  char_u *sz = malloc(sizeof(char_u) * (strlen(str) + 1));
  strcpy(sz, str);
  sz[strlen(str)] = 0;
  return sz;
};

int simpleClipboardTest(int regname, int *numlines, char_u ***lines)
{
  printf("simpleClipboardTest called\n");
  *numlines = 1;
  *lines = ALLOC_ONE(char_u *);
  (*lines)[0] = acopy("Hello, World");
  return TRUE;
};

int multipleLineClipboardTest(int regname, int *numlines, char_u ***lines)
{
  printf("multipleLineClipboardTest called\n");
  *numlines = 3;
  *lines = ALLOC_MULT(char_u *, 3);
  (*lines)[0] = acopy("Hello2");
  (*lines)[1] = acopy("World");
  (*lines)[2] = acopy("Again");
  printf("multipleLineClipboardTest done\n");
  return TRUE;
};

int falseClipboardTest(int regname, int *numlines, char_u ***lines)
{
  return FALSE;
}

MU_TEST(test_paste_from_clipboard)
{
  vimSetClipboardGetCallback(&simpleClipboardTest);

  vimInput("\"");
  vimInput("*");

  vimInput("P");

  char_u *line = vimBufferGetLine(curbuf, 1);

  printf("LINE: |%s|\n", line);
  mu_check(strcmp(line, "Hello, World") == 0);
}

MU_TEST(test_paste_multiple_lines_from_clipboard)
{
  vimSetClipboardGetCallback(&multipleLineClipboardTest);

  vimInput("\"");
  vimInput("+");

  vimInput("P");

  char_u *line1 = vimBufferGetLine(curbuf, 1);
  printf("LINE1: |%s|\n", line1);
  char_u *line2 = vimBufferGetLine(curbuf, 2);
  printf("LINE2: |%s|\n", line2);
  char_u *line3 = vimBufferGetLine(curbuf, 3);
  printf("LINE3: |%s|\n", line3);

  mu_check(strcmp(line1, "Hello2") == 0);
  mu_check(strcmp(line2, "World") == 0);
  mu_check(strcmp(line3, "Again") == 0);
}

MU_TEST(test_paste_overrides_default_register)
{
  // If there is a callback set, and it returns lines,
  // it should overwrite the register.
  vimSetClipboardGetCallback(&multipleLineClipboardTest);

  vimInput("y");
  vimInput("y");

  // The 'P' should pull from the clipboard callback,
  // overriding what was yanked.
  vimInput("P");

  char_u *line1 = vimBufferGetLine(curbuf, 1);
  printf("LINE1: |%s|\n", line1);
  char_u *line2 = vimBufferGetLine(curbuf, 2);
  printf("LINE2: |%s|\n", line2);
  char_u *line3 = vimBufferGetLine(curbuf, 3);
  printf("LINE3: |%s|\n", line3);

  mu_check(strcmp(line1, "Hello2") == 0);
  mu_check(strcmp(line2, "World") == 0);
  mu_check(strcmp(line3, "Again") == 0);
}

/* When clipboard returns false, everything
 * should just behave like a normal register
 */
MU_TEST(test_clipboard_returns_false)
{
  vimSetClipboardGetCallback(&falseClipboardTest);

  vimInput("\"");
  vimInput("b");

  vimInput("y");
  vimInput("y");

  int numLines;
  char_u **lines;
  vimRegisterGet('b', &numLines, &lines);

  mu_check(numLines == 1);
  printf("LINE: %s\n", lines[0]);
  mu_check(strcmp(lines[0], "This is the first line of a test file") == 0);
}

MU_TEST(test_clipboard_returns_false_doesnt_override_default)
{
  vimSetClipboardGetCallback(&falseClipboardTest);

  vimInput("y");
  vimInput("y");

  vimInput("P");

  char_u *line1 = vimBufferGetLine(curbuf, 1);
  printf("LINE1: |%s|\n", line1);
  char_u *line2 = vimBufferGetLine(curbuf, 2);
  printf("LINE2: |%s|\n", line2);

  mu_check(strcmp(line1, "This is the first line of a test file") == 0);
  mu_check(strcmp(line2, "This is the first line of a test file") == 0);
}
MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_clipboard_not_enabled_star);
  MU_RUN_TEST(test_paste_from_clipboard);
  MU_RUN_TEST(test_paste_multiple_lines_from_clipboard);
  MU_RUN_TEST(test_clipboard_returns_false);
  MU_RUN_TEST(test_paste_overrides_default_register);
  MU_RUN_TEST(test_clipboard_returns_false_doesnt_override_default);
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
