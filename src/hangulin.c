/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

#include "vim.h"

#ifndef HANGUL_DEFAULT_KEYBOARD
#define HANGUL_DEFAULT_KEYBOARD 3
#endif

#define AUTOMATA_NEW 0
#define AUTOMATA_CORRECT 1
#define AUTOMATA_SPECIAL 2
#define AUTOMATA_CORRECT_NEW 3
#define AUTOMATA_ERROR 4
#define AUTOMATA_NULL 5

#define F_F 0x1 /* 초성 (initial sound) */
#define F_M 0x2 /* 중성 (medial vowel) */
#define F_L 0x4 /* 종성 (final consonant) */
#define F_A 0x8 /* ASCII */
#define F_NULL 1
#define M_NULL 2
#define L_NULL 1

static int hangul_input_state = 0;
static int f = F_NULL, m = M_NULL, l = L_NULL;
static int sp = 0;
static char_u stack[20] = {0};
static int last_l = -1, last_ll = -1;
static int hangul_keyboard_type = HANGUL_DEFAULT_KEYBOARD;

static void convert_ks_to_3(const char_u *src, int *fp, int *mp, int *lp);
static int convert_3_to_ks(int fv, int mv, int lv, char_u *des);

#define push(x)               \
  {                           \
    stack[sp++] = *(x);       \
    stack[sp++] = *((x) + 1); \
  }
#define pop(x)                \
  {                           \
    *((x) + 1) = stack[--sp]; \
    *(x) = stack[--sp];       \
  }
#define query(x)                \
  {                             \
    *((x) + 1) = stack[sp - 1]; \
    *(x) = stack[sp - 2];       \
  }

#define convert_3_to_code convert_3_to_ks

/**********************************************************************/
/****** 3 벌식자판을 위한 루틴  (Routines for 3 bulsik keyboard) ******/
/**********************************************************************/

/* 3 벌식에서 자판 변환 (3 bulsik keyboard conversion) */

static char_u value_table_for_3[] =
    {
        24, '"', '#', '$', '%', '&',  /* ! " # $ % & */
        18, '(', ')', '*', '+', ',',  /* ' ( ) * + , */
        '-', '.', 13, 17, 29, 22,     /* - . / 0 1 2 */
        19, 19, 26, 5, 12, 28,        /* 3 4 5 6 7 8 */
        20, ':', 9, '2', '=', '3',    /* 9 : ; < = > */
        '?', '@', 8, '!', 11, 10,     /* ? @ A B C D */
        26, 3, '/', 39, '8', '4',     /* E F G H I J */
        '5', '6', '1', '0', '9', '>', /* K L M N O P */
        28, 6, 7, ';', '7', 16,       /* Q R S T U V */
        27, 20, '<', 25, '[', 92,     /* W X Y Z [ \ */
        ']', '^', '_', '`', 23, 20,   /* ] ^ _ ` a b */
        10, 29, 11, 3, 27, 4,         /* c d e f g h */
        8, 13, 2, 14, 20, 11,         /* i j k l m n */
        16, 19, 21, 4, 5, 7,          /* o p q r s t */
        5, 13, 9, 2, 7, 17,           /* u v w x y z */
};

static short_u kind_table_for_3[] =
    {
        F_L, F_A, F_A, F_A, F_A, F_A, /* ! " # $ % & */
        F_F, F_A, F_A, F_A, F_A, F_A, /* ' ( ) * + , */
        F_A, F_A, F_M, F_F, F_L, F_L, /* - . / 0 1 2 */
        F_L, F_M, F_M, F_M, F_M, F_M, /* 3 4 5 6 7 8 */
        F_M, F_A, F_F, F_A, F_A, F_A, /* 9 : ; < = > */
        F_A, F_A, F_L, F_A, F_L, F_L, /* ? @ A B C D */
        F_L, F_L, F_A, F_A, F_A, F_A, /* E F G H I J */
        F_A, F_A, F_A, F_A, F_A, F_A, /* K L M N O P */
        F_L, F_M, F_L, F_A, F_A, F_L, /* Q R S T U V */
        F_L, F_L, F_A, F_L, F_A, F_A, /* W X Y Z [ \ */
        F_A, F_A, F_A, F_A, F_L, F_M, /* ] ^ _ ` a b */
        F_M, F_M, F_M, F_M, F_M, F_F, /* c d e f g h */
        F_F, F_F, F_F, F_F, F_F, F_F, /* i j k l m n */
        F_F, F_F, F_L, F_M, F_L, F_M, /* o p q r s t */
        F_F, F_M, F_L, F_L, F_F, F_L, /* u v w x y z */
};

/* 3 벌식에서 (현재초성, 입력영문) -> 복합초성 처리
 * 3 bulsik: (current initial sound, input english) -> compound initial sound.
 */

static int
comfcon3(int v, int c) {
  if (v == 2 && c == 2)
    return 3;
  if (v == 5 && c == 5)
    return 6;
  if (v == 9 && c == 9)
    return 10;
  if (v == 11 && c == 11)
    return 12;
  if (v == 14 && c == 14)
    return 15;
  return 0;
}

/* 3 벌식에서 (현재모음, 입력 영문) -> 복합 모음 처리
 * 3 bulsik: (current vowel, input english) -> compound vowel.
 */

static int
comvow3(int v, int c) {
  switch (v) {
  case 13: /* ㅗ */
    switch (c) {
    case 3: /* ㅗㅏ */
      return 14;
    case 4: /* ㅗㅐ */
      return 15;
    case 29: /* ㅗㅣ */
      return 18;
    }
    break;

  case 20: /* ㅜ */
    switch (c) {
    case 7: /* ㅜㅓ */
      return 21;
    case 10: /* ㅜㅔ */
      return 22;
    case 29: /* ㅜㅣ */
      return 23;
    }
    break;

    /* 3 벌식 자판은 ㅡㅣ 가 있으므로 ... */
  }
  return 0;
}

/* 3 벌식에서 (현재 받침, 영문자 입력) -> 받침
 * 3 bulsik: (current prop(?), input english) -> prop(?).
 * I want to say, the 'prop' is similar to 'final consonant', but not vowel.
 * (I cannot find the real english from my dictionary. Sorry!)
 * VIM: V = initial sound, I = medial vowel, M = final consonant.
 */

static int
comcon3(int k, int c) {
  switch (k) {
  case 2: /* ㄱ */
    switch (c) {
    case 2:
      return 3; /* ㄱㄱ */
    case 21:
      return 4; /* ㄱㅅ */
    }
    break;

  case 5: /* ㄴ */
    switch (c) {
    case 24: /* ㄴㅈ */
      return 6;
    case 29:
      return 7; /* ㄴㅎ */
    }
    break;

  case 9: /* ㄹ */
    switch (c) {
    case 2: /* ㄹㄱ */
      return 10;
    case 17: /* ㄹㅁ */
      return 11;
    case 19: /* ㄹㅂ */
      return 12;
    case 21: /* ㄹㅅ */
      return 13;
    case 27: /* ㄹㅌ */
      return 14;
    case 28: /* ㄹㅍ */
      return 15;
    case 29: /* ㄹㅎ */
      return 16;
    }
    break;

  case 19:
    switch (c) {
    case 21: /* ㅂㅅ */
      return 20;
    }
    break;
  }
  return 0;
}

/**********************************************************************/
/****** 2 벌식자판을 위한 루틴  (Routines for 2 bulsik keyboard) ******/
/**********************************************************************/

static int
kind_table_for_2(int c) {
  static char_u table[] =
      {
          /* a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s */
          0, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
          /* t, u, v, w, x, y, z */
          0, 1, 0, 0, 0, 1, 0};

  if (c <= 'Z')
    c -= 'A';
  else
    c -= 'a';

  return table[c];
}

/* 2 벌식에서 영문자 -> 조합형 초성 변환
 * (2 bulsik: conversion english char. to initial sound of compound type)
 * 결과: 초성이 아니면 0 (If it is not initial sound, return 0).
 */
static int
fcon(int c) {
  static char_u table[] =
      {
          /*E */ 6, /*F */ 0, /*G */ 0, /*H */ 0, /*I */ 0, /*J */ 0, /*K */ 0,
          /*L */ 0, /*M */ 0, /*N */ 0, /*O */ 0, /*P */ 0, /*Q */ 10, /*R */ 3,
          /*S */ 0, /*T */ 12, /*U */ 0, /*V */ 0, /*W */ 15, /*X */ 0, /*Y */ 0,
          /*Z */ 0, /*[ */ 0, /*\ */ 0, /*] */ 0, /*^ */ 0, /*_ */ 0, /*` */ 0,
          /*a */ 8, /*b */ 0, /*c */ 16, /*d */ 13, /*e */ 5, /*f */ 7, /*g */ 20,
          /*h */ 0, /*i */ 0, /*j */ 0, /*k */ 0, /*l */ 0, /*m */ 0, /*n */ 0,
          /*o */ 0, /*p */ 0, /*q */ 9, /*r */ 2, /*s */ 4, /*t */ 11, /*u */ 0,
          /*v */ 19, /*w */ 14, /*x */ 18, /*y */ 0, /*z */ 17};

  if (c < 'E' || c > 'z')
    return 0;
  return table[c - 'E'];
}

/* 2 벌식에서 영문자 -> 중성 변환
 * (2 bulsik: conversion english char. to medial vowel)
 * 결과: 중성이 아니면 0 (If it is not medial vowel, return 0).
 */
static int
vow(int c) {
  static char_u table[] =
      {
          /*O */ 6, /*P */ 12, /*Q */ 0, /*R */ 0, /*S */ 0, /*T */ 0, /*U */ 0,
          /*V */ 0, /*W */ 0, /*X */ 0, /*Y */ 0, /*Z */ 0, /*[ */ 0, /*\ */ 0,
          /*] */ 0, /*^ */ 0, /*_ */ 0, /*` */ 0, /*a */ 0, /*b */ 26, /*c */ 0,
          /*d */ 0, /*e */ 0, /*f */ 0, /*g */ 0, /*h */ 13, /*i */ 5, /*j */ 7,
          /*k */ 3, /*l */ 29, /*m */ 27, /*n */ 20, /*o */ 4, /*p */ 10, /*q */ 0,
          /*r */ 0, /*s */ 0, /*t */ 0, /*u */ 11, /*v */ 0, /*w */ 0, /*x */ 0,
          /*y */ 19};

  if (c < 'O' || c > 'y')
    return 0;
  return table[c - 'O'];
}

/* 2벌식에서 영문자 -> 받침 변환
 * (2 bulsik: conversion english char. to prop)
 * 결과: 받침이 아니면 0 (If not prop, return 0)
 */
static int
lcon(int c) {
  static char_u table[] =
      {
          /*R */ 3, /*S */ 0, /*T */ 22, /*U */ 0, /*V */ 0, /*W */ 0, /*X */ 0,
          /*Y */ 0, /*Z */ 0, /*[ */ 0, /*\ */ 0, /*] */ 0, /*^ */ 0, /*_ */ 0,
          /*` */ 0, /*a */ 17, /*b */ 0, /*c */ 25, /*d */ 23, /*e */ 8, /*f */ 9,
          /*g */ 29, /*h */ 0, /*i */ 0, /*j */ 0, /*k */ 0, /*l */ 0, /*m */ 0,
          /*n */ 0, /*o */ 0, /*p */ 0, /*q */ 19, /*r */ 2, /*s */ 5, /*t */ 21,
          /*u */ 0, /*v */ 28, /*w */ 24, /*x */ 27, /*y */ 0, /*z */ 26};

  if (c < 'R' || c > 'z')
    return 0;
  return table[c - 'R'];
}

/* 2 벌식에서 (현재 받침, 영문자 입력) -> 받침 변환
 * (2 bulsik: conversion (curr. prop, input english) to prop)
 */

