// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/spoof_panel/spoof_preview_box.h"

#include "ayu/spoof/spoof_sender.h"
#include "base/variant.h"
#include "lang/lang_keys.h"
#include "media/streaming/media_streaming_common.h"
#include "media/streaming/media_streaming_document.h"
#include "media/streaming/media_streaming_instance.h"
#include "media/streaming/media_streaming_loader_local.h"
#include "media/streaming/media_streaming_player.h"
#include "styles/style_ayu_styles.h"
#include "styles/style_basic.h"
#include "styles/style_boxes.h"
#include "styles/style_chat.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_widgets.h"
#include "ui/chat/chat_style.h"
#include "ui/style/style_core_scale.h"
#include "ui/painter.h"
#include "ui/widgets/checkbox.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <algorithm>
#include <QtCore/QEvent>
#include <QtGui/QPainterPath>
#include <QtGui/QMouseEvent>

namespace AyuUi {
namespace {

constexpr auto kVoiceHeight = 96;
constexpr auto kBadgeRadius = 24;

[[nodiscard]] QString FormatDuration(crl::time ms) {
	const auto total = static_cast<int>(ms / 1000);
	return QString::number(total / 60)
		+ u":"_q
		+ QString::number(total % 60).rightJustified(2, u'0');
}

void PaintPlayBadge(
		QPainter &p,
		QPointF center,
		bool paused,
		const QColor &fg,
		const QColor &bg) {
	p.setRenderHint(QPainter::Antialiasing);
	p.setPen(Qt::NoPen);
	p.setBrush(bg);
	p.drawEllipse(center, kBadgeRadius, kBadgeRadius);
	p.setBrush(fg);
	if (paused) {
		const auto r = kBadgeRadius * 0.45;
		p.drawRect(QRectF(center.x() - r * 0.55 - 3, center.y() - r, 4.5, r * 2));
		p.drawRect(QRectF(center.x() - r * 0.55 + 4, center.y() - r, 4.5, r * 2));
	} else {
		QPainterPath triangle;
		triangle.moveTo(center.x() - 6, center.y() - 10);
		triangle.lineTo(center.x() + 11, center.y());
		triangle.lineTo(center.x() - 6, center.y() + 10);
		triangle.closeSubpath();
		p.drawPath(triangle);
	}
}

} // namespace

SpoofPreviewBox::SpoofPreviewBox(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		Ayu::Spoof::Entry entry,
		Fn<void()> onSent)
: _controller(controller)
, _entry(std::move(entry))
, _onSent(std::move(onSent)) {
}

SpoofPreviewBox::~SpoofPreviewBox() = default;

void SpoofPreviewBox::prepare() {
	_duration = std::max(_entry.durationMs, crl::time(0));

	setTitle(rpl::single(QString(
		(_entry.round
			? tr::ayu_SpoofPanelTagRound(tr::now)
			: tr::ayu_SpoofPanelTagVoice(tr::now))
		+ u" · "_q
		+ FormatDuration(_duration))));

	addButton(tr::lng_close(), [=] { closeBox(); });
	addButton(tr::ayu_SpoofPanelSend(), [=] {
		if (_instance) {
			_instance->stop();
		}
		const auto viewOnce = _viewOnce && _viewOnce->checked();
		Ayu::Spoof::SendEntryToActiveChat(_controller.get(), _entry, viewOnce);
		if (_onSent) {
			_onSent();
		}
		closeBox();
	}, getDelegate()->style().button);

	const auto width = st::boxWidth;
	const auto height = _entry.round ? width : kVoiceHeight;
	auto content = object_ptr<Ui::VerticalLayout>(this);
	const auto raw = content.get();
	_view = raw->add(object_ptr<Ui::RpWidget>(raw));
	_view->resize(width, height);
	_viewOnce = raw->add(object_ptr<Ui::Checkbox>(
		raw,
		tr::ayu_SpoofPanelViewOnce(tr::now),
		false,
		st::defaultBoxCheckbox),
		style::margins(st::spoofPanelGridPadding, 0, st::spoofPanelGridPadding, st::spoofPanelGridPadding));
	setInnerWidget(std::move(content));
	setDimensionsToContent(st::boxWidth, raw);

	_view->paintRequest(
	) | rpl::on_next([=] {
		auto p = QPainter(_view);
		paintContent(p);
	}, lifetime());

	_view->events(
	) | rpl::on_next([=](not_null<QEvent*> e) {
		if (e->type() == QEvent::MouseButtonPress) {
			const auto mouse = static_cast<QMouseEvent*>(e.get());
			if (mouse->button() == Qt::LeftButton) {
				togglePlayback();
			}
		}
	}, lifetime());

	setupStreaming();
}

void SpoofPreviewBox::setupStreaming() {
	const auto path = Ayu::Spoof::Storage::Instance().filePath(_entry);
	auto loader = Media::Streaming::MakeFileLoader(path);
	if (!loader) {
		_failed = true;
		return;
	}
	_instance = std::make_unique<Media::Streaming::Instance>(
		std::make_shared<Media::Streaming::Document>(std::move(loader)),
		nullptr);
	if (!_instance->valid()) {
		_instance = nullptr;
		_failed = true;
		return;
	}
	_instance->lockPlayer();
	_instance->player().updates(
	) | rpl::on_next_error([=](Media::Streaming::Update &&update) {
		using namespace Media::Streaming;
		v::match(update.data, [&](Information &info) {
			const auto d = _entry.round
				? info.video.state.duration
				: info.audio.state.duration;
			if (d > 0) {
				_duration = d;
			}
			_view->update();
		}, [&](PreloadedVideo &) {
		}, [&](UpdateVideo &data) {
			_position = data.position;
			if (_instance) {
				// WHY: frames without alpha require non-empty outer
				// (PrepareByRequest asserts on it), like FrameRequestFor
				// in editor/video/video_editor.cpp does.
				auto request = Media::Streaming::FrameRequest();
				request.resize = _view->size() * style::DevicePixelRatio();
				request.outer = request.resize;
				_frame = _instance->frame(request);
			}
			_view->update();
		}, [&](PreloadedAudio &) {
		}, [&](UpdateAudio &data) {
			_position = data.position;
			_view->update();
		}, [&](WaitingForData &) {
		}, [&](SpeedEstimate &) {
		}, [&](MutedByOther &) {
		}, [&](Finished &) {
			_finished = true;
			_paused = true;
			_position = 0;
			_view->update();
		});
	}, [=](Media::Streaming::Error &&) {
		_instance = nullptr;
		_failed = true;
		_view->update();
	}, lifetime());

	startPlayback();
}

void SpoofPreviewBox::startPlayback() {
	if (!_instance) {
		return;
	}
	auto options = Media::Streaming::PlaybackOptions();
	options.mode = _entry.round
		? Media::Streaming::Mode::Both
		: Media::Streaming::Mode::Audio;
	options.position = _finished ? 0 : std::max(_position, crl::time(0));
	options.loop = false;
	_instance->play(options);
	_finished = false;
	_paused = false;
	_view->update();
}

void SpoofPreviewBox::togglePlayback() {
	if (!_instance) {
		return;
	}
	if (_finished) {
		startPlayback();
		return;
	}
	if (_paused) {
		_instance->resume();
		_paused = false;
	} else {
		_instance->pause();
		_paused = true;
	}
	_view->update();
}

void SpoofPreviewBox::paintContent(QPainter &p) {
	p.setRenderHint(QPainter::Antialiasing);
	const auto rect = _view->rect();
	const auto center = QRectF(rect).center();

	if (_failed) {
		p.setFont(st::normalFont);
		p.setPen(st::windowSubTextFg);
		p.drawText(rect, Qt::AlignCenter, tr::ayu_SpoofPanelMissing(tr::now));
		return;
	}

	if (_entry.round) {
		const auto side = std::min(rect.width(), rect.height()) - 8.;
		const auto area = QRectF(
			center.x() - side / 2,
			center.y() - side / 2,
			side,
			side);
		if (!_frame.isNull()) {
			auto path = QPainterPath();
			path.addEllipse(area);
			p.save();
			p.setClipPath(path);
			p.drawImage(area, _frame);
			p.restore();
		} else {
			p.setPen(QPen(st::windowSubTextFg, st::lineWidth));
			p.setBrush(Qt::NoBrush);
			p.drawEllipse(area);
		}
		if (_paused || !_instance) {
			// WHY: paused means "press to play" - show the triangle.
			PaintPlayBadge(
				p,
				center,
				false,
				QColor(Qt::white),
				QColor(0, 0, 0, 128));
		}
		return;
	}

	// Voice: badge on the left, progress bar and time on the right.
	const auto badgeCenter = QPointF(
		kBadgeRadius + 8,
		center.y());
	PaintPlayBadge(
		p,
		badgeCenter,
		!_paused && _instance,
		QColor(Qt::white),
		st::windowActiveTextFg->c);

	const auto barLeft = badgeCenter.x() + kBadgeRadius + 16;
	const auto bar = QRectF(
		barLeft,
		center.y() - 2,
		rect.width() - barLeft - 8,
		4);
	p.setPen(Qt::NoPen);
	p.setBrush(st::windowBgOver);
	p.drawRoundedRect(bar, 2, 2);
	const auto progress = (_duration > 0)
		? std::clamp(float64(_position) / _duration, 0., 1.)
		: 0.;
	p.setBrush(st::windowActiveTextFg);
	p.drawRoundedRect(
		QRectF(bar.left(), bar.top(), bar.width() * progress, bar.height()),
		2,
		2);

	p.setFont(st::normalFont);
	p.setPen(st::windowSubTextFg);
	p.drawText(
		QRectF(barLeft, center.y() + 10, bar.width(), st::normalFont->height),
		Qt::AlignLeft | Qt::AlignVCenter,
		FormatDuration(std::max(_position, crl::time(0)))
		+ u" / "_q
		+ FormatDuration(std::max(_duration, crl::time(0))));
}

} // namespace AyuUi
