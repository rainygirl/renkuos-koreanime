/*
 * Copyright 2026 rainygirl. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * A 2-beolsik (두벌식) Korean input method for RenkuOS's input_server.
 *
 * The method stays active and has two modes of its own, Korean and English;
 * in English mode keys go through untouched. 한/영, a tap of the right Alt
 * key and Shift+Space switch modes. A syllable being composed shows where
 * the text goes, in every application:
 *
 * - Views that take input method events (B_INPUT_METHOD_AWARE: BTextView,
 *   so StyledEdit and text fields, and R Chromium) get an inline
 *   composition -- in R Chromium one that sets KeyboardEvent.isComposing.
 * - Other views (Terminal, ...) would get input_server's composition window
 *   at the bottom of the screen. Instead every event of the composition is
 *   marked be:inline_only, which input_server drops for such views, and
 *   beside it goes a "direct" event whose be:translated key presses -- the
 *   syllable, or a Backspace and the new syllable when it changes -- are
 *   what input_server hands them in its place. Aware views ignore those
 *   events: they carry no be:string.
 *
 * Plain C++98: on 32-bit x86 input_server is a gcc2 program, and this add-on
 * is built with gcc2 to be loaded by it.
 */

#include <Application.h>
#include <Autolock.h>
#include <Deskbar.h>
#include <Entry.h>
#include <Path.h>
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
#include <image.h>

#include <stdlib.h>
#include <string.h>

#include "Hangul.h"
#include "Icons.h"
#include "Indicator.h"


using koreanime::Composer;
using koreanime::kMsgKorean;
using koreanime::kMsgEnglish;


// The message input_server's Deskbar menu sends to pick a method; its
// handler, InputServer::HandleSetMethod(), takes the method's cookie.
static const uint32 kSetMethod = 'MRsm';
// The keymap method is always cookie 1; add-on methods count on from 2.
static const int32 kFirstAddOnCookie = 2;
static const int32 kLastCookieToTry = 24;

// input_server's own Deskbar item, a 16x16 bitmap that is not scaled
static const char* const kMethodReplicantName = "MethodReplicant";

static const uint32 kSpaceKey = 0x5e;
static const uint32 kBackspaceKey = 0x1e;
static const uint32 kHangulKey = 0xf0;		// B_HANGUL_KEY on Korean keyboards
static const uint32 kHangulHanjaKey = 0xf1;	// B_HANGUL_HANJA_KEY


class KoreanInputMethod;


// Receives what views send back (B_INPUT_METHOD_STOPPED when they drop the
// composition), the Deskbar menu's choices and the Deskbar indicator.
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
			void				SetIndicator(const BMessenger& indicator);

private:
			void				_ToggleMode(BList* outList);
			void				_UpdateLook();
			void				_Commit(BList* outList);
			void				_AddStarted(BList* outList);
			void				_AddChanged(BList* outList, unsigned syllable,
									bool confirmed);
			void				_AddStopped(BList* outList);
			void				_ShowPreedit(BList* outList);
			void				_ShowDirect(BList* outList, unsigned done,
									unsigned preedit);
			void				_AddDirectKey(BList* outList, unsigned syllable);
			bool				_IsRightAltTap(BMessage* message);

			void				_NotifyIndicator();

	static	int32				_ActivateThread(void* data);
	static	int32				_DeskbarThread(void* data);

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
			// the syllable typed into a view that is not input method aware
			// as plain text, still to be replaced while it is composed
			unsigned			fShown;
			uint32				fLocks;
			BMessenger			fIndicator;
			thread_id			fActivateThread;
			thread_id			fDeskbarThread;
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
	BLooper(koreanime::kMethodLooperName),
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
		case koreanime::kMsgRegister:
		{
			BMessenger indicator;
			if (message->FindMessenger("indicator", &indicator) == B_OK)
				fMethod->SetIndicator(indicator);
			break;
		}
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
	fShown(0),
	fLocks(0),
	fActivateThread(-1),
	fDeskbarThread(-1),
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

	fDeskbarThread = spawn_thread(_DeskbarThread, "koreanime deskbar",
		B_LOW_PRIORITY, this);
	if (fDeskbarThread >= 0)
		resume_thread(fDeskbarThread);
}


