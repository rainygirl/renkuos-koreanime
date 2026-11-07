/*
 * Copyright 2026 rainygirl. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Types key sequences into the composer the way the input method does and
 * checks the text that results. Builds with any C++ compiler:
 *   c++ -Isrc -o hangul_test test/HangulTest.cpp src/Hangul.cpp && ./hangul_test
 */
#include "Hangul.h"

#include <stdio.h>
#include <string.h>

using namespace koreanime;


// QWERTY letters to RenkuOS key codes; '<' is Backspace.
static unsigned
KeyFor(char c, bool* shift)
{
	static const char* kRows[3] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
	static const unsigned kStart[3] = { 0x27, 0x3c, 0x4c };
	*shift = c >= 'A' && c <= 'Z';
	if (*shift)
		c = c - 'A' + 'a';
	for (int row = 0; row < 3; row++) {
		const char* at = strchr(kRows[row], c);
		if (at != NULL)
			return kStart[row] + (unsigned)(at - kRows[row]);
	}
	return 0;
}


static void
Append(char* text, unsigned codePoint)
{
	if (codePoint == 0)
		return;
	char buffer[5];
	EncodeUTF8(codePoint, buffer);
	strcat(text, buffer);
}


// What the user sees: committed text followed by the preedit.
static void
Type(const char* keys, char* text)
{
	Composer composer;
	text[0] = '\0';
	for (const char* k = keys; *k != '\0'; k++) {
		if (*k == '<') {
			if (!composer.Backspace()) {
				// the method passes Backspace on: delete a committed char
				size_t length = strlen(text);
				while (length > 0 && (text[length - 1] & 0xC0) == 0x80)
					length--;
				if (length > 0)
					text[length - 1] = '\0';
			}
			continue;
		}
		if (*k == ' ') {
			Append(text, composer.Flush());
			strcat(text, " ");
			continue;
		}
		bool shift;
		unsigned key = KeyFor(*k, &shift);
		Append(text, composer.Input(JamoForKey(key, shift)));
	}
	Append(text, composer.Preedit());
}


static int sFailures = 0;


static void
Expect(const char* keys, const char* expected)
{
	char text[256];
	Type(keys, text);
	if (strcmp(text, expected) != 0) {
		printf("FAIL  %-16s -> %s (expected %s)\n", keys, text, expected);
		sFailures++;
	} else
		printf("ok    %-16s -> %s\n", keys, text);
}


int
main()
{
	Expect("gksrmf", "한글");
	Expect("dkssudgktpdy", "안녕하세요");
	Expect("rkrk", "가가");				// a final moves on before a vowel
	Expect("rkqt", "값");				// compound final
	Expect("rkqtl", "갑시");			// and splits before a vowel
	Expect("rkqtdl", "값이");
	Expect("ekfr", "닭");
	Expect("ekfrl", "달기");
	Expect("ekfrdl", "닭이");
	Expect("dhk", "와");				// compound vowel
	Expect("dhkd", "왕");
	Expect("dml", "의");
	Expect("Rk", "까");				// Shift for doubled consonants
	Expect("Qhfk", "뽀라");
	Expect("dO", "얘");
	Expect("rr", "ㄱㄱ");				// two initials do not merge
	Expect("k", "ㅏ");
	Expect("kk", "ㅏㅏ");
	Expect("rkTek", "갔다");
	Expect("rkEk", "가따");			// ㄸ cannot be a final
	Expect("gks<", "하");				// Backspace takes one jamo back
	Expect("gks<<", "ㅎ");
	Expect("gks<<<", "");
	Expect("dhk<", "오");
	Expect("rkqt<", "갑");
	Expect("gksrmf<<<", "한");
	Expect("gksrmf<<<<", "");			// a committed syllable goes whole
	Expect("rkrk<<", "가");
	Expect("gks rmf", "한 글");
	Expect("Tm", "쓰");
	Expect("whdgkqtl", "종합시");
	Expect("dlqfur", "입력");
	Expect("dkswjsgks", "안전한");
	Expect("rnjs", "권");
	Expect("dnpq", "웹");
	Expect("rmfwk", "글자");

	if (sFailures > 0) {
		printf("%d failed\n", sFailures);
		return 1;
	}
	printf("all passed\n");
	return 0;
}
