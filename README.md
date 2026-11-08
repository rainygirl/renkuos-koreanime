# Korean IME for RenkuOS

[English](README.md) | [한국어](README.ko.md)

A Korean input method (2-beolsik, 두벌식) for RenkuOS. Type Hangul
in RenkuOS applications, in Terminal and in R Chromium, where web pages get
proper composition events (`KeyboardEvent.isComposing`). Installing it also
switches the system to Korean.

## Install with pkgman

| RenkuOS | Commands |
| --- | --- |
| 32-bit x86 (x86_gcc2) | `pkgman add-repo https://pkgman.rainygirl.com/x86_gcc2`<br>`pkgman install koreanime` |
| x86_64 | `pkgman add-repo https://pkgman.rainygirl.com/x86_64`<br>`pkgman install koreanime` |
| arm64 | `pkgman add-repo https://pkgman.rainygirl.com/arm64`<br>`pkgman install koreanime` |

That is all. Afterwards:

- the interface language and formatting switch to Korean (ko, ko_KR), unless
  the system already uses Korean;
- the input method turns itself on, now and after every boot. Deskbar shows
  **A** while you type English and **한** while you type Korean, sized for
  the screen (sharp on HiDPI too). It takes the place of the system's small
  input method icon.

## Typing Korean

| | |
| --- | --- |
| Korean / English | the **한/영** key, a tap of the **right Alt** key, or **Shift+Space** |
| Layout | standard 2-beolsik; Shift gives ㅃ ㅉ ㄸ ㄲ ㅆ ㅒ ㅖ |
| Backspace | takes back the last jamo of the syllable being typed |
| Mode menu | click the Deskbar icon |

**Alt+Space** is RenkuOS's own shortcut for switching input methods. If Korean
stops working, press Alt+Space once to bring this one back.

## Where it works

- **Everywhere you type**: the syllable is composed in place, at the
  cursor, with no separate window -- in RenkuOS applications (StyledEdit,
  Mail, the Tracker and everything built on BTextView), in Terminal and in
  applications that do not support input methods.
- **R Chromium** 154.0.8036.0-18 and later (x86_64, arm64), and
  rchromium_x86 114.0.5735.199-18 and later (32-bit x86): pages see
  compositionstart / compositionupdate / compositionend, and keydown with
  `isComposing` and key "Process", as in Chrome on other systems. Enter after
  a syllable sends the whole text, without the doubled last character some
  chat sites show elsewhere.

## Language

```sh
koreanime-setup --status     # current language and formatting
koreanime-setup --english    # back to English
koreanime-setup              # Korean again
```

## Install from source

On the RenkuOS machine (on 32-bit x86 with the default gcc2 compiler):

```sh
make install       # the input method and koreanime-setup, for this user
make uninstall
make test          # the composer's tests
```

## AI disclosure

Parts of this input method were developed with the assistance of AI coding
tools (Anthropic's Claude). All code has been reviewed and tested by the
author.

## License

MIT. Development notes are in [AGENTS.md](AGENTS.md).