static int
comcon2(int k, int c) {
  switch (k) {
  case 2: /* ㄱ */
    switch (c) {
    case 't':
      return 4; /* ㄱㅅ */
    }
    break;

  case 5: /* ㄴ */
    switch (c) {
    case 'w': /* ㄴㅈ */
      return 6;
    case 'g': /* ㄴㅎ */
      return 7;
    }
    break;

  case 9: /* ㄹ */
    switch (c) {
    case 'r': /* ㄹㄱ */
      return 10;
    case 'a': /* ㄹㅁ */
      return 11;
    case 'q': /* ㄹㅂ */
      return 12;
    case 't': /* ㄹㅅ */
      return 13;
    case 'x': /* ㄹㅌ */
      return 14;
    case 'v': /* ㄹㅍ */
      return 15;
    case 'g': /* ㄹㅎ */
      return 16;
    }
    break;

  case 19: /* ㅂ */
    switch (c) {
    case 't': /* ㅂㅅ */
      return 20;
    }
    break;
  }
  return 0;
}

/* 2벌식에서 (현재 중성, 영문 입력) -> 중성 변환
 * (2 bulsik: conversion (curr. medial vowel, input english) to medial
 * vowel)
 */

static int
comvow2(int v, int c) {
  switch (v) {
  case 13: /* ㅗ */
    switch (c) {
    case 'k': /* ㅗㅏ */
      return 14;
    case 'o': /* ㅗㅐ */
      return 15;
    case 'l': /* ㅗㅣ */
      return 18;
    }
    break;

  case 20: /* ㅜ */
    switch (c) {
    case 'j': /* ㅜㅓ */
      return 21;
    case 'p': /* ㅜㅔ */
      return 22;
    case 'l': /* ㅜㅣ */
      return 23;
    }
    break;

  case 27: /* ㅡ */
    switch (c) {
    case 'l': /* ㅡㅣ */
      return 28;
    }
    break;
  }
  return 0;
}

int hangul_input_state_get(void) {
  return hangul_input_state;
}

void hangul_input_state_set(int state) {
  hangul_input_state = state;
  hangul_input_clear();
}

int im_get_status(void) {
  return hangul_input_state_get();
}

void hangul_input_state_toggle(void) {
  if (hangul_input_state_get()) {
    hangul_input_state_set(0);
    if (composing_hangul) {
      push_raw_key(composing_hangul_buffer, 2);
      composing_hangul = 0;
    }
  } else
    hangul_input_state_set(1);

  if (showmode()) {
    setcursor();
  }

  gui_update_cursor(TRUE, FALSE);
}

static int
hangul_automata2(char_u *buf, int_u *c) {
  int t, t2;

  if (*c == BS) {
    if (sp == 0)
      return AUTOMATA_SPECIAL;
    else if (sp < 4) {
      hangul_input_clear();
      return AUTOMATA_NULL;
    }
    pop(buf);
    query(buf);
    convert_ks_to_3(buf, &f, &m, &l);
    last_l = last_ll;
    last_ll = -1;
    return AUTOMATA_CORRECT;
  }
  if ((!(*c >= 'A' && *c <= 'Z')) && (!(*c >= 'a' && *c <= 'z'))) {
    hangul_input_clear();
    return AUTOMATA_SPECIAL;
  }
  t = *c;
  switch (kind_table_for_2(t)) {
  case 0: /* 자음 (consonant) */
    if (f == F_NULL) {
      if (m != M_NULL)
        hangul_input_clear();
      f = fcon(t);
      convert_3_to_code(f, M_NULL, L_NULL, buf);
      push(buf);
      last_ll = last_l = -1;
      return AUTOMATA_NEW;
    }
    if (m == M_NULL)
      return AUTOMATA_ERROR;
    if (l == L_NULL) {
      t2 = lcon(t);
      if (!t2) /* 받침으로 적합하지않다 (cannot use it as a prop) */
      {
        hangul_input_clear();
        last_ll = last_l = -1;
        f = fcon(t);
        convert_3_to_code(f, m, l, buf);
        push(buf);
        return AUTOMATA_NEW;
      }
      if (2 == convert_3_to_code(f, m, t2, buf)) {
        last_ll = -1;
        last_l = t;
        l = t2;
        push(buf);
        return AUTOMATA_CORRECT;
      } else /* 받침으로 쓰려하였으나 code에 없는 글자이다 */
      {      /* cannot find such a prop in the code table */
        last_ll = last_l = -1;
        hangul_input_clear();
        f = fcon(t);
        convert_3_to_code(f, m, l, buf);
        push(buf);
        return AUTOMATA_NEW;
      }
    }
    /* 초 중 종성이 모두 갖추어져 있다
	     * I have all the 'initial sound' and 'medial vowel' and 'final
	     * consonant'.
	     */
    t2 = comcon2(l, t);
    if (t2) {
      if (2 == convert_3_to_code(f, m, t2, buf)) {
        l = t2;
        last_ll = last_l;
        last_l = t;
        push(buf);
        return AUTOMATA_CORRECT;
      }
    }
    last_ll = last_l = -1;
    hangul_input_clear();
    f = fcon(t);
    convert_3_to_code(f, m, l, buf);
    push(buf);
    return AUTOMATA_NEW;

  case 1:
    if (f == F_NULL) {
      hangul_input_clear();
      m = vow(t);
      convert_3_to_code(f, m, L_NULL, buf);
      push(buf);
      last_ll = last_l = -1;
      return AUTOMATA_NEW;
    }
    if (m == M_NULL) {
      m = vow(t);
      if (2 == convert_3_to_code(f, m, L_NULL, buf)) {
        last_ll = last_l = -1;
        push(buf);
        return AUTOMATA_CORRECT;
      }
      m = M_NULL;
      return AUTOMATA_ERROR;
    }
    if (l == L_NULL) {
      t2 = comvow2(m, t);
      if (t2) {
        if (2 != convert_3_to_code(f, t2, L_NULL, buf))
          return AUTOMATA_ERROR;

        m = t2;
        push(buf);
        last_ll = last_l = -1;
        return AUTOMATA_CORRECT;
      }
      return AUTOMATA_ERROR;
    }
    pop(buf);
    pop(buf);
    sp = 0;
    if (last_l == -1) {
      /* 음... 이게 필요하나?? (Hmm... Is it needed?) */
      convert_ks_to_3(buf, &f, &m, &l);
    } else {
      char_u tmp[3];
      f = fcon(last_l);
      convert_3_to_code(f, M_NULL, L_NULL, tmp);
      push(tmp);
    }
    m = vow(t);
    l = L_NULL;
    convert_3_to_code(f, m, l, buf + 2);
    push(buf + 2);
    return AUTOMATA_CORRECT_NEW;

  default:
    iemsg(_("E256: Hangul automata ERROR"));
    break;
  }
  return AUTOMATA_ERROR; /* RrEeAaLlLlYy EeRrRrOoRr */
}

static int
hangul_automata3(char_u *buf, int_u *c) {
  int t, t2;

  if (*c >= '!' && *c <= 'z') {
    *c -= '!';
    t = value_table_for_3[*c];
    switch (kind_table_for_3[*c]) {
    case F_F: /* 초성문자 (char. of an initial sound) */
      if (m != M_NULL || sp == 0) {
        /* 초성이 비었거나 다음 글자 모으기 시작
		     * Empty 'initial sound', so starting automata.
		     */
        hangul_input_clear();
        f = t;
        convert_3_to_code(f, M_NULL, L_NULL, buf);
        push(buf);
        return AUTOMATA_NEW;
      }
      if ((t2 = comfcon3(f, t)) != 0) /* 복자음 (double? consonant) */
      {
        f = t2;
        convert_3_to_code(f, M_NULL, L_NULL, buf);
        push(buf);
        return AUTOMATA_CORRECT;
      }
      return AUTOMATA_ERROR;

    case F_M: /* 모음 (vowel) */
      if (m == M_NULL) {
        if (2 != convert_3_to_code(f, t, L_NULL, buf))
          return AUTOMATA_ERROR;

        m = t;
        push(buf);
        if (f == F_NULL)
          return AUTOMATA_NEW;
        else
          return AUTOMATA_CORRECT;
      }
      if ((t2 = comvow3(m, t))) /* 복모음 (a diphthong) */
      {
        m = t2;
        convert_3_to_code(f, m, L_NULL, buf);
        push(buf);
        return AUTOMATA_CORRECT;
      }
      return AUTOMATA_ERROR;

    case F_L: /* 받침 (prop?) */
      if (m == M_NULL)
        return AUTOMATA_ERROR; /* 중성없는 종성 */
      if (l == L_NULL) {
        if (2 != convert_3_to_code(f, m, t, buf)) {
          l = L_NULL;
          return AUTOMATA_ERROR;
        }
        push(buf);
        l = t;
        return AUTOMATA_CORRECT;
      }
      if ((t2 = comcon3(l, t)) != 0) /* 복 받침 ?? (double prop?) */
      {
        if (2 != convert_3_to_code(f, m, t2, buf))
          return AUTOMATA_ERROR;

        push(buf);
        l = t2;
        return AUTOMATA_CORRECT;
      }
      return AUTOMATA_ERROR;

    case F_A: /* 특수문자나 숫자 (special char. or number) */
      hangul_input_clear();
      *c = t;
      return AUTOMATA_SPECIAL;
    }
  }
  if (*c == BS) {
    if (sp >= 4) {
      pop(buf);
      pop(buf);
      convert_ks_to_3(buf, &f, &m, &l);
      push(buf);
      return AUTOMATA_CORRECT;
    } else if (sp == 0) {
      return AUTOMATA_SPECIAL;
    } else {
      hangul_input_clear();
      return AUTOMATA_NULL;
    }
  }
  hangul_input_clear();
  return AUTOMATA_SPECIAL;
}

void hangul_keyboard_set(void) {
  int keyboard;
  char *s;

  hangul_input_clear();

  if ((s = getenv("VIM_KEYBOARD")) == NULL)
    s = getenv("HANGUL_KEYBOARD_TYPE");

  if (s) {
    if (*s == '2')
      keyboard = 2;
    else
      keyboard = 3;
    hangul_keyboard_type = keyboard;
  }
}

int hangul_input_process(char_u *s, int len) {
  int n;
  unsigned int c;
  char_u hanbuf[20];

  if (len == 1)
    /* normal key press */
    c = *s;
  else if (len == 3 && s[0] == CSI && s[1] == 'k' && s[2] == 'b') {
    /* backspace */
    if (composing_hangul)
      c = Ctrl_H;
    else
      return len;
  } else {
    if (composing_hangul)
      push_raw_key(composing_hangul_buffer, 2);
    hangul_input_clear();
    composing_hangul = 0;
    return len;
  }

  if (hangul_keyboard_type == 2)
    n = hangul_automata2(hanbuf, &c);
  else
    n = hangul_automata3(hanbuf, &c);

  if (n == AUTOMATA_CORRECT) {
    STRNCPY(composing_hangul_buffer, hanbuf, 2);
    gui_update_cursor(TRUE, FALSE);
    return 0;
  } else if (n == AUTOMATA_NEW) {
    if (composing_hangul)
      push_raw_key(composing_hangul_buffer, 2);
    STRNCPY(composing_hangul_buffer, hanbuf, 2);
    composing_hangul = 1;
    gui_update_cursor(TRUE, FALSE);
    return 0;
  } else if (n == AUTOMATA_CORRECT_NEW) {
    if (composing_hangul)
      push_raw_key(hanbuf, 2);
    STRNCPY(composing_hangul_buffer, hanbuf + 2, 2);
    composing_hangul = 1;
    gui_update_cursor(TRUE, FALSE);
    return 0;
  } else if (n == AUTOMATA_NULL) {
    composing_hangul = 0;
    gui_redraw_block(gui.cursor_row, gui.cursor_col,
                     gui.cursor_row, gui.cursor_col + 1,
                     GUI_MON_NOCLEAR);
    gui_update_cursor(TRUE, FALSE);
    return 0;
  } else if (n == AUTOMATA_SPECIAL) {
    if (composing_hangul) {
      push_raw_key(composing_hangul_buffer, 2);
      composing_hangul = 0;
    }
    *s = c;
    return 1;
  } else if (n == AUTOMATA_ERROR) {
    vim_beep(BO_HANGUL);
    return 0;
  }
  return len;
}

