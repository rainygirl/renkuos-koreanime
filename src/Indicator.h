/*
 * Copyright 2026 rainygirl. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * What the Deskbar indicator and the input method say to each other.
 */
#ifndef KOREANIME_INDICATOR_H
#define KOREANIME_INDICATOR_H


#include <SupportDefs.h>


namespace koreanime {

// The Deskbar item's name, and the name of the input method's looper in
// input_server, which the indicator finds by scripting.
static const char* const kIndicatorName = "koreanime";
static const char* const kMethodLooperName = "koreanime";

// indicator -> method: "indicator" (BMessenger) wants the state from now on
static const uint32 kMsgRegister = 'KoRg';
// method -> indicator: "korean" (bool), "active" (bool)
static const uint32 kMsgState = 'KoSt';
// indicator -> method: switch modes
static const uint32 kMsgKorean = 'KoKo';
static const uint32 kMsgEnglish = 'KoEn';

}	// namespace koreanime


#endif	// KOREANIME_INDICATOR_H
