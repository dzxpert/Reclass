#pragma once
#include "dock_header.h"
#include <QComboBox>

namespace rcx {
// Toolbar controls use the same face, flat hover and square edges as dock chrome.
inline QString paneComboStyle(const Theme& t, const QString& family = QString()) {
    return QStringLiteral(
        "QComboBox { background: transparent; color: %1; border: 1px solid transparent;"
        " border-radius: 0px; padding: 0px 6px; font-family: '%5'; font-size: 10pt; }"
        "QComboBox:hover { background: %2; color: %3; }"
        "QComboBox:focus { border-color: %4; }"
        "QComboBox::drop-down { border: none; width: 16px; }"
        "QComboBox::down-arrow { image: none; }"
        "QComboBox QAbstractItemView { background: %6; color: %3;"
        " selection-background-color: %2; selection-color: %3; border: 1px solid %7; }")
        .arg(t.textDim.name(), t.hover.name(), t.text.name(), t.borderFocused.name(),
             family.isEmpty() ? chromeFont().family() : family, t.background.name(), t.border.name());
}

// QSS suppresses Fusion's arrow on flat combos. Use the same tinted SVG as
// AddressBar, rather than a fixed-color asset that disappears on light paper.
class PaneComboBox : public QComboBox {
public:
    using QComboBox::QComboBox;
protected:
    void paintEvent(QPaintEvent* event) override {
        QComboBox::paintEvent(event);
        const auto& t = ThemeManager::instance().current();
        const QIcon arrow = themedVsIcon(":/vsicons/chevron-down.svg", t.textDim,
                                        12, devicePixelRatioF());
        QPainter p(this);
        arrow.paint(&p, QRect(width() - 18, (height() - 12) / 2, 12, 12));
    }
};
inline QString paneToolButtonStyle(const Theme& t) {
    return QStringLiteral(
        "QToolButton { background: transparent; color: %1; border: 1px solid transparent;"
        " border-radius: 0px; padding: 0px 6px; font-family: '%5'; font-size: 10pt; }"
        "QToolButton:hover, QToolButton:pressed { background: %2; color: %3; }"
        "QToolButton:focus { border-color: %4; }"
        "QToolButton::menu-indicator { image: none; }")
        .arg(t.textDim.name(), t.hover.name(), t.text.name(), t.borderFocused.name(), chromeFont().family());
}

// Like AddressBar and DockHeader, own one device-pixel seam beneath the controls.
class PaneToolbar : public QWidget {
public:
    explicit PaneToolbar(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedHeight(kEditorBarHeight);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        const auto& t = ThemeManager::instance().current();
        QPainter p(this);
        p.fillRect(rect(), editorPaperColor(t));
        fillBottomDeviceRowOfRect(p, QRectF(rect()), containerBorderColor(t));
    }
};
} // namespace rcx