void hangul_input_clear(void) {
  sp = 0;
  f = F_NULL;
  m = M_NULL;
  l = L_NULL;
}

#define han_index(h, l) (((h)-0xb0) * (0xff - 0xa1) + ((l)-0xa1))

static const char_u ks_table1[][3] =
    {
        {2, 3, 1},
        {2, 3, 2},
        {2, 3, 5},
        {2, 3, 8},
        {2, 3, 9},
        {2, 3, 10},
        {2, 3, 11},
        {2, 3, 17},
        {2, 3, 19},
        {2, 3, 20},
        {2, 3, 21},
        {2, 3, 22},
        {2, 3, 23},
        {2, 3, 24},
        {2, 3, 25},
        {2, 3, 27},
        {2, 3, 28},
        {2, 3, 29},
        {2, 4, 1},
        {2, 4, 2},
        {2, 4, 5},
        {2, 4, 9},
        {2, 4, 17},
        {2, 4, 19},
        {2, 4, 21},
        {2, 4, 22},
        {2, 4, 23},
        {2, 5, 1},
        {2, 5, 2},
        {2, 5, 5},
        {2, 5, 9},
        {2, 5, 21},
        {2, 5, 23},
        {2, 6, 1},
        {2, 6, 5},
        {2, 6, 9},
        {2, 7, 1},
        {2, 7, 2},
        {2, 7, 5},
        {2, 7, 8},
        {2, 7, 9},
        {2, 7, 11},
        {2, 7, 17},
        {2, 7, 19},
        {2, 7, 21},
        {2, 7, 22},
        {2, 7, 23},
        {2, 7, 24},
        {2, 7, 27},
        {2, 7, 28},
        {2, 7, 29},
        {2, 10, 1},
        {2, 10, 5},
        {2, 10, 9},
        {2, 10, 17},
        {2, 10, 19},
        {2, 10, 21},
        {2, 10, 22},
        {2, 10, 23},
        {2, 11, 1},
        {2, 11, 2},
        {2, 11, 3},
        {2, 11, 5},
        {2, 11, 8},
        {2, 11, 9},
        {2, 11, 17},
        {2, 11, 19},
        {2, 11, 21},
        {2, 11, 22},
        {2, 11, 23},
        {2, 11, 27},
        {2, 12, 1},
        {2, 12, 5},
        {2, 12, 9},
        {2, 12, 19},
        {2, 12, 21},
        {2, 13, 1},
        {2, 13, 2},
        {2, 13, 5},
        {2, 13, 8},
        {2, 13, 9},
        {2, 13, 11},
        {2, 13, 13},
        {2, 13, 16},
        {2, 13, 17},
        {2, 13, 19},
        {2, 13, 21},
        {2, 13, 23},
        {2, 13, 24},
        {2, 14, 1},
        {2, 14, 2},
        {2, 14, 5},
        {2, 14, 9},
        {2, 14, 11},
        {2, 14, 17},
        {2, 14, 19},
        {2, 14, 21},
        {2, 14, 23},
        {2, 15, 1},
        {2, 15, 5},
        {2, 15, 9},
        {2, 15, 19},
        {2, 15, 22},
        {2, 15, 23},
        {2, 18, 1},
        {2, 18, 2},
        {2, 18, 5},
        {2, 18, 9},
        {2, 18, 17},
        {2, 18, 19},
        {2, 18, 21},
        {2, 18, 23},
        {2, 19, 1},
        {2, 19, 5},
        {2, 19, 9},
        {2, 19, 19},
        {2, 19, 21},
        {2, 20, 1},
        {2, 20, 2},
        {2, 20, 5},
        {2, 20, 8},
        {2, 20, 9},
        {2, 20, 10},
        {2, 20, 11},
        {2, 20, 16},
        {2, 20, 17},
        {2, 20, 19},
        {2, 20, 21},
        {2, 20, 23},
        {2, 20, 24},
        {2, 21, 1},
        {2, 21, 2},
        {2, 21, 5},
        {2, 21, 9},
        {2, 21, 22},
        {2, 21, 23},
        {2, 22, 1},
        {2, 22, 21},
        {2, 23, 1},
        {2, 23, 2},
        {2, 23, 5},
        {2, 23, 9},
        {2, 23, 17},
        {2, 23, 19},
        {2, 23, 21},
        {2, 26, 1},
        {2, 26, 5},
        {2, 26, 9},
        {2, 27, 1},
        {2, 27, 2},
        {2, 27, 5},
        {2, 27, 8},
        {2, 27, 9},
        {2, 27, 10},
        {2, 27, 17},
        {2, 27, 19},
        {2, 27, 21},
        {2, 27, 23},
        {2, 28, 1},
        {2, 29, 1},
        {2, 29, 2},
        {2, 29, 5},
        {2, 29, 8},
        {2, 29, 9},
        {2, 29, 11},
        {2, 29, 17},
        {2, 29, 19},
        {2, 29, 21},
        {2, 29, 23},
        {2, 29, 24},
        {2, 29, 28},
        {3, 3, 1},
        {3, 3, 2},
        {3, 3, 3},
        {3, 3, 5},
        {3, 3, 9},
        {3, 3, 11},
        {3, 3, 17},
        {3, 3, 19},
        {3, 3, 21},
        {3, 3, 22},
        {3, 3, 23},
        {3, 3, 27},
        {3, 4, 1},
        {3, 4, 2},
        {3, 4, 5},
        {3, 4, 9},
        {3, 4, 17},
        {3, 4, 19},
        {3, 4, 21},
        {3, 4, 22},
        {3, 4, 23},
        {3, 5, 1},
        {3, 5, 2},
        {3, 5, 9},
        {3, 7, 1},
        {3, 7, 2},
        {3, 7, 3},
        {3, 7, 5},
        {3, 7, 9},
        {3, 7, 17},
        {3, 7, 19},
        {3, 7, 21},
        {3, 7, 22},
        {3, 7, 23},
        {3, 10, 1},
        {3, 10, 2},
        {3, 10, 5},
        {3, 10, 17},
        {3, 10, 21},
        {3, 10, 23},
        {3, 11, 1},
        {3, 11, 5},
        {3, 11, 9},
        {3, 11, 21},
        {3, 11, 22},
        {3, 11, 27},
        {3, 12, 1},
        {3, 13, 1},
        {3, 13, 2},
        {3, 13, 5},
        {3, 13, 7},
        {3, 13, 9},
        {3, 13, 17},
        {3, 13, 19},
        {3, 13, 21},
        {3, 13, 23},
        {3, 13, 24},
        {3, 13, 25},
        {3, 14, 1},
        {3, 14, 2},
        {3, 14, 9},
        {3, 14, 22},
        {3, 14, 23},
        {3, 15, 1},
        {3, 15, 2},
        {3, 15, 23},
        {3, 18, 1},
        {3, 18, 5},
        {3, 18, 9},
        {3, 18, 17},
        {3, 18, 19},
        {3, 18, 23},
        {3, 19, 1},
        {3, 20, 1},
        {3, 20, 2},
        {3, 20, 5},
        {3, 20, 9},
        {3, 20, 16},
        {3, 20, 17},
        {3, 20, 19},
        {3, 20, 21},
        {3, 20, 23},
        {3, 20, 24},
        {3, 21, 1},
        {3, 21, 9},
        {3, 21, 22},
        {3, 21, 23},
        {3, 22, 1},
        {3, 22, 2},
        {3, 22, 5},
        {3, 22, 9},
        {3, 22, 17},
        {3, 22, 19},
        {3, 22, 22},
        {3, 23, 1},
        {3, 23, 5},
        {3, 23, 9},
        {3, 23, 17},
        {3, 23, 19},
        {3, 26, 1},
        {3, 27, 1},
        {3, 27, 2},
        {3, 27, 5},
        {3, 27, 7},
        {3, 27, 9},
        {3, 27, 11},
        {3, 27, 16},
        {3, 27, 17},
        {3, 27, 19},
        {3, 27, 21},
        {3, 27, 23},
        {3, 27, 27},
        {3, 29, 1},
        {3, 29, 2},
        {3, 29, 5},
        {3, 29, 9},
        {3, 29, 17},
        {3, 29, 19},
        {3, 29, 21},
        {3, 29, 23},
        {4, 3, 1},
        {4, 3, 2},
        {4, 3, 3},
        {4, 3, 5},
        {4, 3, 8},
        {4, 3, 9},
        {4, 3, 10},
        {4, 3, 11},
        {4, 3, 17},
        {4, 3, 19},
        {4, 3, 21},
        {4, 3, 22},
        {4, 3, 23},
        {4, 3, 24},
        {4, 3, 25},
        {4, 3, 27},
        {4, 3, 29},
        {4, 4, 1},
        {4, 4, 2},
        {4, 4, 5},
        {4, 4, 9},
        {4, 4, 17},
        {4, 4, 19},
        {4, 4, 21},
        {4, 4, 22},
        {4, 4, 23},
        {4, 5, 1},
        {4, 5, 2},
        {4, 5, 5},
        {4, 5, 9},
        {4, 5, 17},
        {4, 5, 23},
        {4, 7, 1},
        {4, 7, 2},
        {4, 7, 4},
        {4, 7, 5},
        {4, 7, 9},
        {4, 7, 11},
        {4, 7, 12},
        {4, 7, 17},
        {4, 7, 19},
        {4, 7, 21},
        {4, 7, 22},
        {4, 7, 23},
        {4, 7, 29},
        {4, 10, 1},
        {4, 10, 2},
        {4, 10, 5},
        {4, 10, 9},
        {4, 10, 17},
        {4, 10, 19},
        {4, 10, 21},
        {4, 10, 22},
        {4, 10, 23},
        {4, 11, 1},
        {4, 11, 2},
        {4, 11, 5},
        {4, 11, 9},
        {4, 11, 17},
        {4, 11, 19},
        {4, 11, 22},
        {4, 11, 23},
        {4, 11, 26},
        {4, 12, 1},
        {4, 12, 5},
        {4, 13, 1},
        {4, 13, 2},
        {4, 13, 5},
        {4, 13, 9},
        {4, 13, 11},
        {4, 13, 17},
        {4, 13, 19},
        {4, 13, 21},
        {4, 13, 23},
        {4, 13, 28},
        {4, 13, 29},
        {4, 14, 1},
        {4, 14, 5},
        {4, 14, 9},
        {4, 14, 22},
        {4, 18, 1},
        {4, 18, 5},
        {4, 18, 9},
        {4, 18, 17},
        {4, 18, 19},
        {4, 18, 21},
        {4, 19, 1},
        {4, 19, 2},
        {4, 19, 5},
        {4, 19, 9},
        {4, 19, 19},
        {4, 19, 21},
        {4, 19, 23},
        {4, 20, 1},
        {4, 20, 2},
        {4, 20, 5},
        {4, 20, 8},
        {4, 20, 9},
        {4, 20, 17},
        {4, 20, 19},
        {4, 20, 21},
        {4, 20, 23},
        {4, 21, 1},
        {4, 21, 22},
        {4, 22, 1},
        {4, 23, 1},
        {4, 23, 5},
        {4, 23, 9},
        {4, 23, 17},
        {4, 23, 19},
        {4, 26, 1},
        {4, 26, 2},
        {4, 26, 9},
        {4, 26, 17},
        {4, 26, 19},
        {4, 26, 23},
        {4, 27, 1},
        {4, 27, 2},
        {4, 27, 5},
        {4, 27, 9},
        {4, 27, 10},
        {4, 27, 11},
        {4, 27, 17},
        {4, 27, 19},
        {4, 27, 21},
        {4, 27, 23},
        {4, 27, 24},
        {4, 27, 28},
        {4, 28, 1},
        {4, 28, 5},
        {4, 28, 9},
        {4, 29, 1},
        {4, 29, 2},
        {4, 29, 5},
        {4, 29, 9},
        {4, 29, 11},
        {4, 29, 17},
        {4, 29, 19},
        {4, 29, 21},
        {4, 29, 23},
        {4, 29, 28},
        {5, 3, 1},
        {5, 3, 2},
        {5, 3, 3},
        {5, 3, 5},
        {5, 3, 8},
        {5, 3, 9},
        {5, 3, 10},
        {5, 3, 11},
        {5, 3, 12},
        {5, 3, 16},
        {5, 3, 17},
        {5, 3, 19},
        {5, 3, 21},
        {5, 3, 22},
        {5, 3, 23},
        {5, 3, 24},
        {5, 3, 25},
        {5, 3, 29},
        {5, 4, 1},
        {5, 4, 2},
        {5, 4, 5},
        {5, 4, 9},
        {5, 4, 17},
        {5, 4, 19},
        {5, 4, 21},
        {5, 4, 22},
        {5, 4, 23},
        {5, 5, 1},
        {5, 7, 1},
        {5, 7, 2},
        {5, 7, 3},
        {5, 7, 5},
        {5, 7, 8},
        {5, 7, 9},
        {5, 7, 11},
        {5, 7, 12},
        {5, 7, 17},
        {5, 7, 19},
        {5, 7, 21},
        {5, 7, 23},
        {5, 7, 25},
        {5, 7, 28},
        {5, 10, 1},
        {5, 10, 2},
        {5, 10, 5},
        {5, 10, 9},
        {5, 10, 17},
        {5, 10, 19},
        {5, 10, 21},
        {5, 10, 22},
        {5, 10, 23},
        {5, 11, 1},
        {5, 11, 5},
        {5, 11, 9},
        {5, 11, 22},
        {5, 11, 23},
        {5, 12, 1},
        {5, 12, 5},
        {5, 13, 1},
        {5, 13, 2},
        {5, 13, 5},
        {5, 13, 8},
        {5, 13, 9},
        {5, 13, 11},
        {5, 13, 13},
        {5, 13, 17},
        {5, 13, 19},
        {5, 13, 21},
        {5, 13, 23},
        {5, 13, 25},
        {5, 13, 27},
        {5, 14, 1},
        {5, 14, 5},
        {5, 14, 9},
        {5, 15, 1},
        {5, 15, 22},
        {5, 18, 1},
        {5, 18, 5},
        {5, 18, 9},
        {5, 18, 17},
        {5, 18, 19},
        {5, 18, 21},
        {5, 19, 1},
        {5, 20, 1},
        {5, 20, 2},
        {5, 20, 5},
        {5, 20, 9},
        {5, 20, 17},
        {5, 20, 19},
        {5, 20, 21},
        {5, 20, 23},
        {5, 21, 1},
        {5, 21, 22},
        {5, 22, 1},
        {5, 22, 23},
        {5, 23, 1},
        {5, 23, 5},
        {5, 23, 9},
        {5, 23, 19},
        {5, 23, 21},
        {5, 23, 23},
        {5, 26, 1},
        {5, 26, 5},
        {5, 26, 9},
        {5, 26, 17},
        {5, 26, 23},
        {5, 27, 1},
        {5, 27, 2},
        {5, 27, 5},
        {5, 27, 8},
        {5, 27, 9},
        {5, 27, 11},
        {5, 27, 17},
        {5, 27, 19},
        {5, 27, 21},
        {5, 27, 23},
        {5, 28, 1},
        {5, 29, 1},
        {5, 29, 2},
        {5, 29, 5},
        {5, 29, 8},
        {5, 29, 9},
        {5, 29, 17},
        {5, 29, 19},
        {5, 29, 21},
        {5, 29, 22},
        {5, 29, 23},
        {5, 29, 24},
        {6, 3, 1},
        {6, 3, 2},
        {6, 3, 5},
        {6, 3, 9},
        {6, 3, 17},
        {6, 3, 19},
        {6, 3, 21},
        {6, 3, 22},
        {6, 3, 23},
        {6, 3, 29},
        {6, 4, 1},
        {6, 4, 2},
        {6, 4, 5},
        {6, 4, 9},
        {6, 4, 17},
        {6, 4, 19},
        {6, 4, 21},
        {6, 4, 22},
        {6, 4, 23},
        {6, 7, 1},
        {6, 7, 2},
        {6, 7, 5},
        {6, 7, 9},
        {6, 7, 11},
        {6, 7, 12},
        {6, 7, 17},
        {6, 7, 19},
        {6, 7, 21},
        {6, 7, 22},
        {6, 7, 23},
        {6, 7, 29},
        {6, 10, 1},
        {6, 10, 2},
        {6, 10, 5},
        {6, 10, 9},
        {6, 10, 17},
        {6, 10, 19},
        {6, 10, 21},
        {6, 10, 22},
        {6, 10, 23},
        {6, 11, 1},
        {6, 11, 22},
        {6, 13, 1},
        {6, 13, 2},
        {6, 13, 5},
        {6, 13, 9},
        {6, 13, 23},
        {6, 14, 1},
        {6, 14, 9},
        {6, 15, 1},
        {6, 18, 1},
        {6, 18, 5},
        {6, 20, 1},
        {6, 20, 2},
        {6, 20, 5},
        {6, 20, 9},
        {6, 20, 16},
        {6, 20, 17},
        {6, 20, 23},
        {6, 22, 1},
        {6, 23, 1},
        {6, 23, 5},
        {6, 23, 9},
        {6, 23, 17},
        {6, 23, 19},
        {6, 23, 23},
        {6, 27, 1},
        {6, 27, 2},
        {6, 27, 5},
        {6, 27, 8},
        {6, 27, 9},
        {6, 27, 17},
        {6, 27, 19},
        {6, 27, 21},
        {6, 28, 1},
        {6, 28, 5},
        {6, 28, 9},
        {6, 28, 17},
        {6, 28, 19},
        {6, 29, 1},
        {6, 29, 5},
        {6, 29, 9},
        {6, 29, 17},
        {6, 29, 19},
        {6, 29, 21},
        {6, 29, 23},
        {7, 3, 1},
        {7, 3, 2},
        {7, 3, 5},
        {7, 3, 9},
        {7, 3, 17},
        {7, 3, 19},
        {7, 3, 21},
        {7, 3, 22},
        {7, 3, 23},
        {7, 3, 24},
        {7, 3, 28},
        {7, 3, 29},
        {7, 4, 1},
        {7, 4, 2},
        {7, 4, 5},
        {7, 4, 9},
        {7, 4, 17},
        {7, 4, 19},
        {7, 4, 21},
        {7, 4, 22},
        {7, 4, 23},
        {7, 5, 1},
        {7, 5, 2},
        {7, 5, 5},
        {7, 5, 21},
        {7, 5, 23},
        {7, 7, 1},
        {7, 7, 2},
        {7, 7, 5},
        {7, 7, 9},
        {7, 7, 17},
        {7, 7, 19},
        {7, 7, 21},
        {7, 7, 22},
        {7, 7, 23},
        {7, 7, 29},
        {7, 10, 1},
        {7, 10, 2},
        {7, 10, 5},
        {7, 10, 9},
        {7, 10, 17},
        {7, 10, 19},
        {7, 10, 21},
        {7, 10, 23},
        {7, 11, 1},
        {7, 11, 2},
        {7, 11, 5},
        {7, 11, 9},
        {7, 11, 17},
        {7, 11, 19},
        {7, 11, 21},
        {7, 11, 22},
        {7, 11, 23},
        {7, 12, 1},
        {7, 12, 5},
        {7, 12, 19},
        {7, 12, 21},
        {7, 13, 1},
        {7, 13, 2},
        {7, 13, 5},
        {7, 13, 9},
        {7, 13, 17},
        {7, 13, 19},
        {7, 13, 21},
        {7, 13, 23},
        {7, 14, 1},
        {7, 14, 5},
        {7, 14, 23},
        {7, 15, 22},
        {7, 18, 1},
        {7, 18, 5},
        {7, 18, 9},
        {7, 18, 17},
        {7, 18, 19},
        {7, 18, 21},
        {7, 18, 23},
        {7, 19, 1},
        {7, 19, 5},
        {7, 19, 9},
        {7, 19, 19},
        {7, 19, 21},
        {7, 19, 23},
        {7, 20, 1},
        {7, 20, 2},
        {7, 20, 5},
        {7, 20, 9},
        {7, 20, 17},
        {7, 20, 19},
        {7, 20, 21},
        {7, 20, 23},
        {7, 21, 1},
        {7, 21, 22},
        {7, 22, 1},
        {7, 23, 1},
        {7, 23, 2},
        {7, 23, 5},
        {7, 23, 9},
        {7, 23, 17},
        {7, 23, 21},
        {7, 23, 23},
        {7, 26, 1},
        {7, 26, 2},
        {7, 26, 5},
        {7, 26, 9},
        {7, 26, 17},
        {7, 26, 19},
        {7, 26, 21},
        {7, 26, 23},
        {7, 27, 1},
        {7, 27, 2},
        {7, 27, 5},
        {7, 27, 9},
        {7, 27, 17},
        {7, 27, 19},
        {7, 27, 21},
        {7, 27, 23},
        {7, 27, 24},
        {7, 27, 27},
        {7, 27, 28},
        {7, 29, 1},
        {7, 29, 2},
        {7, 29, 5},
        {7, 29, 9},
        {7, 29, 17},
        {7, 29, 19},
        {7, 29, 21},
        {7, 29, 23},
        {8, 3, 1},
        {8, 3, 2},
        {8, 3, 5},
        {8, 3, 7},
        {8, 3, 8},
        {8, 3, 9},
        {8, 3, 10},
        {8, 3, 11},
        {8, 3, 17},
        {8, 3, 19},
        {8, 3, 21},
        {8, 3, 23},
        {8, 3, 24},
        {8, 3, 27},
        {8, 3, 29},
        {8, 4, 1},
        {8, 4, 2},
        {8, 4, 5},
        {8, 4, 9},
        {8, 4, 17},
        {8, 4, 19},
        {8, 4, 21},
        {8, 4, 22},
        {8, 4, 23},
        {8, 4, 24},
        {8, 5, 1},
        {8, 5, 2},
        {8, 5, 9},
        {8, 5, 23},
        {8, 7, 1},
        {8, 7, 2},
        {8, 7, 5},
        {8, 7, 9},
        {8, 7, 11},
        {8, 7, 17},
        {8, 7, 19},
        {8, 7, 21},
        {8, 7, 23},
        {8, 7, 24},
        {8, 7, 29},
        {8, 10, 1},
        {8, 10, 2},
        {8, 10, 5},
        {8, 10, 9},
        {8, 10, 17},
        {8, 10, 19},
        {8, 10, 21},
        {8, 10, 22},
        {8, 10, 23},
        {8, 11, 1},
        {8, 11, 2},
        {8, 11, 5},
        {8, 11, 9},
        {8, 11, 21},
        {8, 11, 22},
        {8, 11, 23},
        {8, 11, 25},
        {8, 12, 1},
        {8, 13, 1},
        {8, 13, 2},
        {8, 13, 4},
        {8, 13, 5},
        {8, 13, 9},
        {8, 13, 11},
        {8, 13, 17},
        {8, 13, 19},
        {8, 13, 21},
        {8, 13, 23},
        {8, 14, 1},
        {8, 14, 5},
        {8, 14, 22},
        {8, 14, 23},
        {8, 18, 1},
        {8, 18, 5},
        {8, 18, 9},
        {8, 18, 19},
        {8, 18, 21},
        {8, 18, 23},
        {8, 19, 1},
        {8, 19, 5},
        {8, 19, 9},
        {8, 19, 19},
        {8, 19, 21},
        {8, 20, 1},
        {8, 20, 2},
        {8, 20, 3},
        {8, 20, 5},
        {8, 20, 8},
        {8, 20, 9},
        {8, 20, 10},
        {8, 20, 11},
        {8, 20, 17},
        {8, 20, 19},
        {8, 20, 21},
        {8, 20, 23},
        {8, 20, 27},
        {8, 20, 29},
        {8, 21, 1},
        {8, 21, 5},
        {8, 21, 9},
        {8, 21, 19},
        {8, 21, 21},
        {8, 22, 1},
        {8, 23, 1},
        {8, 23, 5},
        {8, 23, 9},
        {8, 26, 1},
        {8, 26, 5},
        {8, 26, 9},
        {8, 26, 17},
        {8, 26, 21},
        {8, 27, 1},
        {8, 27, 5},
        {8, 27, 9},
        {8, 27, 17},
        {8, 27, 21},
        {8, 29, 1},
        {8, 29, 2},
        {8, 29, 5},
        {8, 29, 8},
        {8, 29, 9},
        {8, 29, 11},
        {8, 29, 17},
        {8, 29, 19},
        {8, 29, 21},
        {8, 29, 22},
        {8, 29, 23},
        {8, 29, 25},
        {8, 29, 27},
        {9, 3, 1},
        {9, 3, 2},
        {9, 3, 3},
        {9, 3, 4},
        {9, 3, 5},
        {9, 3, 8},
        {9, 3, 9},
        {9, 3, 10},
        {9, 3, 11},
        {9, 3, 12},
        {9, 3, 17},
        {9, 3, 19},
        {9, 3, 21},
        {9, 3, 23},
        {9, 3, 27},
        {9, 4, 1},
        {9, 4, 2},
        {9, 4, 5},
        {9, 4, 9},
        {9, 4, 17},
        {9, 4, 19},
        {9, 4, 21},
        {9, 4, 22},
        {9, 4, 23},
        {9, 4, 27},
        {9, 5, 1},
        {9, 5, 2},
        {9, 5, 5},
        {9, 5, 19},
        {9, 7, 1},
        {9, 7, 2},
        {9, 7, 5},
        {9, 7, 8},
        {9, 7, 9},
        {9, 7, 11},
        {9, 7, 17},
        {9, 7, 19},
        {9, 7, 21},
        {9, 7, 23},
        {9, 7, 24},
        {9, 10, 1},
        {9, 10, 2},
        {9, 10, 5},
        {9, 10, 8},
        {9, 10, 9},
        {9, 10, 17},
        {9, 10, 19},
        {9, 10, 21},
        {9, 10, 22},
        {9, 10, 23},
        {9, 11, 1},
        {9, 11, 2},
        {9, 11, 5},
        {9, 11, 9},
        {9, 11, 19},
        {9, 11, 21},
        {9, 11, 22},
        {9, 11, 23},
        {9, 11, 27},
        {9, 12, 1},
        {9, 12, 5},
        {9, 13, 1},
        {9, 13, 2},
        {9, 13, 3},
        {9, 13, 5},
        {9, 13, 9},
        {9, 13, 17},
        {9, 13, 19},
        {9, 13, 21},
        {9, 13, 23},
        {9, 14, 1},
        {9, 14, 5},
        {9, 14, 22},
        {9, 15, 1},
        {9, 15, 22},
        {9, 18, 1},
        {9, 18, 2},
        {9, 18, 5},
        {9, 18, 9},
        {9, 18, 17},
        {9, 18, 19},
        {9, 19, 1},
        {9, 19, 5},
        {9, 20, 1},
        {9, 20, 2},
        {9, 20, 5},
        {9, 20, 8},
        {9, 20, 9},
        {9, 20, 10},
        {9, 20, 11},
        {9, 20, 17},
        {9, 20, 19},
        {9, 20, 21},
        {9, 20, 23},
        {9, 20, 27},
        {9, 20, 28},
        {9, 21, 1},
        {9, 21, 9},
        {9, 21, 22},
        {9, 22, 1},
        {9, 23, 1},
        {9, 23, 2},
        {9, 23, 5},
        {9, 23, 9},
        {9, 23, 23},
        {9, 26, 1},
        {9, 26, 5},
        {9, 26, 9},
        {9, 26, 17},
        {9, 26, 21},
        {9, 26, 23},
        {9, 27, 1},
        {9, 27, 2},
        {9, 27, 5},
        {9, 27, 9},
        {9, 27, 17},
        {9, 27, 19},
        {9, 27, 21},
        {9, 29, 1},
        {9, 29, 2},
        {9, 29, 5},
        {9, 29, 9},
        {9, 29, 11},
        {9, 29, 17},
        {9, 29, 19},
        {9, 29, 21},
        {9, 29, 23},
        {9, 29, 24},
        {9, 29, 25},
        {10, 3, 1},
        {10, 3, 2},
        {10, 3, 5},
        {10, 3, 9},
        {10, 3, 11},
        {10, 3, 17},
        {10, 3, 19},
        {10, 3, 21},
        {10, 3, 22},
        {10, 3, 23},
        {10, 3, 29},
        {10, 4, 1},
        {10, 4, 2},
        {10, 4, 5},
        {10, 4, 9},
        {10, 4, 17},
        {10, 4, 19},
        {10, 4, 21},
        {10, 4, 22},
        {10, 4, 23},
        {10, 5, 1},
        {10, 5, 2},
        {10, 5, 17},
        {10, 7, 1},
        {10, 7, 2},
        {10, 7, 5},
        {10, 7, 8},
        {10, 7, 9},
        {10, 7, 17},
        {10, 7, 21},
        {10, 7, 22},
        {10, 7, 23},
        {10, 10, 1},
        {10, 10, 23},
        {10, 11, 1},
        {10, 11, 2},
        {10, 11, 17},
        {10, 11, 19},
        {10, 11, 21},
        {10, 11, 22},
        {10, 11, 23},
        {10, 13, 1},
        {10, 13, 2},
        {10, 13, 5},
        {10, 13, 9},
        {10, 13, 17},
        {10, 13, 19},
        {10, 13, 23},
        {10, 18, 1},
        {10, 19, 1},
        {10, 19, 23},
        {10, 20, 1},
        {10, 20, 2},
        {10, 20, 5},
        {10, 20, 9},
        {10, 20, 17},
        {10, 20, 21},
        {10, 20, 23},
        {10, 26, 1},
        {10, 26, 23},
        {10, 27, 1},
        {10, 27, 5},
        {10, 27, 9},
        {10, 27, 17},
        {10, 27, 19},
        {10, 29, 1},
        {10, 29, 2},
        {10, 29, 5},
        {10, 29, 9},
        {10, 29, 17},
        {10, 29, 19},
        {10, 29, 21},
        {10, 29, 23},
        {11, 3, 1},
        {11, 3, 2},
        {11, 3, 4},
        {11, 3, 5},
        {11, 3, 8},
        {11, 3, 9},
        {11, 3, 10},
        {11, 3, 11},
        {11, 3, 17},
        {11, 3, 19},
        {11, 3, 21},
        {11, 3, 22},
        {11, 3, 23},
        {11, 3, 27},
        {11, 4, 1},
        {11, 4, 2},
        {11, 4, 5},
        {11, 4, 9},
        {11, 4, 17},
        {11, 4, 19},
        {11, 4, 21},
        {11, 4, 22},
        {11, 4, 23},
        {11, 5, 1},
        {11, 5, 2},
        {11, 5, 5},
        {11, 5, 9},
        {11, 5, 17},
        {11, 5, 19},
        {11, 5, 21},
        {11, 5, 23},
        {11, 6, 1},
        {11, 6, 5},
        {11, 6, 9},
        {11, 6, 17},
        {11, 6, 23},
        {11, 7, 1},
        {11, 7, 2},
        {11, 7, 3},
        {11, 7, 4},
        {11, 7, 5},
        {11, 7, 8},
        {11, 7, 9},
        {11, 7, 11},
        {11, 7, 12},
        {11, 7, 17},
        {11, 7, 19},
        {11, 7, 21},
        {11, 7, 22},
        {11, 7, 23},
        {11, 7, 28},
        {11, 10, 1},
        {11, 10, 2},
        {11, 10, 5},
        {11, 10, 9},
        {11, 10, 17},
        {11, 10, 19},
        {11, 10, 21},
        {11, 10, 22},
        {11, 10, 23},
        {11, 11, 1},
        {11, 11, 2},
        {11, 11, 5},
        {11, 11, 9},
        {11, 11, 17},
        {11, 11, 19},
        {11, 11, 21},
        {11, 11, 22},
        {11, 11, 23},
        {11, 12, 1},
        {11, 12, 5},
        {11, 12, 9},
        {11, 12, 23},
        {11, 13, 1},
        {11, 13, 2},
        {11, 13, 3},
        {11, 13, 5},
        {11, 13, 9},
        {11, 13, 11},
        {11, 13, 17},
        {11, 13, 19},
        {11, 13, 21},
        {11, 13, 23},
        {11, 13, 27},
        {11, 14, 1},
        {11, 14, 2},
        {11, 14, 5},
        {11, 14, 9},
        {11, 14, 23},
        {11, 15, 1},
        {11, 15, 5},
        {11, 15, 9},
        {11, 15, 17},
        {11, 15, 21},
        {11, 15, 22},
        {11, 18, 1},
        {11, 18, 5},
        {11, 18, 9},
        {11, 18, 17},
        {11, 18, 19},
        {11, 18, 21},
        {11, 19, 1},
        {11, 19, 2},
        {11, 19, 5},
        {11, 19, 9},
        {11, 19, 17},
        {11, 19, 19},
        {11, 19, 21},
        {11, 19, 23},
        {11, 20, 1},
        {11, 20, 2},
        {11, 20, 5},
        {11, 20, 8},
        {11, 20, 9},
        {11, 20, 17},
        {11, 20, 19},
        {11, 20, 21},
        {11, 20, 23},
        {11, 20, 25},
        {11, 20, 27},
        {11, 20, 28},
        {11, 21, 1},
        {11, 21, 22},
        {11, 22, 1},
        {11, 22, 2},
        {11, 22, 5},
        {11, 22, 9},
        {11, 22, 17},
        {11, 22, 23},
        {11, 23, 1},
        {11, 23, 2},
        {11, 23, 5},
        {11, 23, 9},
        {11, 23, 17},
        {11, 23, 19},
        {11, 23, 21},
        {11, 23, 23},
        {11, 26, 1},
        {11, 26, 2},
        {11, 26, 9},
        {11, 26, 17},
        {11, 26, 21},
        {11, 26, 23},
        {11, 27, 1},
        {11, 27, 2},
        {11, 27, 5},
        {11, 27, 9},
        {11, 27, 10},
        {11, 27, 17},
        {11, 27, 19},
        {11, 27, 21},
        {11, 27, 23},
        {11, 29, 1},
        {11, 29, 2},
        {11, 29, 5},
        {11, 29, 8},
        {11, 29, 9},
        {11, 29, 16},
        {11, 29, 17},
        {11, 29, 19},
        {11, 29, 21},
        {11, 29, 23},
        {11, 29, 28},
        {12, 3, 1},
        {12, 3, 2},
        {12, 3, 4},
        {12, 3, 5},
        {12, 3, 9},
        {12, 3, 17},
        {12, 3, 19},
        {12, 3, 22},
        {12, 3, 23},
        {12, 3, 29},
        {12, 4, 1},
        {12, 4, 2},
        {12, 4, 5},
        {12, 4, 9},
        {12, 4, 17},
        {12, 4, 19},
        {12, 4, 22},
        {12, 4, 23},
        {12, 5, 23},
        {12, 7, 1},
        {12, 7, 2},
        {12, 7, 5},
        {12, 7, 9},
        {12, 7, 11},
        {12, 7, 17},
        {12, 7, 19},
        {12, 7, 22},
        {12, 7, 23},
        {12, 10, 1},
        {12, 10, 5},
        {12, 10, 9},
        {12, 12, 5},
        {12, 13, 1},
        {12, 13, 2},
        {12, 13, 5},
        {12, 13, 8},
        {12, 13, 9},
        {12, 13, 11},
        {12, 13, 17},
        {12, 13, 19},
        {12, 13, 23},
        {12, 14, 1},
        {12, 14, 2},
        {12, 14, 5},
        {12, 14, 22},
        {12, 15, 1},
        {12, 15, 22},
        {12, 18, 1},
        {12, 18, 5},
        {12, 18, 9},
        {12, 18, 17},
        {12, 18, 19},
        {12, 19, 1},
        {12, 20, 1},
        {12, 20, 2},
        {12, 20, 5},
        {12, 20, 9},
        {12, 20, 17},
        {12, 20, 19},
        {12, 20, 23},
        {12, 21, 1},
        {12, 21, 22},
        {12, 22, 1},
        {12, 23, 1},
        {12, 23, 5},
        {12, 26, 23},
        {12, 27, 1},
        {12, 27, 2},
        {12, 27, 5},
        {12, 27, 9},
        {12, 27, 11},
        {12, 27, 16},
        {12, 27, 17},
        {12, 27, 19},
        {12, 28, 1},
        {12, 28, 5},
        {12, 28, 9},
        {12, 28, 17},
        {12, 29, 1},
        {12, 29, 2},
        {12, 29, 5},
        {12, 29, 9},
        {12, 29, 17},
        {12, 29, 19},
        {12, 29, 21},
        {12, 29, 23},
        {13, 3, 1},
        {13, 3, 2},
        {13, 3, 5},
        {13, 3, 6},
        {13, 3, 7},
        {13, 3, 9},
        {13, 3, 10},
        {13, 3, 11},
        {13, 3, 16},
        {13, 3, 17},
        {13, 3, 19},
        {13, 3, 21},
        {13, 3, 22},
        {13, 3, 23},
        {13, 3, 27},
        {13, 3, 28},
        {13, 4, 1},
        {13, 4, 2},
        {13, 4, 5},
        {13, 4, 9},
        {13, 4, 17},
        {13, 4, 19},
        {13, 4, 21},
        {13, 4, 22},
        {13, 4, 23},
        {13, 5, 1},
        {13, 5, 2},
        {13, 5, 5},
        {13, 5, 9},
        {13, 5, 12},
        {13, 5, 17},
        {13, 5, 19},
        {13, 5, 21},
        {13, 5, 23},
        {13, 5, 27},
        {13, 5, 29},
        {13, 6, 1},
        {13, 6, 5},
        {13, 6, 9},
        {13, 6, 19},
        {13, 7, 1},
        {13, 7, 2},
        {13, 7, 5},
        {13, 7, 6},
        {13, 7, 8},
        {13, 7, 9},
        {13, 7, 10},
        {13, 7, 11},
        {13, 7, 17},
        {13, 7, 19},
        {13, 7, 20},
        {13, 7, 21},
        {13, 7, 22},
        {13, 7, 23},
        {13, 7, 24},
        {13, 7, 26},
        {13, 7, 28},
        {13, 10, 1},
        {13, 10, 2},
        {13, 10, 5},
        {13, 10, 9},
        {13, 10, 17},
        {13, 10, 19},
        {13, 10, 21},
        {13, 10, 23},
        {13, 11, 1},
        {13, 11, 2},
        {13, 11, 3},
        {13, 11, 5},
        {13, 11, 9},
        {13, 11, 11},
        {13, 11, 12},
        {13, 11, 17},
        {13, 11, 19},
        {13, 11, 20},
        {13, 11, 21},
        {13, 11, 22},
        {13, 11, 23},
        {13, 11, 27},
        {13, 11, 28},
        {13, 11, 29},
        {13, 12, 1},
        {13, 12, 5},
        {13, 12, 9},
        {13, 12, 17},
        {13, 12, 19},
        {13, 12, 21},
        {13, 12, 22},
        {13, 13, 1},
        {13, 13, 2},
        {13, 13, 5},
        {13, 13, 9},
        {13, 13, 10},
        {13, 13, 11},
        {13, 13, 13},
        {13, 13, 16},
        {13, 13, 17},
        {13, 13, 19},
        {13, 13, 21},
        {13, 13, 23},
        {13, 13, 25},
        {13, 14, 1},
        {13, 14, 2},
        {13, 14, 5},
        {13, 14, 9},
        {13, 14, 17},
        {13, 14, 19},
        {13, 14, 21},
        {13, 14, 22},
        {13, 14, 23},
        {13, 15, 1},
        {13, 15, 2},
        {13, 15, 5},
        {13, 15, 17},
        {13, 15, 21},
        {13, 15, 23},
        {13, 18, 1},
        {13, 18, 2},
        {13, 18, 5},
        {13, 18, 9},
        {13, 18, 17},
        {13, 18, 19},
        {13, 18, 21},
        {13, 18, 23},
        {13, 19, 1},
        {13, 19, 2},
        {13, 19, 5},
        {13, 19, 9},
        {13, 19, 17},
        {13, 19, 19},
        {13, 19, 21},
        {13, 19, 23},
        {13, 20, 1},
        {13, 20, 2},
        {13, 20, 5},
        {13, 20, 9},
        {13, 20, 10},
        {13, 20, 11},
        {13, 20, 17},
        {13, 20, 19},
        {13, 20, 21},
        {13, 20, 23},
        {13, 21, 1},
        {13, 21, 2},
        {13, 21, 5},
        {13, 21, 9},
        {13, 21, 17},
        {13, 21, 19},
        {13, 21, 22},
        {13, 21, 23},
        {13, 22, 1},
        {13, 22, 2},
        {13, 22, 5},
        {13, 22, 9},
        {13, 22, 17},
        {13, 22, 19},
        {13, 22, 23},
        {13, 23, 1},
        {13, 23, 2},
        {13, 23, 5},
        {13, 23, 9},
        {13, 23, 17},
        {13, 23, 19},
        {13, 23, 21},
        {13, 23, 23},
        {13, 26, 1},
        {13, 26, 2},
        {13, 26, 5},
        {13, 26, 9},
        {13, 26, 17},
        {13, 26, 19},
        {13, 26, 21},
        {13, 26, 23},
        {13, 26, 25},
        {13, 27, 1},
        {13, 27, 2},
        {13, 27, 5},
        {13, 27, 9},
        {13, 27, 15},
        {13, 27, 17},
        {13, 27, 19},
        {13, 27, 21},
        {13, 27, 23},
        {13, 27, 24},
        {13, 27, 25},
        {13, 27, 26},
        {13, 27, 27},
        {13, 27, 28},
        {13, 27, 29},
        {13, 28, 1},
        {13, 28, 5},
        {13, 28, 9},
        {13, 28, 17},
        {13, 28, 21},
        {13, 29, 1},
        {13, 29, 2},
        {13, 29, 5},
        {13, 29, 9},
        {13, 29, 10},
        {13, 29, 11},
        {13, 29, 16},
        {13, 29, 17},
        {13, 29, 19},
        {13, 29, 21},
        {13, 29, 22},
        {13, 29, 23},
        {13, 29, 24},
        {13, 29, 28},
        {14, 3, 1},
        {14, 3, 2},
        {14, 3, 5},
        {14, 3, 7},
        {14, 3, 8},
        {14, 3, 9},
        {14, 3, 11},
        {14, 3, 17},
        {14, 3, 19},
        {14, 3, 21},
        {14, 3, 22},
        {14, 3, 23},
        {14, 3, 24},
        {14, 4, 1},
        {14, 4, 2},
        {14, 4, 5},
        {14, 4, 9},
        {14, 4, 17},
        {14, 4, 19},
        {14, 4, 21},
        {14, 4, 22},
        {14, 4, 23},
        {14, 5, 1},
        {14, 5, 2},
        {14, 5, 5},
        {14, 5, 7},
        {14, 5, 9},
        {14, 5, 17},
        {14, 5, 23},
        {14, 6, 1},
        {14, 6, 5},
        {14, 6, 9},
        {14, 7, 1},
        {14, 7, 2},
        {14, 7, 5},
        {14, 7, 9},
        {14, 7, 11},
        {14, 7, 17},
        {14, 7, 19},
        {14, 7, 21},
        {14, 7, 23},
        {14, 7, 24},
        {14, 10, 1},
        {14, 10, 2},
        {14, 10, 5},
        {14, 10, 9},
        {14, 10, 17},
        {14, 10, 19},
        {14, 10, 21},
        {14, 10, 23},
        {14, 11, 1},
        {14, 11, 5},
        {14, 11, 9},
        {14, 11, 17},
        {14, 11, 19},
        {14, 11, 22},
        {14, 11, 23},
        {14, 12, 1},
        {14, 13, 1},
        {14, 13, 2},
        {14, 13, 5},
        {14, 13, 9},
        {14, 13, 11},
        {14, 13, 17},
        {14, 13, 19},
        {14, 13, 21},
        {14, 13, 23},
        {14, 13, 24},
        {14, 13, 25},
        {14, 13, 29},
        {14, 14, 1},
        {14, 14, 2},
        {14, 14, 9},
        {14, 14, 19},
        {14, 14, 21},
        {14, 14, 23},
        {14, 15, 1},
        {14, 15, 22},
        {14, 15, 23},
        {14, 18, 1},
        {14, 18, 5},
        {14, 18, 9},
        {14, 18, 17},
        {14, 18, 19},
        {14, 18, 21},
        {14, 18, 23},
        {14, 19, 1},
        {14, 19, 2},
        {14, 19, 5},
        {14, 19, 23},
        {14, 20, 1},
        {14, 20, 2},
        {14, 20, 5},
        {14, 20, 9},
        {14, 20, 10},
        {14, 20, 11},
        {14, 20, 17},
        {14, 20, 19},
        {14, 20, 21},
        {14, 20, 23},
        {14, 21, 1},
        {14, 21, 22},
        {14, 22, 1},
        {14, 23, 1},
        {14, 23, 2},
        {14, 23, 5},
        {14, 23, 9},
        {14, 23, 17},
        {14, 23, 19},
        {14, 23, 21},
        {14, 26, 1},
        {14, 26, 5},
        {14, 26, 9},
        {14, 26, 17},
        {14, 27, 1},
        {14, 27, 2},
        {14, 27, 5},
        {14, 27, 9},
        {14, 27, 17},
        {14, 27, 19},
        {14, 27, 21},
        {14, 27, 23},
        {14, 29, 1},
        {14, 29, 2},
        {14, 29, 5},
        {14, 29, 8},
        {14, 29, 9},
        {14, 29, 11},
        {14, 29, 17},
        {14, 29, 19},
        {14, 29, 21},
        {14, 29, 23},
        {14, 29, 24},
        {14, 29, 27},
        {14, 29, 28},
        {15, 3, 1},
        {15, 3, 2},
        {15, 3, 5},
        {15, 3, 7},
        {15, 3, 9},
        {15, 3, 12},
        {15, 3, 17},
        {15, 3, 19},
        {15, 3, 21},
        {15, 3, 22},
        {15, 3, 23},
        {15, 4, 1},
        {15, 4, 2},
        {15, 4, 5},
        {15, 4, 9},
        {15, 4, 17},
        {15, 4, 19},
        {15, 4, 21},
        {15, 4, 22},
        {15, 4, 23},
        {15, 5, 1},
        {15, 5, 5},
        {15, 5, 23},
        {15, 7, 1},
        {15, 7, 2},
        {15, 7, 5},
        {15, 7, 9},
        {15, 7, 17},
        {15, 7, 19},
        {15, 7, 21},
        {15, 7, 22},
        {15, 7, 23},
        {15, 10, 1},
        {15, 10, 23},
        {15, 11, 1},
        {15, 11, 22},
        {15, 13, 1},
        {15, 13, 2},
        {15, 13, 5},
        {15, 13, 9},
        {15, 13, 17},
        {15, 13, 19},
        {15, 13, 21},
        {15, 13, 23},
        {15, 13, 25},
        {15, 14, 1},
        {15, 14, 2},
        {15, 14, 9},
        {15, 14, 22},
        {15, 15, 1},
        {15, 15, 22},
        {15, 18, 1},
        {15, 18, 5},
        {15, 18, 9},
        {15, 18, 17},
        {15, 18, 19},
        {15, 19, 23},
        {15, 20, 1},
        {15, 20, 2},
        {15, 20, 5},
        {15, 20, 9},
        {15, 20, 17},
        {15, 20, 19},
        {15, 20, 23},
        {15, 21, 1},
        {15, 21, 22},
        {15, 21, 23},
        {15, 23, 1},
        {15, 26, 1},
        {15, 27, 1},
        {15, 27, 17},
        {15, 27, 21},
        {15, 27, 23},
        {15, 29, 1},
        {15, 29, 2},
        {15, 29, 5},
        {15, 29, 9},
        {15, 29, 17},
        {15, 29, 19},
        {15, 29, 23},
        {15, 29, 24},
        {15, 29, 29},
        {16, 3, 1},
        {16, 3, 2},
        {16, 3, 5},
        {16, 3, 7},
        {16, 3, 9},
        {16, 3, 17},
        {16, 3, 19},
        {16, 3, 21},
        {16, 3, 22},
        {16, 3, 23},
        {16, 3, 24},
        {16, 4, 1},
        {16, 4, 2},
        {16, 4, 5},
        {16, 4, 9},
        {16, 4, 17},
        {16, 4, 19},
        {16, 4, 21},
        {16, 4, 22},
        {16, 4, 23},
        {16, 5, 1},
        {16, 5, 5},
        {16, 5, 7},
        {16, 5, 9},
        {16, 5, 17},
        {16, 5, 23},
        {16, 7, 1},
        {16, 7, 2},
        {16, 7, 5},
        {16, 7, 9},
        {16, 7, 17},
        {16, 7, 19},
        {16, 7, 21},
        {16, 7, 22},
        {16, 7, 23},
        {16, 10, 1},
        {16, 10, 2},
        {16, 10, 5},
        {16, 10, 9},
        {16, 10, 17},
        {16, 10, 19},
        {16, 10, 21},
        {16, 10, 23},
        {16, 11, 1},
        {16, 11, 5},
        {16, 11, 22},
        {16, 12, 1},
        {16, 12, 5},
        {16, 12, 23},
        {16, 13, 1},
        {16, 13, 2},
        {16, 13, 5},
        {16, 13, 9},
        {16, 13, 17},
        {16, 13, 19},
        {16, 13, 21},
        {16, 13, 23},
        {16, 14, 1},
        {16, 14, 5},
        {16, 14, 9},
        {16, 14, 23},
        {16, 18, 1},
        {16, 18, 5},
        {16, 18, 9},
        {16, 18, 17},
        {16, 18, 19},
        {16, 18, 21},
        {16, 18, 23},
        {16, 19, 1},
        {16, 19, 17},
        {16, 20, 1},
        {16, 20, 2},
        {16, 20, 5},
        {16, 20, 9},
        {16, 20, 17},
        {16, 20, 19},
        {16, 20, 21},
        {16, 20, 23},
        {16, 21, 1},
        {16, 21, 22},
        {16, 22, 1},
        {16, 22, 5},
        {16, 23, 1},
        {16, 23, 5},
        {16, 23, 9},
        {16, 23, 17},
        {16, 23, 19},
        {16, 23, 21},
        {16, 23, 23},
        {16, 26, 1},
        {16, 26, 5},
        {16, 26, 9},
        {16, 26, 17},
        {16, 26, 23},
        {16, 27, 1},
        {16, 27, 2},
        {16, 27, 5},
        {16, 27, 9},
        {16, 27, 17},
        {16, 27, 19},
        {16, 27, 21},
        {16, 27, 23},
        {16, 29, 1},
        {16, 29, 2},
        {16, 29, 5},
        {16, 29, 8},
        {16, 29, 9},
        {16, 29, 10},
        {16, 29, 17},
        {16, 29, 19},
        {16, 29, 21},
        {16, 29, 23},
        {17, 3, 1},
        {17, 3, 2},
        {17, 3, 5},
        {17, 3, 9},
        {17, 3, 17},
        {17, 3, 19},
        {17, 3, 21},
        {17, 3, 23},
        {17, 4, 1},
        {17, 4, 2},
        {17, 4, 5},
        {17, 4, 9},
        {17, 4, 17},
        {17, 4, 19},
        {17, 4, 21},
        {17, 4, 22},
        {17, 4, 23},
        {17, 5, 1},
        {17, 5, 2},
        {17, 5, 23},
        {17, 7, 1},
        {17, 7, 2},
        {17, 7, 5},
        {17, 7, 8},
        {17, 7, 9},
        {17, 7, 17},
        {17, 7, 19},
        {17, 7, 21},
        {17, 7, 22},
        {17, 7, 23},
        {17, 10, 1},
        {17, 10, 2},
        {17, 10, 5},
        {17, 10, 9},
        {17, 10, 17},
        {17, 10, 19},
        {17, 10, 21},
        {17, 10, 23},
        {17, 11, 1},
        {17, 11, 5},
        {17, 11, 9},
        {17, 11, 17},
        {17, 11, 19},
        {17, 11, 21},
        {17, 11, 22},
        {17, 11, 23},
        {17, 12, 1},
        {17, 13, 1},
        {17, 13, 2},
        {17, 13, 5},
        {17, 13, 9},
        {17, 13, 17},
        {17, 13, 19},
        {17, 13, 21},
        {17, 13, 23},
        {17, 14, 1},
        {17, 14, 2},
        {17, 14, 5},
        {17, 14, 9},
        {17, 14, 17},
        {17, 14, 23},
        {17, 15, 1},
        {17, 15, 23},
        {17, 18, 1},
        {17, 18, 9},
        {17, 19, 1},
        {17, 20, 1},
        {17, 20, 2},
        {17, 20, 5},
        {17, 20, 9},
        {17, 20, 17},
        {17, 20, 19},
        {17, 20, 21},
        {17, 20, 23},
        {17, 21, 1},
        {17, 21, 5},
        {17, 21, 9},
        {17, 21, 23},
        {17, 22, 1},
        {17, 22, 23},
        {17, 23, 1},
        {17, 23, 2},
        {17, 23, 5},
        {17, 23, 9},
        {17, 23, 17},
        {17, 23, 19},
        {17, 23, 21},
        {17, 23, 23},
        {17, 26, 1},
        {17, 26, 5},
        {17, 26, 9},
        {17, 26, 17},
        {17, 27, 1},
        {17, 27, 2},
        {17, 27, 5},
        {17, 27, 9},
        {17, 27, 17},
        {17, 27, 19},
        {17, 27, 23},
        {17, 29, 1},
        {17, 29, 2},
        {17, 29, 5},
        {17, 29, 9},
        {17, 29, 17},
        {17, 29, 19},
        {17, 29, 21},
        {17, 29, 23},
        {18, 3, 1},
        {18, 3, 2},
        {18, 3, 5},
        {18, 3, 9},
        {18, 3, 10},
        {18, 3, 17},
        {18, 3, 19},
        {18, 3, 21},
        {18, 3, 22},
        {18, 3, 23},
        {18, 4, 1},
        {18, 4, 2},
        {18, 4, 5},
        {18, 4, 9},
        {18, 4, 17},
        {18, 4, 19},
        {18, 4, 21},
        {18, 4, 22},
        {18, 4, 23},
        {18, 5, 1},
        {18, 5, 23},
        {18, 7, 1},
        {18, 7, 2},
        {18, 7, 5},
        {18, 7, 9},
        {18, 7, 11},
        {18, 7, 17},
        {18, 7, 19},
        {18, 7, 21},
        {18, 7, 22},
        {18, 7, 23},
        {18, 10, 1},
        {18, 10, 2},
        {18, 10, 5},
        {18, 10, 9},
        {18, 10, 17},
        {18, 10, 19},
        {18, 10, 21},
        {18, 10, 23},
        {18, 11, 1},
        {18, 11, 5},
        {18, 11, 22},
        {18, 12, 1},
        {18, 12, 5},
        {18, 13, 1},
        {18, 13, 2},
        {18, 13, 5},
        {18, 13, 9},
        {18, 13, 17},
        {18, 13, 19},
        {18, 13, 21},
        {18, 13, 23},
        {18, 13, 28},
        {18, 14, 1},
        {18, 14, 5},
        {18, 15, 1},
        {18, 18, 1},
        {18, 18, 5},
        {18, 18, 21},
        {18, 18, 23},
        {18, 19, 1},
        {18, 20, 1},
        {18, 20, 2},
        {18, 20, 5},
        {18, 20, 9},
        {18, 20, 17},
        {18, 20, 19},
        {18, 20, 21},
        {18, 20, 23},
        {18, 21, 1},
        {18, 21, 22},
        {18, 22, 1},
        {18, 23, 1},
        {18, 23, 2},
        {18, 23, 5},
        {18, 23, 9},
        {18, 23, 17},
        {18, 23, 19},
        {18, 23, 23},
        {18, 26, 1},
        {18, 26, 5},
        {18, 26, 9},
        {18, 26, 17},
        {18, 26, 23},
        {18, 27, 1},
        {18, 27, 2},
        {18, 27, 5},
        {18, 27, 8},
        {18, 27, 9},
        {18, 27, 11},
        {18, 27, 17},
        {18, 27, 19},
        {18, 27, 21},
        {18, 28, 1},
        {18, 28, 5},
        {18, 28, 9},
        {18, 28, 17},
        {18, 28, 19},
        {18, 29, 1},
        {18, 29, 2},
        {18, 29, 5},
        {18, 29, 9},
        {18, 29, 17},
        {18, 29, 19},
        {18, 29, 21},
        {18, 29, 23},
        {19, 3, 1},
        {19, 3, 2},
        {19, 3, 3},
        {19, 3, 5},
        {19, 3, 9},
        {19, 3, 11},
        {19, 3, 17},
        {19, 3, 19},
        {19, 3, 21},
        {19, 3, 22},
        {19, 3, 23},
        {19, 3, 27},
        {19, 4, 1},
        {19, 4, 2},
        {19, 4, 5},
        {19, 4, 9},
        {19, 4, 17},
        {19, 4, 19},
        {19, 4, 21},
        {19, 4, 22},
        {19, 4, 23},
        {19, 5, 1},
        {19, 5, 2},
        {19, 7, 1},
        {19, 7, 2},
        {19, 7, 5},
        {19, 7, 9},
        {19, 7, 17},
        {19, 7, 19},
        {19, 7, 21},
        {19, 7, 22},
        {19, 7, 23},
        {19, 10, 1},
        {19, 10, 2},
        {19, 10, 5},
        {19, 10, 9},
        {19, 10, 17},
        {19, 10, 19},
        {19, 10, 21},
        {19, 10, 23},
        {19, 11, 1},
        {19, 11, 5},
        {19, 11, 9},
        {19, 11, 17},
        {19, 11, 19},
        {19, 11, 22},
        {19, 11, 23},
        {19, 12, 1},
        {19, 12, 9},
        {19, 12, 19},
        {19, 12, 21},
        {19, 13, 1},
        {19, 13, 2},
        {19, 13, 5},
        {19, 13, 9},
        {19, 13, 17},
        {19, 13, 19},
        {19, 13, 21},
        {19, 13, 23},
        {19, 14, 1},
        {19, 14, 23},
        {19, 18, 1},
        {19, 18, 5},
        {19, 19, 1},
        {19, 19, 5},
        {19, 19, 9},
        {19, 19, 19},
        {19, 19, 21},
        {19, 20, 1},
        {19, 20, 2},
        {19, 20, 5},
        {19, 20, 8},
        {19, 20, 9},
        {19, 20, 11},
        {19, 20, 17},
        {19, 20, 19},
        {19, 20, 21},
        {19, 20, 23},
        {19, 21, 1},
        {19, 21, 23},
        {19, 23, 1},
        {19, 23, 5},
        {19, 23, 9},
        {19, 23, 17},
        {19, 23, 21},
        {19, 26, 1},
        {19, 26, 5},
        {19, 26, 9},
        {19, 26, 17},
        {19, 26, 21},
        {19, 26, 23},
        {19, 27, 1},
        {19, 27, 5},
        {19, 27, 9},
        {19, 27, 17},
        {19, 27, 19},
        {19, 27, 21},
        {19, 29, 1},
        {19, 29, 2},
        {19, 29, 5},
        {19, 29, 9},
        {19, 29, 17},
        {19, 29, 19},
        {19, 29, 21},
        {19, 29, 23},
        {20, 3, 1},
        {20, 3, 2},
        {20, 3, 5},
        {20, 3, 9},
        {20, 3, 14},
        {20, 3, 17},
        {20, 3, 19},
        {20, 3, 21},
        {20, 3, 23},
        {20, 4, 1},
        {20, 4, 2},
        {20, 4, 5},
        {20, 4, 9},
        {20, 4, 17},
        {20, 4, 19},
        {20, 4, 21},
        {20, 4, 22},
        {20, 4, 23},
        {20, 5, 1},
        {20, 5, 23},
        {20, 7, 1},
        {20, 7, 2},
        {20, 7, 5},
        {20, 7, 9},
        {20, 7, 11},
        {20, 7, 17},
        {20, 7, 19},
        {20, 7, 21},
        {20, 7, 23},
        {20, 10, 1},
        {20, 10, 2},
        {20, 10, 5},
        {20, 10, 9},
        {20, 10, 17},
        {20, 10, 19},
        {20, 10, 21},
        {20, 10, 23},
        {20, 11, 1},
        {20, 11, 2},
        {20, 11, 5},
        {20, 11, 9},
        {20, 11, 17},
        {20, 11, 19},
        {20, 11, 21},
        {20, 11, 22},
        {20, 11, 23},
        {20, 12, 1},
        {20, 12, 5},
        {20, 12, 9},
        {20, 12, 19},
        {20, 13, 1},
        {20, 13, 2},
        {20, 13, 5},
        {20, 13, 9},
        {20, 13, 14},
        {20, 13, 17},
        {20, 13, 19},
        {20, 13, 21},
        {20, 13, 23},
        {20, 13, 27},
        {20, 14, 1},
        {20, 14, 2},
        {20, 14, 5},
        {20, 14, 9},
        {20, 14, 21},
        {20, 14, 23},
        {20, 15, 1},
        {20, 15, 2},
        {20, 15, 5},
        {20, 15, 21},
        {20, 15, 23},
        {20, 18, 1},
        {20, 18, 2},
        {20, 18, 5},
        {20, 18, 9},
        {20, 18, 19},
        {20, 18, 21},
        {20, 18, 23},
        {20, 19, 1},
        {20, 19, 5},
        {20, 19, 9},
        {20, 19, 19},
        {20, 19, 21},
        {20, 20, 1},
        {20, 20, 2},
        {20, 20, 5},
        {20, 20, 9},
        {20, 20, 14},
        {20, 20, 17},
        {20, 20, 21},
        {20, 20, 23},
        {20, 21, 1},
        {20, 21, 5},
        {20, 21, 9},
        {20, 21, 17},
        {20, 21, 23},
        {20, 22, 1},
        {20, 22, 2},
        {20, 22, 5},
        {20, 22, 9},
        {20, 22, 23},
        {20, 23, 1},
        {20, 23, 2},
        {20, 23, 5},
        {20, 23, 9},
        {20, 23, 17},
        {20, 23, 19},
        {20, 23, 21},
        {20, 23, 23},
        {20, 26, 1},
        {20, 26, 2},
        {20, 26, 5},
        {20, 26, 9},
        {20, 26, 17},
        {20, 26, 21},
        {20, 26, 23},
        {20, 27, 1},
        {20, 27, 2},
        {20, 27, 5},
        {20, 27, 7},
        {20, 27, 8},
        {20, 27, 9},
        {20, 27, 10},
        {20, 27, 17},
        {20, 27, 19},
        {20, 27, 21},
        {20, 27, 23},
        {20, 27, 27},
        {20, 28, 1},
        {20, 28, 5},
        {20, 28, 9},
        {20, 28, 17},
        {20, 28, 19},
        {20, 28, 23},
        {20, 29, 1},
        {20, 29, 2},
        {20, 29, 5},
        {20, 29, 9},
        {20, 29, 17},
        {20, 29, 19},
        {20, 29, 21},
        {20, 29, 23},
};

