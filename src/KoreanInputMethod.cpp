/*
 * Copyright 2026 rainygirl. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * A 2-beolsik (두벌식) Korean input method for RenkuOS's input_server.
 *
 * The method stays active and has two modes of its own, Korean and English;
 * in English mode keys go through untouched. 한/영, a tap of the right Alt
 * key and Shift+Space switch modes. While a syllable is being composed it is
 * sent to the focused view as an inline input method composition
 * (B_INPUT_METHOD_EVENT), which BTextView, Terminal and R Chromium show in
 * place -- in R Chromium as a composition that sets KeyboardEvent.isComposing.
 *
 * Plain C++98: on 32-bit x86 input_server is a gcc2 program, and this add-on
 * is built with gcc2 to be loaded by it.
 */

#include <Application.h>
#include <Autolock.h>
#include <Input.h>
#include <InputServerMethod.h>
#include <InterfaceDefs.h>
#include <Locker.h>
#include <Looper.h>
#include <Menu.h>
#include <MenuItem.h>
#include <Message.h>
#include <Messenger.h>
#include <OS.h>

#include <stdlib.h>
#include <string.h>

#include "Hangul.h"
#include "Icons.h"


using koreanime::Composer;


// The message input_server's Deskbar menu sends to pick a method; its
// handler, InputServer::HandleSetMethod(), takes the method's cookie.
static const uint32 kSetMethod = 'MRsm';
// The keymap method is always cookie 1; add-on methods count on from 2.
static const int32 kFirstAddOnCookie = 2;
static const int32 kLastCookieToTry = 24;

static const uint32 kMsgKorean = 'KoKo';
static const uint32 kMsgEnglish = 'KoEn';

static const uint32 kSpaceKey = 0x5e;
static const uint32 kHangulKey = 0xf0;		// B_HANGUL_KEY on Korean keyboards
static const uint32 kHangulHanjaKey = 0xf1;	// B_HANGUL_HANJA_KEY


class KoreanInputMethod;


// Receives what views send back (B_INPUT_METHOD_STOPPED when they drop the
// composition) and the Deskbar menu's choices.
class MethodLooper : public BLooper {
public:
								MethodLooper(KoreanInputMethod* method);

	virtual	void				MessageReceived(BMessage* message);

private:
			KoreanInputMethod*	fMethod;
};


class KoreanInputMethod : public BInputServerMethod {
public:
								KoreanInputMethod();
	virtual						~KoreanInputMethod();

	virtual	status_t			InitCheck();
	virtual	filter_result		Filter(BMessage* message, BList* outList);
	virtual	status_t			MethodActivated(bool active);

			void				CompositionDropped();
			void				SetKoreanMode(bool korean);

private:
			void				_ToggleMode(BList* outList);
			void				_UpdateLook();
			void				_Commit(BList* outList);
			void				_AddStarted(BList* outList);
			void				_AddChanged(BList* outList, unsigned syllable,
									bool confirmed);
			void				_AddStopped(BList* outList);
			void				_ShowPreedit(BList* outList);
			bool				_IsRightAltTap(BMessage* message);

	static	int32				_ActivateThread(void* data);

			BLocker				fLock;
			Composer			fComposer;
			MethodLooper*		fLooper;
			BMenu*				fMenu;
			bool				fKorean;
			bool				fComposing;
			bool				fActive;
			bool				fRightAltDown;
			bool				fKeySinceRightAlt;
			bool				fKeyPending;
			thread_id			fActivateThread;
			volatile bool		fQuitting;
};


extern "C" _EXPORT BInputServerMethod* instantiate_input_method();


BInputServerMethod*
instantiate_input_method()
{
	return new KoreanInputMethod();
}


// #pragma mark - MethodLooper


MethodLooper::MethodLooper(KoreanInputMethod* method)
	:
	BLooper("koreanime"),
	fMethod(method)
{
}


