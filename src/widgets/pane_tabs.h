#pragma once

#include "themes/theme.h"
#include "themes/thememanager.h"
#include "paintutil.h"
#include "dock_header.h"

#include <QPaintEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QPainter>
#include <QLabel>
#include <QSplitter>
#include <QString>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

namespace rcx {

// ── The per-pane view tabs (bottom of every editor pane) ──
// One label per command: these tabs OWN the view mode, so tab 0 is the
// structure view — it used to carry the application name, which put the brand
// on screen twice (title bar + a tab) and named a *view* after the product.
struct PaneTabSpec { const char* name; const char* tip; };

inline constexpr PaneTabSpec kPaneTabs[] = {
    { "Structure", "Structure view" },
    { "Code",      "Generated code" },
    { "Graph",     "Type relationships and references" },
    { "Debug",     "Layout diagnostics (compose dump)" },
};
inline constexpr int kPaneTabCount = int(sizeof(kPaneTabs) / sizeof(kPaneTabs[0]));

// Use the document-tab grammar: adjacent cells, single device-pixel dividers,
// regular chrome text and a two-device-pixel underline on the current view.
class PaneTabBar : public QTabBar {
public:
    explicit PaneTabBar(QWidget* parent = nullptr) : QTabBar(parent) {
        setFont(chromeFont());
        setExpanding(false);
        setDrawBase(false);
        setMouseTracking(true);
    }
protected:
    QSize tabSizeHint(int index) const override {
        return {fontMetrics().horizontalAdvance(tabText(index)) + 2 * kGutter,
                kDockHeaderContentH};
    }
    QSize minimumTabSizeHint(int index) const override { return tabSizeHint(index); }
    void mouseMoveEvent(QMouseEvent* e) override {
        const int next = tabAt(e->pos());
        if (next != hovered) { hovered = next; update(); }
        QTabBar::mouseMoveEvent(e);
    }
    void leaveEvent(QEvent* e) override { hovered = -1; update(); QTabBar::leaveEvent(e); }
    void paintEvent(QPaintEvent*) override {
        const auto& t = ThemeManager::instance().current();
        const QColor seam = containerBorderColor(t);
        QPainter p(this);
        p.fillRect(rect(), t.background);
        fillTopDeviceRowOfRect(p, QRectF(rect()), seam);
        for (int i = 0; i < count(); ++i) {
            if (!isTabVisible(i)) continue;
            const QRect r = tabRect(i);
            const bool active = i == currentIndex();
            p.fillRect(r, active ? editorPaperColor(t) : i == hovered ? t.hover : t.background);
            // Leave the selected cell open to the content above it.
            if (!active) fillTopDeviceRowOfRect(p, QRectF(r), seam);
            fillRightDeviceColOfRect(p, QRectF(r), seam);
            p.setPen(active || i == hovered ? t.text : t.textDim);
            p.drawText(r.adjusted(kGutter, 0, -kGutter, 0), Qt::AlignVCenter | Qt::AlignLeft, tabText(i));
            if (active) {
                fillBottomDeviceRowsOfRect(p, QRectF(r), kUnderlineRows, t.indHoverSpan);
                if (hasFocus()) {
                    const QRectF focus = QRectF(r).adjusted(2, 2, -2, -2);
                    fillLeftDeviceColOfRect(p, focus, t.borderFocused);
                    fillRightDeviceColOfRect(p, focus, t.borderFocused);
                }
            }
        }
    }
private:
    int hovered = -1;
};

class PaneTabs : public QTabWidget {
public:
    explicit PaneTabs(QWidget* parent = nullptr) : QTabWidget(parent) {
        setTabBar(new PaneTabBar(this));
        setTabPosition(South);
        setDocumentMode(true);
    }
protected:
    void paintEvent(QPaintEvent* e) override {
        QTabWidget::paintEvent(e);
        // Continue the strip seam through the spare space to the corner controls.
        QPainter p(this);
        fillTopDeviceRowOfRect(p, QRectF(0, tabBar()->y(), width(), tabBar()->height()),
                              containerBorderColor(ThemeManager::instance().current()));
    }
};

inline QString paneTabStyle(const Theme& t, const QString& family) {
    return QStringLiteral(
        "QTabWidget { background: %1; }"
        "QTabWidget::pane { border: none; background: %2; }"
        "QTabBar { border: none; background: %1; font-family: '%3'; font-size: 10pt; }")
        .arg(t.background.name(), editorPaperColor(t).name(), family);
}

// ── The pane's outline ──
//
// The box used to be painted by the CONTENT: main.cpp's EditorContainer drew a
// 4-edge box around itself. That made the outline track "was this view wrapped
// in a container" rather than "this is a pane", and it showed three ways:
// Structure and Code were boxed (both are wrapped), Debug was bare (its
// QsciScintilla IS the tab page), Both drew TWO boxes (one container per
// splitter side), and the view-tab strip — outside every container — hung below
// the box as an unbordered band, pure white on a light theme.
//
// So a container around the whole pane draws it instead: one outline, the view
// tabs inside it, identical in all four view modes. The tab widget is held with
// a 1-logical-px margin so the edges are never covered by it; painting them on
// the QTabWidget itself does not work, because its page and tab bar are
// children that cover the parent completely.
//
// PaneTabs owns the content/strip seam; content containers have no inset border.
//
// No Q_OBJECT: this declares no signals or slots, and both the functor+context
// connect and findChildren work without moc (the rule BreadcrumbBar and
// EnumPickerPopup already follow).
class PaneBox : public QWidget {
public:
    explicit PaneBox(QWidget* inner, QWidget* parent = nullptr) : QWidget(parent) {
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(1, 1, 1, 1);   // room for the box
        lay->setSpacing(0);
        lay->addWidget(inner);
        m_inner = inner;
        applyBoxTheme(ThemeManager::instance().current());
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
                [this](const Theme& t) { applyBoxTheme(t); });
    }