static const unsigned short ks_table2[][4] =
    {
        {0xa4bf, 1, 3, 1},
        {0xa4c0, 1, 4, 1},
        {0xa4c1, 1, 5, 1},
        {0xa4c2, 1, 6, 1},
        {0xa4c3, 1, 7, 1},
        {0xa4c4, 1, 10, 1},
        {0xa4c5, 1, 11, 1},
        {0xa4c6, 1, 12, 1},
        {0xa4c7, 1, 13, 1},
        {0xa4c8, 1, 14, 1},
        {0xa4c9, 1, 15, 1},
        {0xa4ca, 1, 18, 1},
        {0xa4cb, 1, 19, 1},
        {0xa4cc, 1, 20, 1},
        {0xa4cd, 1, 21, 1},
        {0xa4ce, 1, 22, 1},
        {0xa4cf, 1, 23, 1},
        {0xa4d0, 1, 26, 1},
        {0xa4d1, 1, 27, 1},
        {0xa4d2, 1, 28, 1},
        {0xa4d3, 1, 29, 1},
        {0xa4a1, 2, 2, 1},
        {0xa4a2, 3, 2, 1},
        {0xa4a4, 4, 2, 1},
        {0xa4a7, 5, 2, 1},
        {0xa4a8, 6, 2, 1},
        {0xa4a9, 7, 2, 1},
        {0xa4b1, 8, 2, 1},
        {0xa4b2, 9, 2, 1},
        {0xa4b3, 10, 2, 1},
        {0xa4b5, 11, 2, 1},
        {0xa4b6, 12, 2, 1},
        {0xa4b7, 13, 2, 1},
        {0xa4b8, 14, 2, 1},
        {0xa4b9, 15, 2, 1},
        {0xa4ba, 16, 2, 1},
        {0xa4bb, 17, 2, 1},
        {0xa4bc, 18, 2, 1},
        {0xa4bd, 19, 2, 1},
        {0xa4be, 20, 2, 1},
};