void
MethodLooper::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case B_INPUT_METHOD_EVENT:
		{
			int32 opcode;
			if (message->FindInt32("be:opcode", &opcode) == B_OK
				&& opcode == B_INPUT_METHOD_STOPPED) {
				fMethod->CompositionDropped();
			}
			break;
		}
		case kMsgKorean:
			fMethod->SetKoreanMode(true);
			break;
		case kMsgEnglish:
			fMethod->SetKoreanMode(false);
			break;
		default:
			BLooper::MessageReceived(message);
	}
}


// #pragma mark - KoreanInputMethod


KoreanInputMethod::KoreanInputMethod()
	:
	BInputServerMethod("한국어 두벌식", kRomanIcon),
	fLock("koreanime"),
	fLooper(NULL),
	fMenu(NULL),
	fKorean(false),
	fComposing(false),
	fActive(false),
	fRightAltDown(false),
	fKeySinceRightAlt(false),
	fKeyPending(false),
	fActivateThread(-1),
	fQuitting(false)
{
	fLooper = new MethodLooper(this);
	fLooper->Run();

	_UpdateLook();

	// Korean input should not need a trip to the Deskbar menu after every
	// boot: make this the active method once input_server has settled.
	fActivateThread = spawn_thread(_ActivateThread, "koreanime activate",
		B_LOW_PRIORITY, this);
	if (fActivateThread >= 0)
		resume_thread(fActivateThread);
}


KoreanInputMethod::~KoreanInputMethod()
{
	fQuitting = true;
	if (fActivateThread >= 0) {
		status_t result;
		wait_for_thread(fActivateThread, &result);
	}

	SetMenu(NULL, BMessenger());
	delete fMenu;

	if (fLooper->Lock())
		fLooper->Quit();
}


status_t
KoreanInputMethod::InitCheck()
{
	return fLooper != NULL ? B_OK : B_NO_MEMORY;
}


/*!	input_server has no call for "make me the active method", and the only
	handle on a method is the cookie it gets when it is loaded -- which this
	add-on is never told. Its Deskbar menu selects a method by sending
	'MRsm' with that cookie to input_server, so this does the same for each
	cookie add-on methods can have, until MethodActivated(true) says that it
	was ours.
*/
int32
KoreanInputMethod::_ActivateThread(void* data)
{
	KoreanInputMethod* self = (KoreanInputMethod*)data;

	// input_server loads add-ons before it is ready to switch methods
	for (int32 wait = 0; wait < 30 && !self->fQuitting; wait++)
		snooze(100000);

	BMessenger inputServer(be_app);
	for (int32 cookie = kFirstAddOnCookie;
			cookie <= kLastCookieToTry && !self->fQuitting; cookie++) {
		if (self->fActive)
			return 0;
		BMessage message(kSetMethod);
		message.AddInt32("cookie", cookie);
		if (inputServer.SendMessage(&message) != B_OK)
			return 0;
		for (int32 wait = 0; wait < 5 && !self->fActive; wait++)
			snooze(100000);
	}

	if (!self->fActive && !self->fQuitting) {
		// whatever got activated on the way, go back to the keymap
		BMessage message(kSetMethod);
		message.AddInt32("cookie", 1);
		inputServer.SendMessage(&message);
	}
	return 0;
}


status_t
KoreanInputMethod::MethodActivated(bool active)
{
	BAutolock _(fLock);

	if (!active && fComposing) {
		// Commit what is there; Filter() is not called again until we are
		// active, so this goes through input_server's method queue.
		BList events;
		_Commit(&events);
		for (int32 i = 0; i < events.CountItems(); i++)
			EnqueueMessage((BMessage*)events.ItemAt(i));
	}

	fActive = active;
	fRightAltDown = false;

	if (active)
		_UpdateLook();

	return BInputServerMethod::MethodActivated(active);
}


void
KoreanInputMethod::CompositionDropped()
{
	BAutolock _(fLock);
	fComposer.Reset();
	fComposing = false;
}


void
KoreanInputMethod::SetKoreanMode(bool korean)
{
	BAutolock _(fLock);
	if (fKorean == korean)
		return;

	if (fComposing) {
		BList events;
		_Commit(&events);
		for (int32 i = 0; i < events.CountItems(); i++)
			EnqueueMessage((BMessage*)events.ItemAt(i));
	}
	fKorean = korean;
	_UpdateLook();
}