    void previewPlacement(int direction) {
        m_previewDirection = direction;
        if (!m_preview) {
            m_preview = new QLabel(this);
            m_preview->setAttribute(Qt::WA_TransparentForMouseEvents);
            m_preview->setAlignment(Qt::AlignCenter);
            m_preview->setFont(font());
        }
        if (!direction) { m_preview->hide(); return; }
        const auto& t = ThemeManager::instance().current();
        const QColor c = t.borderFocused;
        m_preview->setStyleSheet(QString("background: rgba(%1,%2,%3,35); color: %4; border: 1px solid %5;")
            .arg(c.red()).arg(c.green()).arg(c.blue()).arg(t.text.name(), c.name()));
        m_preview->setText(direction == Qt::Horizontal ? "New view to the right" : "New view below");
        positionPreview(); m_preview->show(); m_preview->raise();
    }
    QColor boxColor() const { return m_box; }
    void setPaneActive(bool active) { m_active = active; update(); }

protected:
    void resizeEvent(QResizeEvent* e) override { QWidget::resizeEvent(e); positionPreview(); }
    void paintEvent(QPaintEvent*) override {
        if (!m_box.isValid()) return;
        QPainter p(this);
        const QRectF r(rect());
        rcx::fillTopDeviceRowOfRect(p, r, m_active ? m_focus : m_box);
        rcx::fillBottomDeviceRowOfRect(p, r, m_box);
        rcx::fillLeftDeviceColOfRect(p, r, m_box);
        rcx::fillRightDeviceColOfRect(p, r, m_box);
    }

private:
    QLabel* m_preview = nullptr;
    int m_previewDirection = 0;
    void positionPreview() {
        if (!m_preview || !m_previewDirection) return;
        QRect r = rect().adjusted(2, 2, -2, -34);
        if (m_previewDirection == Qt::Horizontal) r.setLeft(r.center().x());
        else r.setTop(r.center().y());
        m_preview->setGeometry(r);
    }
    void applyBoxTheme(const Theme& t) {
        m_box = containerBorderColor(t);
        m_focus = t.borderFocused;
        // Both mode puts the two views in a splitter inside this box, so its
        // handle is an interior seam and has to match. Done here so no
        // theme-apply site has to know the pane owns a splitter.
        const QString handle = QStringLiteral("QSplitter::handle { background: %1; }")
                                   .arg(m_box.name());
        if (m_inner)
            for (QSplitter* sp : m_inner->findChildren<QSplitter*>())
                sp->setStyleSheet(handle);
        update();
    }

    bool m_active = false;
    QWidget* m_inner = nullptr;
    QColor   m_box;
    QColor   m_focus;
};

} // namespace rcx
