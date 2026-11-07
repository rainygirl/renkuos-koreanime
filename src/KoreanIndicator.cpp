/*
 * Copyright 2026 rainygirl. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * The Deskbar item that shows the input mode: 한 or A, drawn with the system
 * font at the size Deskbar gives its items, so it stays sharp on a scaled
 * (HiDPI) screen. input_server's own method item is a 16x16 bitmap that is
 * never scaled; the input method takes it out of the Deskbar and puts this
 * one in.
 *
 * Deskbar loads this add-on, calls instantiate_deskbar_item(), archives the
 * view and instantiates it again from the archive, so the view is archivable
 * and has no "add_on" signature: the class is found in the image Deskbar
 * already loaded.
 *
 * Plain C++98, as the input method: on 32-bit x86 Deskbar is a gcc2 program.
 */

#include <Font.h>
#include <Menu.h>
#include <MenuItem.h>
#include <Message.h>
#include <Messenger.h>
#include <PopUpMenu.h>
#include <View.h>
#include <Window.h>

#include <math.h>
#include <string.h>

#include "Indicator.h"


using namespace koreanime;


static const char* const kInputServerSignature
	= "application/x-vnd.Be-input_server";
// try to reach the input method every this many pulses while it is not there
static const int32 kRegisterEveryPulses = 4;


class _EXPORT KoreanIndicator : public BView {
public:
								KoreanIndicator(BRect frame);
								KoreanIndicator(BMessage* archive);

	static	KoreanIndicator*	Instantiate(BMessage* archive);
	virtual	status_t			Archive(BMessage* archive, bool deep) const;

	virtual	void				AttachedToWindow();
	virtual	void				Draw(BRect updateRect);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MessageReceived(BMessage* message);
	virtual	void				Pulse();

private:
			void				_Register();

			BMessenger			fMethod;
			bool				fKorean;
			bool				fActive;
			int32				fPulses;
};


extern "C" _EXPORT BView* instantiate_deskbar_item(float maxWidth,
	float maxHeight);


BView*
instantiate_deskbar_item(float maxWidth, float maxHeight)
{
	float side = maxHeight;
	if (side < 16)
		side = 16;
	return new KoreanIndicator(BRect(0, 0, side - 1, side - 1));
}


KoreanIndicator::KoreanIndicator(BRect frame)
	:
	BView(frame, kIndicatorName, B_FOLLOW_LEFT | B_FOLLOW_TOP,
		B_WILL_DRAW | B_PULSE_NEEDED),
	fKorean(false),
	fActive(false),
	fPulses(0)
{
}


KoreanIndicator::KoreanIndicator(BMessage* archive)
	:
	BView(archive),
	fKorean(false),
	fActive(false),
	fPulses(0)
{
}


KoreanIndicator*
KoreanIndicator::Instantiate(BMessage* archive)
{
	if (!validate_instantiation(archive, "KoreanIndicator"))
		return NULL;
	return new KoreanIndicator(archive);
}


status_t
KoreanIndicator::Archive(BMessage* archive, bool deep) const
{
	return BView::Archive(archive, deep);
}


void
KoreanIndicator::AttachedToWindow()
{
	BView::AttachedToWindow();
	if (Parent() != NULL)
		SetViewColor(Parent()->ViewColor());
	else
		SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	SetLowColor(ViewColor());
	_Register();
}


void
KoreanIndicator::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	float scale = bounds.Height() / 16.0f;
	if (scale < 1)
		scale = 1;

	// a key cap: light face, dark rim, the letter in the middle
	BRect key = bounds.InsetByCopy(floorf(scale), floorf(scale));
	float radius = 2 * scale;
	rgb_color face = {255, 255, 255, 255};
	rgb_color rim = {80, 80, 80, 255};
	rgb_color ink = {0, 0, 0, 255};
	if (!fActive) {
		// another input method (the keymap) is in use
		face = tint_color(ViewColor(), B_LIGHTEN_1_TINT);
		rim = tint_color(ViewColor(), B_DARKEN_2_TINT);
		ink = tint_color(ViewColor(), B_DARKEN_3_TINT);
	}

	SetHighColor(face);
	FillRoundRect(key, radius, radius);
	SetHighColor(rim);
	SetPenSize(scale > 1.5f ? floorf(scale) : 1);
	StrokeRoundRect(key, radius, radius);
	SetPenSize(1);

	const char* label = fKorean && fActive ? "한" : "A";
	BFont font(be_bold_font);
	font.SetSize(key.Height() * 0.72f);
	SetFont(&font);
	font_height height;
	font.GetHeight(&height);
	float width = font.StringWidth(label);
	BPoint where(key.left + (key.Width() - width) / 2 + 0.5f,
		key.top + (key.Height() + height.ascent - height.descent) / 2 + 0.5f);
	SetHighColor(ink);
	SetLowColor(face);
	DrawString(label, BPoint(floorf(where.x), floorf(where.y)));
	SetLowColor(ViewColor());
}


void
KoreanIndicator::MouseDown(BPoint where)
{
	if (!fMethod.IsValid())
		_Register();

	BPopUpMenu* menu = new BPopUpMenu("koreanime", false, false);
	menu->SetFont(be_plain_font);
	BMenuItem* korean = new BMenuItem("한글  (한/영, 오른쪽 Alt, Shift+Space)",
		new BMessage(kMsgKorean));
	BMenuItem* english = new BMenuItem("English", new BMessage(kMsgEnglish));
	korean->SetMarked(fKorean);
	english->SetMarked(!fKorean);
	menu->AddItem(korean);
	menu->AddItem(english);
	menu->SetEnabled(fMethod.IsValid());
	menu->SetTargetForItems(fMethod);

	ConvertToScreen(&where);
	BRect clickToOpen = ConvertToScreen(Bounds());
	menu->SetAsyncAutoDestruct(true);
	menu->Go(where, true, true, clickToOpen, true);
}


void
KoreanIndicator::MessageReceived(BMessage* message)
{
	if (message->what == kMsgState) {
		bool korean;
		bool active;
		if (message->FindBool("korean", &korean) == B_OK)
			fKorean = korean;
		if (message->FindBool("active", &active) == B_OK)
			fActive = active;
		Invalidate();
		return;
	}
	BView::MessageReceived(message);
}


void
KoreanIndicator::Pulse()
{
	// input_server restarts now and then (and starts after Deskbar)
	if (fMethod.IsValid())
		return;
	if (fActive) {
		fActive = false;
		Invalidate();
	}
	if (++fPulses % kRegisterEveryPulses == 0)
		_Register();
}


/*!	Finds the input method's looper in input_server by scripting --
	BApplication resolves "Looper" by name, any BHandler answers "Messenger"
	-- and asks it for the state, now and whenever it changes.
*/
void
KoreanIndicator::_Register()
{
	BMessenger inputServer(kInputServerSignature);
	if (!inputServer.IsValid())
		return;

	BMessage request(B_GET_PROPERTY);
	request.AddSpecifier("Messenger");
	request.AddSpecifier("Looper", kMethodLooperName);
	BMessage reply;
	if (inputServer.SendMessage(&request, &reply, 500000, 500000) != B_OK)
		return;
	BMessenger method;
	if (reply.FindMessenger("result", &method) != B_OK || !method.IsValid())
		return;

	BMessage hello(kMsgRegister);
	hello.AddMessenger("indicator", BMessenger(this));
	if (method.SendMessage(&hello, (BHandler*)NULL, 500000) == B_OK)
		fMethod = method;
}