void
KoreanInputMethod::_ToggleMode(BList* outList)
{
	_Commit(outList);
	fKorean = !fKorean;
	_UpdateLook();
}


void
KoreanInputMethod::_UpdateLook()
{
	SetIcon(fKorean ? kHangulIcon : kRomanIcon);

	BMenu* menu = new BMenu("한국어");
	BMenuItem* korean = new BMenuItem("한글  (한/영, 오른쪽 Alt, Shift+Space)",
		new BMessage(kMsgKorean));
	BMenuItem* english = new BMenuItem("English", new BMessage(kMsgEnglish));
	korean->SetMarked(fKorean);
	english->SetMarked(!fKorean);
	menu->AddItem(korean);
	menu->AddItem(english);

	SetMenu(menu, BMessenger(NULL, fLooper));
	delete fMenu;
	fMenu = menu;
}


void
KoreanInputMethod::_AddStarted(BList* outList)
{
	BMessage* message = new BMessage(B_INPUT_METHOD_EVENT);
	message->AddInt32("be:opcode", B_INPUT_METHOD_STARTED);
	message->AddMessenger("be:reply_to", BMessenger(NULL, fLooper));
	outList->AddItem(message);
	fComposing = true;
}


void
KoreanInputMethod::_AddChanged(BList* outList, unsigned syllable,
	bool confirmed)
{
	char text[5];
	int32 length = 0;
	text[0] = '\0';
	if (syllable != 0)
		length = koreanime::EncodeUTF8(syllable, text);

	BMessage* message = new BMessage(B_INPUT_METHOD_EVENT);
	message->AddInt32("be:opcode", B_INPUT_METHOD_CHANGED);
	message->AddString("be:string", text);
	message->AddInt32("be:clause_start", 0);
	message->AddInt32("be:clause_end", length);
	message->AddInt32("be:selection", length);
	message->AddInt32("be:selection", length);
	message->AddBool("be:confirmed", confirmed);
	// The first change a key causes says so. R Chromium sends the page a
	// keydown with key "Process" for each of these, and no more: one key can
	// commit a syllable and start the next, and that is still one keydown.
	message->AddBool("koreanime:key", fKeyPending);
	fKeyPending = false;
	outList->AddItem(message);
}


void
KoreanInputMethod::_AddStopped(BList* outList)
{
	BMessage* message = new BMessage(B_INPUT_METHOD_EVENT);
	message->AddInt32("be:opcode", B_INPUT_METHOD_STOPPED);
	outList->AddItem(message);
	fComposing = false;
}


// Commits the syllable being composed, if any, and ends the composition.
void
KoreanInputMethod::_Commit(BList* outList)
{
	unsigned syllable = fComposer.Flush();
	if (!fComposing && syllable == 0)
		return;
	if (!fComposing)
		_AddStarted(outList);
	_AddChanged(outList, syllable, true);
	_AddStopped(outList);
}


void
KoreanInputMethod::_ShowPreedit(BList* outList)
{
	unsigned preedit = fComposer.Preedit();
	if (preedit == 0) {
		if (fComposing) {
			// everything was taken back: leave nothing behind
			_AddChanged(outList, 0, true);
			_AddStopped(outList);
		}
		return;
	}
	if (!fComposing)
		_AddStarted(outList);
	_AddChanged(outList, preedit, false);
}