/* 조합형 초성 - 완성형 낱자 변환
 * conversion: initial sound of compound type - ??? of completion type
 */

static const char_u johab_fcon_to_wan[] =
    {
        0,
        0xd4, 0xa1, 0xa2, 0xa4, 0xa7, /* (채움),ㄱ,ㄲ,ㄴ,ㄷ */
        0xa8, 0xa9, 0xb1, 0xb2, 0xb3, /* ㄸ,ㄹ,ㅁ,ㅂ,ㅃ */
        0xb5, 0xb6, 0xb7, 0xb8, 0xb9, /* ㅅ,ㅆ,ㅇ,ㅈ,ㅉ */
        0xba, 0xbb, 0xbc, 0xbd, 0xbe  /* ㅊ,ㅋ,ㅌ,ㅍ,ㅎ */
};

/* 조합형 중성 -> 완성형 낱자 변환
 * conversion: medial vowel of compound type - ??? of completion type
 */

static const char_u johab_vow_to_wan[] =
    {
        0, 0,
        0xd4, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, /* (채움),ㅏ,ㅐ,ㅑ,ㅒ,ㅓ */
        0, 0,
        0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, /* ㅔ,ㅕ,ㅖ,ㅗ,ㅗㅏ,ㅗㅐ */
        0, 0,
        0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, /* ㅗㅣ,ㅛ,ㅜ,ㅜㅓ,ㅜㅔ,ㅜㅣ */
        0, 0,
        0xd0, 0xd1, 0xd2, 0xd3 /* ㅠ,ㅡ,ㅡㅣ,ㅣ */
};

