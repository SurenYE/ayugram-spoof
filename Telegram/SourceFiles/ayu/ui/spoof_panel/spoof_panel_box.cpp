// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/spoof_panel/spoof_panel_box.h"

#include "ayu/spoof/spoof_storage.h"
#include "ayu/ui/spoof_panel/spoof_preview_box.h"
#include "base/timer.h"
#include "lang/lang_keys.h"
#include "styles/style_ayu_styles.h"
#include "styles/style_basic.h"
#include "styles/style_boxes.h"
#include "styles/style_info.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_widgets.h"
#include "ui/chat/chat_style.h"
#include "ui/painter.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"

#include <algorithm>
#include <memory>
#include <QtCore/QRectF>
#include <QtGui/QPainterPath>
#include <QtGui/QPixmap>

namespace AyuUi {
namespace {

[[nodiscard]] QString FormatDuration(crl::time ms) {
	const auto total = static_cast<int>(ms / 1000);
	return QString::number(total / 60)
		+ u":"_q
		+ QString::number(total % 60).rightJustified(2, u'0');
}

class SpoofItem final : public Ui::RippleButton {
public:
	SpoofItem(
		QWidget *parent,
		const Ayu::Spoof::Entry &entry,
		Fn<void()> removeCallback);

	[[nodiscard]] quint64 id() const {
		return _entry.id;
	}

protected:
	void paintEvent(QPaintEvent *e) override;
	void contextMenuEvent(QContextMenuEvent *e) override;

private:
	void paintRound(Painter &p, QRectF area);
	void paintVoiceWave(Painter &p, QRectF area);

