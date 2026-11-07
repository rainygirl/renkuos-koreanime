/*
 * Copyright 2026 rainygirl. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#include "Hangul.h"


namespace koreanime {


static const unsigned kSyllableBase = 0xAC00;
static const unsigned kFirstVowel = 0x314F;	// ㅏ; ㅏ-ㅣ are in jungseong order

// Choseong in syllable order, as compatibility jamo.
static const unsigned kInitials[19] = {
	0x3131, 0x3132, 0x3134, 0x3137, 0x3138, 0x3139, 0x3141, 0x3142, 0x3143,
	0x3145, 0x3146, 0x3147, 0x3148, 0x3149, 0x314A, 0x314B, 0x314C, 0x314D,
	0x314E
};

// Jongseong 1-27 in syllable order (0 is "none").
static const unsigned kFinals[28] = {
	0,
	0x3131, 0x3132, 0x3133, 0x3134, 0x3135, 0x3136, 0x3137, 0x3139, 0x313A,
	0x313B, 0x313C, 0x313D, 0x313E, 0x313F, 0x3140, 0x3141, 0x3142, 0x3144,
	0x3145, 0x3146, 0x3147, 0x3148, 0x314A, 0x314B, 0x314C, 0x314D, 0x314E
};

// Two finals that make one, and how a vowel splits them again.
struct FinalPair {
	int		first;		// simple final
	unsigned second;	// consonant typed after it
	int		compound;
};

static const FinalPair kFinalPairs[] = {
	{ 1, 0x3145, 3 },	// ㄱㅅ ㄳ
	{ 4, 0x3148, 5 },	// ㄴㅈ ㄵ
	{ 4, 0x314E, 6 },	// ㄴㅎ ㄶ
	{ 8, 0x3131, 9 },	// ㄹㄱ ㄺ
	{ 8, 0x3141, 10 },	// ㄹㅁ ㄻ
	{ 8, 0x3142, 11 },	// ㄹㅂ ㄼ
	{ 8, 0x3145, 12 },	// ㄹㅅ ㄽ
	{ 8, 0x314C, 13 },	// ㄹㅌ ㄾ
	{ 8, 0x314D, 14 },	// ㄹㅍ ㄿ
	{ 8, 0x314E, 15 },	// ㄹㅎ ㅀ
	{ 17, 0x3145, 18 }	// ㅂㅅ ㅄ
};
static const int kFinalPairCount = sizeof(kFinalPairs) / sizeof(kFinalPairs[0]);

// Two vowels that make one (jungseong indices).
struct MedialPair {
	int		first;
	int		second;
	int		compound;
};

static const MedialPair kMedialPairs[] = {
	{ 8, 0, 9 },		// ㅗㅏ ㅘ
	{ 8, 1, 10 },		// ㅗㅐ ㅙ
	{ 8, 20, 11 },		// ㅗㅣ ㅚ
	{ 13, 4, 14 },		// ㅜㅓ ㅝ
	{ 13, 5, 15 },		// ㅜㅔ ㅞ
	{ 13, 20, 16 },		// ㅜㅣ ㅟ
	{ 18, 20, 19 }		// ㅡㅣ ㅢ
};
static const int kMedialPairCount
	= sizeof(kMedialPairs) / sizeof(kMedialPairs[0]);


// The 2-beolsik layout on RenkuOS key codes (the letter keys of a US
// keyboard, whatever keymap is active): plain and with Shift.
struct KeyJamo {
	unsigned	key;
	unsigned	plain;
	unsigned	shifted;
};

static const KeyJamo kLayout[] = {
	{ 0x27, 0x3142, 0x3143 },	// q ㅂ ㅃ
	{ 0x28, 0x3148, 0x3149 },	// w ㅈ ㅉ
	{ 0x29, 0x3137, 0x3138 },	// e ㄷ ㄸ
	{ 0x2a, 0x3131, 0x3132 },	// r ㄱ ㄲ
	{ 0x2b, 0x3145, 0x3146 },	// t ㅅ ㅆ
	{ 0x2c, 0x315B, 0x315B },	// y ㅛ
	{ 0x2d, 0x3155, 0x3155 },	// u ㅕ
	{ 0x2e, 0x3151, 0x3151 },	// i ㅑ
	{ 0x2f, 0x3150, 0x3152 },	// o ㅐ ㅒ
	{ 0x30, 0x3154, 0x3156 },	// p ㅔ ㅖ
	{ 0x3c, 0x3141, 0x3141 },	// a ㅁ
	{ 0x3d, 0x3134, 0x3134 },	// s ㄴ
	{ 0x3e, 0x3147, 0x3147 },	// d ㅇ
	{ 0x3f, 0x3139, 0x3139 },	// f ㄹ
	{ 0x40, 0x314E, 0x314E },	// g ㅎ
	{ 0x41, 0x3157, 0x3157 },	// h ㅗ
	{ 0x42, 0x3153, 0x3153 },	// j ㅓ
	{ 0x43, 0x314F, 0x314F },	// k ㅏ
	{ 0x44, 0x3163, 0x3163 },	// l ㅣ
	{ 0x4c, 0x314B, 0x314B },	// z ㅋ
	{ 0x4d, 0x314C, 0x314C },	// x ㅌ
	{ 0x4e, 0x314A, 0x314A },	// c ㅊ
	{ 0x4f, 0x314D, 0x314D },	// v ㅍ
	{ 0x50, 0x3160, 0x3160 },	// b ㅠ
	{ 0x51, 0x315C, 0x315C },	// n ㅜ
	{ 0x52, 0x3161, 0x3161 }	// m ㅡ
};
static const int kLayoutCount = sizeof(kLayout) / sizeof(kLayout[0]);


unsigned
JamoForKey(unsigned key, bool shift)
{
	for (int i = 0; i < kLayoutCount; i++) {
		if (kLayout[i].key == key)
			return shift ? kLayout[i].shifted : kLayout[i].plain;
	}
	return 0;
}


bool
IsConsonant(unsigned jamo)
{
	return jamo >= 0x3131 && jamo <= 0x314E;
}


bool
IsVowel(unsigned jamo)
{
	return jamo >= kFirstVowel && jamo <= 0x3163;
}


int
EncodeUTF8(unsigned codePoint, char* buffer)
{
	int length;
	if (codePoint < 0x80) {
		buffer[0] = (char)codePoint;
		length = 1;
	} else if (codePoint < 0x800) {
		buffer[0] = (char)(0xC0 | (codePoint >> 6));
		buffer[1] = (char)(0x80 | (codePoint & 0x3F));
		length = 2;
	} else if (codePoint < 0x10000) {
		buffer[0] = (char)(0xE0 | (codePoint >> 12));
		buffer[1] = (char)(0x80 | ((codePoint >> 6) & 0x3F));
		buffer[2] = (char)(0x80 | (codePoint & 0x3F));
		length = 3;
	} else {
		buffer[0] = (char)(0xF0 | (codePoint >> 18));
		buffer[1] = (char)(0x80 | ((codePoint >> 12) & 0x3F));
		buffer[2] = (char)(0x80 | ((codePoint >> 6) & 0x3F));
		buffer[3] = (char)(0x80 | (codePoint & 0x3F));
		length = 4;
	}
	buffer[length] = '\0';
	return length;
}


static int
InitialIndex(unsigned jamo)
{
	for (int i = 0; i < 19; i++) {
		if (kInitials[i] == jamo)
			return i;
	}
	return -1;
}


static int
FinalIndex(unsigned jamo)
{
	for (int i = 1; i < 28; i++) {
		if (kFinals[i] == jamo)
			return i;
	}
	return 0;
}


// #pragma mark - Composer


Composer::Composer()
{
	Reset();
}


void
Composer::Reset()
{
	fState.initial = -1;
	fState.medial = -1;
	fState.final = 0;
	fHistoryCount = 0;
}


bool
Composer::IsEmpty() const
{
	return fState.initial < 0 && fState.medial < 0;
}


unsigned
Composer::Preedit() const
{
	if (fState.initial >= 0 && fState.medial >= 0) {
		return kSyllableBase
			+ (fState.initial * 21 + fState.medial) * 28 + fState.final;
	}
	if (fState.initial >= 0)
		return kInitials[fState.initial];
	if (fState.medial >= 0)
		return kFirstVowel + fState.medial;
	return 0;
}


unsigned
Composer::Flush()
{
	unsigned syllable = Preedit();
	Reset();
	return syllable;
}


void
Composer::_Push()
{
	if (fHistoryCount == (int)(sizeof(fHistory) / sizeof(fHistory[0]))) {
		for (int i = 1; i < fHistoryCount; i++)
			fHistory[i - 1] = fHistory[i];
		fHistoryCount--;
	}
	fHistory[fHistoryCount++] = fState;
}


// Begins a new syllable: Backspace then walks back through `initial` alone
// to nothing.
void
Composer::_Start(int initial, int medial)
{
	Reset();
	_Push();
	if (initial >= 0 && medial >= 0) {
		fState.initial = initial;
		_Push();
		fState.medial = medial;
		return;
	}
	fState.initial = initial;
	fState.medial = medial;
}


unsigned
Composer::Input(unsigned jamo)
{
	if (IsConsonant(jamo)) {
		if (IsEmpty()) {
			_Push();
			fState.initial = InitialIndex(jamo);
			if (fState.initial < 0) {
				// ㄳ and the like cannot begin a syllable; stand alone
				fState.initial = -1;
				fHistoryCount = 0;
				return jamo;
			}
			return 0;
		}

		if (fState.initial < 0 || fState.medial < 0) {
			// a lone consonant or a lone vowel: it is done
			unsigned done = Flush();
			_Start(InitialIndex(jamo), -1);
			return done;
		}

		if (fState.final == 0) {
			int final = FinalIndex(jamo);
			if (final > 0) {
				_Push();
				fState.final = final;
				return 0;
			}
		} else {
			for (int i = 0; i < kFinalPairCount; i++) {
				if (kFinalPairs[i].first == fState.final
					&& kFinalPairs[i].second == jamo) {
					_Push();
					fState.final = kFinalPairs[i].compound;
					return 0;
				}
			}
		}

		unsigned done = Flush();
		_Start(InitialIndex(jamo), -1);
		return done;
	}

	if (!IsVowel(jamo))
		return 0;

	int medial = (int)(jamo - kFirstVowel);

	if (IsEmpty() || fState.medial < 0) {
		_Push();
		fState.medial = medial;
		return 0;
	}

	if (fState.final == 0) {
		for (int i = 0; i < kMedialPairCount; i++) {
			if (kMedialPairs[i].first == fState.medial
				&& kMedialPairs[i].second == medial) {
				_Push();
				fState.medial = kMedialPairs[i].compound;
				return 0;
			}
		}
		unsigned done = Flush();
		_Start(-1, medial);
		return done;
	}

	// A vowel after a final: the final (or the second half of a compound
	// one) becomes the next syllable's initial. 각 + ㅏ -> 가가, 갃 + ㅏ -> 각사.
	int keep = 0;
	unsigned moved = kFinals[fState.final];
	for (int i = 0; i < kFinalPairCount; i++) {
		if (kFinalPairs[i].compound == fState.final) {
			keep = kFinalPairs[i].first;
			moved = kFinalPairs[i].second;
			break;
		}
	}
	fState.final = keep;
	unsigned done = Flush();
	_Start(InitialIndex(moved), medial);
	return done;
}


bool
Composer::Backspace()
{
	if (fHistoryCount > 0) {
		fState = fHistory[--fHistoryCount];
		return true;
	}
	if (!IsEmpty()) {
		Reset();
		return true;
	}
	return false;
}


}	// namespace koreanime