/* 조합형 종성 -> 완성형 낱자 변환
 * conversion: final consonant of compound type - ??? of completion type
 */

static const char_u johab_lcon_to_wan[] =
    {
        0,
        0xd4, 0xa1, 0xa2, 0xa3, 0xa4, /* (채움), ㄱ, ㄲ, ㄱㅅ, ㄴ */
        0xa5, 0xa6, 0xa7, 0xa9, 0xaa, /* ㄴㅈ, ㄴㅎ, ㄷ, ㄹ, ㄹㄱ */
        0xab, 0xac, 0xad, 0xae, 0xaf, /* ㄹㅁ, ㄹㅂ, ㄹㅅ, ㄹㅌ, ㄹㅍ */
        0xb0, 0xb1, 0, 0xb2, 0xb4,    /* ㄹㅎ, ㅁ, 0, ㅂ, ㅂㅅ */
        0xb5, 0xb6, 0xb7, 0xb8, 0xba, /* ㅅ, ㅆ, ㅇ, ㅈ, ㅊ */
        0xbb, 0xbc, 0xbd, 0xbe        /* ㅋ, ㅌ, ㅍ, ㅎ */
};

static void
convert_ks_to_3(
    const char_u *src,
    int *fp,
    int *mp,
    int *lp) {
  int h = *src;
  int low = *(src + 1);
  int c;
  int i;

  if ((i = han_index(h, low)) >= 0 && i < (int)(sizeof(ks_table1) / sizeof(ks_table1[0]))) {
    *fp = ks_table1[i][0];
    *mp = ks_table1[i][1];
    *lp = ks_table1[i][2];
  } else {
    c = (h << 8) | low;
    for (i = 0; i < 40; i++)
      if (ks_table2[i][0] == c) {
        *fp = ks_table2[i][1];
        *mp = ks_table2[i][2];
        *lp = ks_table2[i][3];
        return;
      }
    *fp = 0xff; /* 그래픽 코드 (graphic code) */
    *mp = h;
    *lp = low;
  }
}