	Ayu::Spoof::Entry _entry;
	Fn<void()> _removeCallback;
	QPixmap _thumb;

};

SpoofItem::SpoofItem(
		QWidget *parent,
		const Ayu::Spoof::Entry &entry,
		Fn<void()> removeCallback)
: Ui::RippleButton(parent, st::infoMusicButtonRipple)
, _entry(entry)
, _removeCallback(std::move(removeCallback)) {
	resize(st::spoofPanelItemSize, st::spoofPanelItemSize);
	if (!entry.thumbFileName.isEmpty()) {
		_thumb = QPixmap(Ayu::Spoof::Storage::Instance().thumbPath(entry));
	}
}

void SpoofItem::paintEvent(QPaintEvent *e) {
	Painter p(this);
	p.setPen(Qt::NoPen);
	p.setBrush(st::windowBgOver);
	p.drawRoundedRect(
		rect(),
		st::spoofPanelRounding,
		st::spoofPanelRounding);

	const auto side = width() - 2 * st::spoofPanelItemGap;
	const auto area = QRectF(
		(width() - side) / 2.,
		st::spoofPanelItemGap,
		side,
		side);
	if (_entry.round) {
		paintRound(p, area);
	} else {
		paintVoiceWave(p, area);
	}

	const auto textRect = QRect(
		st::spoofPanelItemGap,
		height() - st::spoofPanelDurationFont->height - st::spoofPanelItemGap,
		width() - 2 * st::spoofPanelItemGap,
		st::spoofPanelDurationFont->height);
	p.setFont(st::spoofPanelDurationFont);
	p.setPen(st::windowFg);
	p.drawText(
		textRect,
		Qt::AlignLeft | Qt::AlignVCenter,
		FormatDuration(_entry.durationMs));
	p.setPen(st::windowSubTextFg);
	p.drawText(
		textRect,
		Qt::AlignRight | Qt::AlignVCenter,
		_entry.round
			? tr::ayu_SpoofPanelTagRound(tr::now)
			: tr::ayu_SpoofPanelTagVoice(tr::now));
}

void SpoofItem::paintRound(Painter &p, QRectF area) {
	if (_thumb.isNull()) {
		p.setPen(QPen(st::windowSubTextFg, st::lineWidth));
		p.setBrush(Qt::NoBrush);
		p.drawEllipse(area);
		return;
	}
	auto path = QPainterPath();
	path.addEllipse(area);
	p.save();
	p.setClipPath(path);
	const auto scaled = _thumb.scaled(
		area.width(),
		area.height(),
		Qt::KeepAspectRatioByExpanding);
	p.drawPixmap(
		area.left() + (area.width() - scaled.width()) / 2,
		area.top() + (area.height() - scaled.height()) / 2,
		scaled);
	p.restore();
}

void SpoofItem::paintVoiceWave(Painter &p, QRectF area) {
	p.setBrush(st::windowSubTextFg);
	const auto bars = 9;
	const auto step = area.width() / bars;
	const auto barWidth = std::max(step / 3, 1.);
	const auto maxHalf = area.height() / 2 - step;
	auto seed = _entry.id;
	for (auto i = 0; i < bars; ++i) {
		seed = seed * 0x9E3779B97F4A7C15ULL + 0x165667B19E3779F9ULL;
		const auto half = step
			+ static_cast<double>((seed >> 40) % std::max(static_cast<quint64>(maxHalf), 1ULL));
		const auto x = area.left() + i * step + (step - barWidth) / 2;
		const auto y = area.center().y() - half;
		p.drawRoundedRect(
			QRectF(x, y, barWidth, half * 2),
			barWidth / 2,
			barWidth / 2);
	}
}

void SpoofItem::contextMenuEvent(QContextMenuEvent *e) {
	// WHY: PopupMenu deletes itself when hidden (deleteOnHide is on by
	// default), so it must not be owned by a unique_ptr here - that used
	// to double-free the menu and crash the client when the box closed.
	const auto menu = Ui::CreateChild<Ui::PopupMenu>(this);
	menu->addAction(
		tr::ayu_SpoofPanelRemove(tr::now),
		_removeCallback,
		&st::menuIconDelete);
	menu->popup(e->globalPos());
}

} // namespace

SpoofPanelBox::SpoofPanelBox(
		QWidget *parent,
		not_null<Window::SessionController*> controller)
: _controller(controller) {
}

void SpoofPanelBox::prepare() {
	addButton(tr::lng_close(), [=] { closeBox(); });
	addLeftButton(tr::ayu_SpoofPanelClearAll(), [=] {
		Ayu::Spoof::Storage::Instance().clear();
		rebuild();
	});

	// WHY: the inner widget is created once and its children are rebuilt
	// in place. Calling setInnerWidget again on a shown box recreates the
	// scroll area and left the new tiles invisible until the box reopened.
	auto content = object_ptr<Ui::VerticalLayout>(this);
	_content = content.get();
	setInnerWidget(std::move(content));
	setDimensionsToContent(st::boxWidth, _content);

	rebuild();
}

void SpoofPanelBox::rebuild() {
	auto &storage = Ayu::Spoof::Storage::Instance();
	storage.load();
	const auto entries = storage.entries();

	setTitle(rpl::single(tr::ayu_SpoofPanelTitle(
		tr::now,
		lt_count,
		static_cast<int>(entries.size()))));

	const auto boxWidth = st::boxWidth;
	const auto itemSize = st::spoofPanelItemSize;
	const auto gap = st::spoofPanelItemGap;
	const auto pad = st::spoofPanelGridPadding;
	const auto cols = std::max(1, (boxWidth - 2 * pad + gap) / (itemSize + gap));

	_content->clear();
	if (entries.empty()) {
		_content->add(object_ptr<Ui::FlatLabel>(
			_content,
			tr::ayu_SpoofPanelEmpty(tr::now),
			st::boxDividerLabel),
			style::margins(pad, pad, pad, pad));
		return;
	}

	const auto rows = (static_cast<int>(entries.size()) + cols - 1) / cols;
	auto grid = object_ptr<Ui::RpWidget>(_content);
	const auto gridRaw = grid.data();
	gridRaw->resize(
		boxWidth - 2 * pad,
		rows * itemSize + (rows - 1) * gap);
	for (auto i = 0; i < static_cast<int>(entries.size()); ++i) {
		const auto entry = entries[i];
		const auto item = Ui::CreateChild<SpoofItem>(gridRaw, entry, [=] {
			Ayu::Spoof::Storage::Instance().remove(entry.id);
			rebuild();
		});
		item->move(
			(i % cols) * (itemSize + gap),
			(i / cols) * (itemSize + gap));
		item->setClickedCallback([this, entry] {
			const auto self = QPointer<SpoofPanelBox>(this);
			_controller->show(Box<SpoofPreviewBox>(
				_controller.get(),
				entry,
				[self] {
					if (self) {
						self->closeBox();
					}
				}));
		});
	}
	_content->add(std::move(grid), style::margins(pad, pad, pad, gap));
	_content->add(object_ptr<Ui::FlatLabel>(
		_content,
		tr::ayu_SpoofPanelHint(tr::now),
		st::boxDividerLabel),
		style::margins(pad, 0, pad, pad));
}

} // namespace AyuUi
