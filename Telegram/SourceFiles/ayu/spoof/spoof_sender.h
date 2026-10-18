// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ayu/spoof/spoof_storage.h"

namespace Window {
class SessionController;
} // namespace Window

namespace Ayu::Spoof {

// Shows "recording voice/video message" for the entry duration,
// then uploads the stored file as a fresh message to the active chat.
// When viewOnce is set, the media is sent as disappearing (one view).
void SendEntryToActiveChat(
	Window::SessionController *controller,
	const Entry &entry,
	bool viewOnce = false);

} // namespace Ayu::Spoof