static int
convert_3_to_ks(
    int fv,
    int mv,
    int lv,
    char_u *des) {
  char_u key[3];
  register int hi, lo, mi = 0, result, found;

  if (fv == 0xff) {
    des[0] = mv;
    des[1] = lv;
    return 2;
  }
  key[0] = fv;
  key[1] = mv;
  key[2] = lv;
  lo = 0;
  hi = sizeof(ks_table1) / 3 - 1;
  found = 0;
  while (lo + 1 < hi) {
    mi = (lo + hi) / 2;
    result = STRNCMP(ks_table1[mi], key, 3);
    if (result == 0) {
      found = 1;
      break;
    } else if (result > 0)
      hi = mi;
    else
      lo = mi;
  }
  if (!found) {
    if (!STRNCMP(ks_table1[lo], key, 3)) {
      found = 1;
      mi = lo;
    }
    if (!STRNCMP(ks_table1[hi], key, 3)) {
      found = 1;
      mi = hi;
    }
  }
  if (!found) {
    for (mi = 0; mi < 40; mi++)
      if (ks_table2[mi][1] == fv && ks_table2[mi][2] == mv &&
          ks_table2[mi][3] == lv) {
        des[0] = (unsigned)(ks_table2[mi][0]) >> 8;
        des[1] = ks_table2[mi][0];
        return 2; /* found */
      }
  } else {
    des[0] = mi / (0xff - 0xa1) + 0xb0;
    des[1] = mi % (0xff - 0xa1) + 0xa1;
    return 2; /* found */
  }

  /* 완성형 표에 없다. ``KS C 5601 - 1992 정보 교환용 부호 해설''
     * 3.3 절에 설명된 방법으로 encoding 한다.
     */

  *des++ = 0xa4; /* 채움 */
  *des++ = 0xd4;
  *des++ = 0xa4; /* 낱자는 모두 a4 행에 있다. */
  *des++ = johab_fcon_to_wan[fv];
  *des++ = 0xa4;
  *des++ = johab_vow_to_wan[mv];
  *des++ = 0xa4;
  *des++ = johab_lcon_to_wan[lv];
  return 8;
}

char_u *
hangul_string_convert(char_u *buf, int *p_len) {
  char_u *tmpbuf = NULL;
  vimconv_T vc;

  if (enc_utf8) {
    vc.vc_type = CONV_NONE;
    if (convert_setup(&vc, (char_u *)"euc-kr", p_enc) == OK) {
      tmpbuf = string_convert(&vc, buf, p_len);
      convert_setup(&vc, NULL, NULL);
    }
  }

  return tmpbuf;
}

char_u *
hangul_composing_buffer_get(int *p_len) {
  char_u *tmpbuf = NULL;

  if (composing_hangul) {
    int len = 2;

    tmpbuf = hangul_string_convert(composing_hangul_buffer, &len);
    if (tmpbuf != NULL) {
      *p_len = len;
    } else {
      tmpbuf = vim_strnsave(composing_hangul_buffer, 2);
      *p_len = 2;
    }
  }

  return tmpbuf;
}