KoreanInputMethod::~KoreanInputMethod()
{
	fQuitting = true;
	status_t result;
	if (fActivateThread >= 0)
		wait_for_thread(fActivateThread, &result);
	if (fDeskbarThread >= 0)
		wait_for_thread(fDeskbarThread, &result);

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
			return 0;	// picked in the Deskbar already
		BMessage message(kSetMethod);
		message.AddInt32("cookie", cookie);
		if (inputServer.SendMessage(&message) != B_OK)
			return 0;
		for (int32 wait = 0; wait < 5 && !self->fActive; wait++)
			snooze(100000);
		if (self->fActive)
			return 0;
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
	else
		_NotifyIndicator();

	return BInputServerMethod::MethodActivated(active);
}


void
KoreanInputMethod::CompositionDropped()
{
	BAutolock _(fLock);
	fComposer.Reset();
	fComposing = false;
	fShown = 0;
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
KoreanInputMethod::SetIndicator(const BMessenger& indicator)
{
	BAutolock _(fLock);
	fIndicator = indicator;
	_NotifyIndicator();
}


// Tells the Deskbar indicator the mode; never waits for it.
void
KoreanInputMethod::_NotifyIndicator()
{
	if (!fIndicator.IsValid())
		return;
	BMessage message(koreanime::kMsgState);
	message.AddBool("korean", fKorean);
	message.AddBool("active", fActive);
	fIndicator.SendMessage(&message, (BHandler*)NULL, 0);
}


// The indicator add-on: add-ons/koreanime/KoreanIndicator, beside the
// add-ons/input_server/methods folder this add-on was loaded from -- in the
// system or in ~/config/non-packaged alike.
static status_t
indicator_ref(entry_ref* ref)
{
	image_info info;
	int32 cookie = 0;
	addr_t self = (addr_t)&indicator_ref;
	while (get_next_image_info(B_CURRENT_TEAM, &cookie, &info) == B_OK) {
		addr_t text = (addr_t)info.text;
		if (self < text || self >= text + info.text_size)
			continue;
		BPath path(info.name);
		for (int32 i = 0; i < 3; i++) {
			if (path.GetParent(&path) != B_OK)
				return B_ERROR;
		}
		path.Append("koreanime/KoreanIndicator");
		return get_ref_for_path(path.Path(), ref);
	}
	return B_ERROR;
}


/*!	Keeps the Deskbar showing this method's indicator instead of
	input_server's item. input_server puts its item back whenever its list of
	methods changes, and Deskbar may start after input_server or be
	restarted, so this looks again every few seconds.
*/
int32
KoreanInputMethod::_DeskbarThread(void* data)
{
	KoreanInputMethod* self = (KoreanInputMethod*)data;

	entry_ref ref;
	bool haveIndicator = indicator_ref(&ref) == B_OK
		&& BEntry(&ref).Exists();

	int32 wait = 20;
	while (!self->fQuitting) {
		for (int32 i = 0; i < wait && !self->fQuitting; i++)
			snooze(100000);
		wait = 30;
		if (self->fQuitting || !haveIndicator)
			break;

		BDeskbar deskbar;
		if (!deskbar.IsRunning())
			continue;
		if (!deskbar.HasItem(koreanime::kIndicatorName)
			&& deskbar.AddItem(&ref) != B_OK)
			continue;
		if (deskbar.HasItem(kMethodReplicantName))
			deskbar.RemoveItem(kMethodReplicantName);
	}
	return 0;
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
	_NotifyIndicator();

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
	message->AddBool("be:inline_only", true);
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
	message->AddBool("be:inline_only", true);
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
	message->AddBool("be:inline_only", true);
	outList->AddItem(message);
	fComposing = false;
}


/*!	One key press for a view that is not input method aware: the syllable,
	or Backspace when \a syllable is 0. input_server delivers be:translated
	to such views and drops the rest of the event; aware views get the event
	itself and pass over it, as it has no be:string.
*/
void
KoreanInputMethod::_AddDirectKey(BList* outList, unsigned syllable)
{
	char text[5];
	int32 key = 0;
	int32 rawChar = 0xa;
	if (syllable != 0) {
		text[koreanime::EncodeUTF8(syllable, text)] = '\0';
	} else {
		text[0] = B_BACKSPACE;
		text[1] = '\0';
		key = kBackspaceKey;
		rawChar = B_BACKSPACE;
	}

	BMessage keyDown(B_KEY_DOWN);
	keyDown.AddInt64("when", system_time());
	keyDown.AddInt32("key", key);
	keyDown.AddInt32("modifiers", fLocks);
	for (int32 i = 0; text[i] != '\0'; i++)
		keyDown.AddInt8("byte", (int8)text[i]);
	keyDown.AddString("bytes", text);
	keyDown.AddInt32("raw_char", rawChar);

	BMessage* message = new BMessage(B_INPUT_METHOD_EVENT);
	message->AddInt32("be:opcode", B_INPUT_METHOD_CHANGED);
	message->AddBool("be:inline_only", true);
	message->AddBool("be:confirmed", true);
	message->AddBool("koreanime:direct", true);
	message->AddMessage("be:translated", &keyDown);
	outList->AddItem(message);
}


/*!	Brings a view that is not input method aware up to date after a key:
	\a done is the syllable the key finished, if any, \a preedit the one now
	being composed. What fShown says is on screen is replaced only if it
	changed.
*/
void
KoreanInputMethod::_ShowDirect(BList* outList, unsigned done, unsigned preedit)
{
	unsigned first = done != 0 ? done : preedit;
	if (fShown != 0 && first == fShown) {
		if (done != 0 && preedit != 0)
			_AddDirectKey(outList, preedit);
	} else {
		if (fShown != 0)
			_AddDirectKey(outList, 0);
		if (done != 0)
			_AddDirectKey(outList, done);
		if (preedit != 0)
			_AddDirectKey(outList, preedit);
	}
	fShown = preedit;
}


// Commits the syllable being composed, if any, and ends the composition.
void
KoreanInputMethod::_Commit(BList* outList)
{
	unsigned syllable = fComposer.Flush();
	// in a view that is not aware, the syllable is already there as text
	fShown = 0;
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

	if (message->what == B_MOUSE_DOWN) {
		// a click may move the caret: what is composed is done
		if (!fComposing && fComposer.IsEmpty())
			return B_DISPATCH_MESSAGE;
		_Commit(outList);
		outList->AddItem(new BMessage(*message));
		return B_DISPATCH_MESSAGE;
	}

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
	fLocks = modifiers & (B_CAPS_LOCK | B_NUM_LOCK | B_SCROLL_LOCK);

	// The toggle keys themselves go nowhere, but what _ToggleMode() commits
	// must: input_server drops outList with B_SKIP_MESSAGE, and an aware view
	// (R Chromium, BTextView) would be left composing.
	if (key == (int32)kHangulKey) {
		_ToggleMode(outList);
		return outList->CountItems() > 0 ? B_DISPATCH_MESSAGE : B_SKIP_MESSAGE;
	}
	if (key == (int32)kHangulHanjaKey)
		return B_SKIP_MESSAGE;

	const uint32 kShortcut = B_COMMAND_KEY | B_CONTROL_KEY | B_OPTION_KEY;

	if (key == (int32)kSpaceKey && (modifiers & B_SHIFT_KEY) != 0
		&& (modifiers & kShortcut) == 0) {
		_ToggleMode(outList);
		return outList->CountItems() > 0 ? B_DISPATCH_MESSAGE : B_SKIP_MESSAGE;
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
		_ShowDirect(outList, done, fComposer.Preedit());
		return outList->CountItems() > 0 ? B_DISPATCH_MESSAGE : B_SKIP_MESSAGE;
	}

	const char* bytes = NULL;
	if (message->FindString("bytes", &bytes) == B_OK && bytes != NULL
		&& bytes[0] == B_BACKSPACE && bytes[1] == '\0'
		&& !fComposer.IsEmpty()) {
		fKeyPending = true;
		fComposer.Backspace();
		_ShowPreedit(outList);
		_ShowDirect(outList, 0, fComposer.Preedit());
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
