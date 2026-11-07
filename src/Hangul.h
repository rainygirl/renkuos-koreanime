/*
 * Copyright 2026 rainygirl. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * The 2-beolsik (두벌식) composer: jamo in, one Hangul syllable at a time
 * out. No system headers, so it builds and is tested anywhere; plain C++98,
 * because on 32-bit x86 the input_server that loads it is a gcc2 program.
 */
#ifndef KOREANIME_HANGUL_H
#define KOREANIME_HANGUL_H


namespace koreanime {


// Compatibility jamo (U+3131-U+3163) for the key with RenkuOS key code
// `key`, or 0 when the key is not one of the 26 letter keys.
unsigned JamoForKey(unsigned key, bool shift);

bool IsConsonant(unsigned jamo);
bool IsVowel(unsigned jamo);

// Writes `codePoint` as UTF-8 into `buffer` (room for 4 bytes and a NUL)
// and returns the number of bytes written.
int EncodeUTF8(unsigned codePoint, char* buffer);


class Composer {
public:
								Composer();

			void				Reset();
			bool				IsEmpty() const;

	// Adds a jamo. Returns the syllable that it completed and that should
	// be committed now, or 0. The syllable still being composed is
	// Preedit().
			unsigned			Input(unsigned jamo);

	// Takes back the last jamo of the syllable being composed. Returns
	// false when there was nothing to take back.
			bool				Backspace();

	// The syllable being composed (a precomposed syllable, or a lone
	// compatibility jamo), or 0 when nothing is being composed.
			unsigned			Preedit() const;

	// Returns Preedit() and empties the composer.
			unsigned			Flush();

private:
	struct State {
		int		initial;	// choseong index 0-18, or -1
		int		medial;		// jungseong index 0-20, or -1
		int		final;		// jongseong index 1-27, or 0
	};

			void				_Push();
			void				_Start(int initial, int medial);

			State				fState;
			State				fHistory[8];
			int					fHistoryCount;
};


}	// namespace koreanime


#endif	// KOREANIME_HANGUL_H