/*!	A press and release of the right Alt key with nothing typed in between
	-- what the 한/영 key is on most Korean laptops. With RenkuOS's default
	PC layout right Alt is the right Command key.
*/
bool
KoreanInputMethod::_IsRightAltTap(BMessage* message)
{
	uint32 modifiers = 0;
	uint32 oldModifiers = 0;
	message->FindInt32("modifiers", (int32*)&modifiers);
	message->FindInt32("be:old_modifiers", (int32*)&oldModifiers);

	const uint32 kRightAlt = B_RIGHT_COMMAND_KEY;
	const uint32 kOthers = B_SHIFT_KEY | B_CONTROL_KEY | B_OPTION_KEY
		| B_LEFT_COMMAND_KEY;

	bool wasDown = (oldModifiers & kRightAlt) != 0;
	bool isDown = (modifiers & kRightAlt) != 0;

	if (!wasDown && isDown) {
		fRightAltDown = (modifiers & kOthers) == 0;
		fKeySinceRightAlt = false;
		return false;
	}
	if (wasDown && !isDown) {
		bool tap = fRightAltDown && !fKeySinceRightAlt
			&& (modifiers & kOthers) == 0;
		fRightAltDown = false;
		return tap;
	}
	if ((modifiers & kOthers) != 0)
		fRightAltDown = false;
	return false;
}


filter_result
KoreanInputMethod::Filter(BMessage* message, BList* outList)
{
	BAutolock _(fLock);
	fKeyPending = false;

	if (message->what == B_MODIFIERS_CHANGED) {
		if (_IsRightAltTap(message)) {
			// the modifiers change itself still goes on
			_ToggleMode(outList);
			outList->AddItem(new BMessage(*message));
			return B_DISPATCH_MESSAGE;
		}
		return B_DISPATCH_MESSAGE;
	}

	if (message->what != B_KEY_DOWN && message->what != B_UNMAPPED_KEY_DOWN)
		return B_DISPATCH_MESSAGE;

	fKeySinceRightAlt = true;

	int32 key = 0;
	uint32 modifiers = 0;
	message->FindInt32("key", &key);
	message->FindInt32("modifiers", (int32*)&modifiers);

	if (key == (int32)kHangulKey) {
		_ToggleMode(outList);
		return B_SKIP_MESSAGE;
	}
	if (key == (int32)kHangulHanjaKey)
		return B_SKIP_MESSAGE;

	const uint32 kShortcut = B_COMMAND_KEY | B_CONTROL_KEY | B_OPTION_KEY;

	if (key == (int32)kSpaceKey && (modifiers & B_SHIFT_KEY) != 0
		&& (modifiers & kShortcut) == 0) {
		_ToggleMode(outList);
		return B_SKIP_MESSAGE;
	}

	if (message->what != B_KEY_DOWN)
		return B_DISPATCH_MESSAGE;

	if (!fKorean || (modifiers & kShortcut) != 0) {
		// typing in English, or a shortcut: what is composed is done
		if (!fComposing && fComposer.IsEmpty())
			return B_DISPATCH_MESSAGE;
		_Commit(outList);
		outList->AddItem(new BMessage(*message));
		return B_DISPATCH_MESSAGE;
	}

	bool shift = (modifiers & B_SHIFT_KEY) != 0;
	unsigned jamo = koreanime::JamoForKey((unsigned)key, shift);
	if (jamo != 0) {
		fKeyPending = true;
		unsigned done = fComposer.Input(jamo);
		if (done != 0) {
			if (!fComposing)
				_AddStarted(outList);
			_AddChanged(outList, done, true);
			_AddStopped(outList);
		}
		_ShowPreedit(outList);
		return outList->CountItems() > 0 ? B_DISPATCH_MESSAGE : B_SKIP_MESSAGE;
	}

	const char* bytes = NULL;
	if (message->FindString("bytes", &bytes) == B_OK && bytes != NULL
		&& bytes[0] == B_BACKSPACE && bytes[1] == '\0'
		&& !fComposer.IsEmpty()) {
		fKeyPending = true;
		fComposer.Backspace();
		_ShowPreedit(outList);
		return outList->CountItems() > 0 ? B_DISPATCH_MESSAGE : B_SKIP_MESSAGE;
	}

	// anything else -- space, digits, punctuation, Enter, the arrows --
	// ends the syllable and then does what it does
	if (!fComposing && fComposer.IsEmpty())
		return B_DISPATCH_MESSAGE;
	_Commit(outList);
	outList->AddItem(new BMessage(*message));
	return B_DISPATCH_MESSAGE;
}
