// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/spoof/spoof_sender.h"

#include "api/api_common.h"
#include "api/api_send_progress.h"
#include "apiwrap.h"
#include "base/timer.h"
#include "base/weak_ptr.h"
#include "crl/crl_on_main.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_thread.h"
#include "history/history.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "storage/localimageloader.h"
#include "window/window_session_controller.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <QtCore/QFile>
#include <vector>

namespace Ayu::Spoof {
namespace {

// WHY: the manager re-sends an action not sooner than in 5 s, tick faster
// so the next send lands right at the expiry and the status never blinks.
constexpr auto kStatusRepeat = crl::time(2500);

struct SenderState {
	base::weak_ptr<Window::SessionController> controller;
	History *history = nullptr;
	Api::SendProgressType type = Api::SendProgressType::RecordVoice;
	QByteArray data;
	Entry entry;
	bool viewOnce = false;
	std::optional<base::Timer> statusTimer;
	std::optional<base::Timer> endTimer;
};

// WHY: owns active senders; the timer callback keeps only a weak reference,
// so finishing (or the controller dying then finishing) frees the state.
std::vector<std::shared_ptr<SenderState>> &ActiveSenders() {
	static auto result = std::vector<std::shared_ptr<SenderState>>();
	return result;
}

void FinishSender(const std::shared_ptr<SenderState> &state) {
	if (state->statusTimer) {
		state->statusTimer->cancel();
		state->statusTimer->setCallback(nullptr);
	}
	if (state->endTimer) {
		state->endTimer->cancel();
		state->endTimer->setCallback(nullptr);
	}
	auto &active = ActiveSenders();
	active.erase(
		std::remove(active.begin(), active.end(), state),
		active.end());
	if (const auto controller = state->controller.get()) {
		auto &session = controller->session();
		session.sendProgressManager().cancel(state->history, state->type);
		auto options = Api::SendOptions();
		if (state->viewOnce) {
			// WHY: tdesktop marks "view once" media with an effectively
			// infinite ttl, exactly like the round-video record bar does.
			options.ttlSeconds = std::numeric_limits<int>::max();
		}
		const auto to = FileLoadTo(
			state->history->peer->id,
			options,
			FullReplyTo(),
			MsgId());
		session.api().fileLoader()->addTask(
			std::make_unique<FileLoadTask>(FileLoadTask::VoiceArgs{
				.session = &session,
				.voice = state->data,
				.duration = state->entry.durationMs,
				.waveform = QVector<signed char>(),
				.video = state->entry.round,
				.to = to,
				.caption = TextWithTags(),
			}));
	}
}

} // namespace

void SendEntryToActiveChat(
		Window::SessionController *controller,
		const Entry &entry,
		bool viewOnce) {
	if (!controller) {
		return;
	}
	const auto thread = controller->activeChatCurrent().thread();
	if (!thread) {
		controller->showToast(tr::ayu_SpoofPanelNoChat(tr::now));
		return;
	}
	const auto path = Storage::Instance().filePath(entry);
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly)) {
		controller->showToast(tr::ayu_SpoofPanelMissing(tr::now));
		Storage::Instance().remove(entry.id);
		return;
	}
	const auto state = std::make_shared<SenderState>();
	state->controller = base::make_weak(controller);
	state->history = thread->owningHistory();
	state->type = entry.round
		? Api::SendProgressType::RecordRound
		: Api::SendProgressType::RecordVoice;
	state->data = file.readAll();
	state->entry = entry;
	state->viewOnce = viewOnce;

	if (entry.durationMs <= 0) {
		FinishSender(state);
		return;
	}

	ActiveSenders().push_back(state);
	const auto history = state->history;
	const auto type = state->type;
	state->controller->session().sendProgressManager().update(history, type, 0);
	state->statusTimer.emplace([weak = std::weak_ptr<SenderState>(state), history, type] {
		const auto sender = weak.lock();
		if (!sender) {
			return;
		}
		crl::guard(sender->controller, [=] {
			sender->controller->session().sendProgressManager().update(history, type, 0);
		})();
	});
	state->statusTimer->callEach(kStatusRepeat);
	state->endTimer.emplace([weak = std::weak_ptr<SenderState>(state)] {
		const auto sender = weak.lock();
		if (sender) {
			FinishSender(sender);
		}
	});
	state->endTimer->callOnce(entry.durationMs);
}

} // namespace Ayu::Spoof
