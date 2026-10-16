// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ayu/spoof/spoof_storage.h"
#include "boxes/abstract_box.h"

#include <memory>
#include <QtGui/QImage>

namespace Media::Streaming {
class Instance;
} // namespace Media::Streaming

namespace Window {
class SessionController;
} // namespace Window

namespace AyuUi {

class SpoofPreviewBox final : public Ui::BoxContent {
public:
	SpoofPreviewBox(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		Ayu::Spoof::Entry entry,
		Fn<void()> onSent);
	~SpoofPreviewBox();

protected:
	void prepare() override;

private:
	void setupStreaming();
	void togglePlayback();
	void startPlayback();
	void paintContent(class QPainter &p);

	not_null<Window::SessionController*> _controller;
	Ayu::Spoof::Entry _entry;
	Fn<void()> _onSent;

	std::unique_ptr<Media::Streaming::Instance> _instance;
	class Ui::RpWidget *_view = nullptr;

	QImage _frame;
	crl::time _position = 0;
	crl::time _duration = 0;
	bool _paused = false;
	bool _finished = false;
	bool _failed = false;

};

} // namespace AyuUi
