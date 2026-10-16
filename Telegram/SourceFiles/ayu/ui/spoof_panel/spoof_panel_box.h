// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "boxes/abstract_box.h"

namespace Ui {
class RpWidget;
class VerticalLayout;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace AyuUi {

class SpoofPanelBox final : public Ui::BoxContent {
public:
	SpoofPanelBox(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

protected:
	void prepare() override;

private:
	void rebuild();

	not_null<Window::SessionController*> _controller;
	Ui::VerticalLayout *_content = nullptr;

};

} // namespace AyuUi
