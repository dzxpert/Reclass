#pragma once
#include "pane_tabs.h"
#include "pane_controls.h"
#include "dock_header.h"
#include <QMenu>
#include <QComboBox>
#include <QTimer>
#include <functional>

namespace rcx {
class PaneViewStrip : public QWidget {
public:
    std::function<void(int)> chooseView;
    std::function<void(Qt::Orientation, int)> splitView;
    std::function<void(int)> previewPlacement;
    std::function<bool(Qt::Orientation)> canSplit;
    std::function<void()> pairViews, closePane, activated, keepOnly, equalize, maximize;
    std::function<void(QMenu*)> populateControls;

    explicit PaneViewStrip(QTabWidget* pages) : QWidget(pages), pages(pages) {
        setObjectName("rcxPaneStrip");
        row = new QHBoxLayout(this);
        row->setContentsMargins(0, 0, kGutter, 0); row->setSpacing(0);
        setFixedHeight(kDockHeaderContentH);
        pages->installEventFilter(this);
        connect(pages->tabBar(), &QTabBar::tabBarClicked, this, [this](int) {
            if (activated) activated();
        });
        split = makeHeaderToolButton(":/vsicons/split-horizontal.svg", "Split view", this);
        split->setText("Split"); split->setAccessibleName("Split view");
        split->setPopupMode(QToolButton::InstantPopup);
        auto* menu = new QMenu(split);
        addPlacement(menu, "Split Right", Qt::Horizontal);
        addPlacement(menu, "Split Below", Qt::Vertical);
        split->setMenu(menu); row->addWidget(split);
        more = makeHeaderToolButton(":/vsicons/ellipsis.svg", "View options", this);
        more->setAccessibleName("View options"); more->setPopupMode(QToolButton::InstantPopup);
        auto* options = new QMenu(more);
        connect(options, &QMenu::aboutToShow, this, [this, options] {
            options->clear();
            if (populateControls) populateControls(options);
            if (canClose) {
                if (!options->isEmpty()) options->addSeparator();
                options->addAction(maximized ? "Restore split" : "Expand this view", this, [this] { if (maximize) maximize(); });
                options->addAction("Show only this view", this, [this] { if (keepOnly) keepOnly(); });
            }
            if (!options->isEmpty()) options->addSeparator();
            auto* diagnostics = options->addAction("Layout diagnostics", this, [this] { select(current == 3 ? 0 : 3); });
            diagnostics->setCheckable(true); diagnostics->setChecked(current == 3);
            diagnostics->setToolTip("Developer details about the Structure renderer");
        });
        more->setMenu(options); row->addWidget(more);
        close = makeHeaderToolButton(":/vsicons/close.svg", "Close this split", this);
        close->setAccessibleName("Close view"); close->hide(); row->addWidget(close);

        connect(close, &QToolButton::clicked, this, [this] { if (closePane) closePane(); });
        applyTheme(ThemeManager::instance().current());
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this](const Theme& t) { applyTheme(t); });
    }
    void setControls(QWidget* widget) {
        controls = widget;
        updateOverflow();
    }
    void setView(int mode) {
        current = mode;
        pages->tabBar()->setTabVisible(3, mode == 3);
        updateOverflow();
    }
    void setCanClose(bool can) { canClose = can; close->setVisible(can); updateOverflow(); }
    void setMaximized(bool value) { maximized = value; }
    void applyTheme(const Theme& t) {
        setFont(chromeFont());
        setStyleSheet("QWidget#rcxPaneStrip { background: " + t.background.name() + "; }");
        for (auto* b : {split, close, more}) {
            b->setFont(chromeFont()); b->setFocusPolicy(Qt::StrongFocus);
            b->setStyleSheet(paneToolButtonStyle(t) + "QToolButton { padding: 0px; }");
            b->setIcon(themedVsIcon(b->property("rcxIconPath").toString(), t.textDim, 14, devicePixelRatioF()));
            if (b->menu()) b->menu()->setFont(chromeFont());
        }
        if (controls) {
            for (auto* label : controls->findChildren<QLabel*>()) {
                label->setFont(chromeFont()); label->setStyleSheet("color: " + t.textDim.name());
            }
            for (auto* button : controls->findChildren<QToolButton*>()) button->setStyleSheet(paneToolButtonStyle(t));
        }
        updateOverflow();
    }
    QSize minimumSizeHint() const override { return sizeHint(); }
protected:
    void paintEvent(QPaintEvent*) override {
        const auto& t = ThemeManager::instance().current();
        QPainter p(this); p.fillRect(rect(), t.background);
        fillTopDeviceRowOfRect(p, QRectF(rect()), containerBorderColor(t));
    }
    void showEvent(QShowEvent* e) override {
        QWidget::showEvent(e);
        QTimer::singleShot(0, this, [this] {
            pages->tabBar()->ensurePolished();
            updateOverflow();
            layout()->invalidate();
            layout()->activate();
        });
    }
    bool eventFilter(QObject* object, QEvent* event) override {
        if (object == pages && event->type() == QEvent::Resize) updateOverflow();
        return QWidget::eventFilter(object, event);
    }
private:
    QHBoxLayout* row;
    QTabWidget* pages;
    QToolButton *split, *close, *more;
    QWidget* controls = nullptr;
    int current = 0;
    bool canClose = false, maximized = false;
    void select(int i) { if (activated) activated(); if (chooseView) chooseView(i); }
    void addPlacement(QMenu* menu, const QString& label, Qt::Orientation direction) {
        auto* action = menu->addAction(label, this, [this, direction] {
            if (previewPlacement) previewPlacement(0);
            if (splitView) splitView(direction, current == 0 ? 1 : 0);
        });
        action->setData(int(direction));
        if (!menu->property("previewConnected").toBool()) {
            menu->setProperty("previewConnected", true); menu->setToolTipsVisible(true);
            connect(menu, &QMenu::aboutToShow, this, [this, menu] {
                for (auto* a : menu->actions()) {
                    const bool available = !canSplit || canSplit(Qt::Orientation(a->data().toInt()));
                    a->setEnabled(available);
                    a->setToolTip(available ? "Open a second view of this document" : "Enlarge the pane to make room for a second view");
                }
            });
            connect(menu, &QMenu::hovered, this, [this](QAction* a) { if (previewPlacement) previewPlacement(a->data().toInt()); });
            connect(menu, &QMenu::aboutToHide, this, [this] { if (previewPlacement) previewPlacement(0); });
        }
    }
    void updateOverflow() {
        const int paneWidth = pages->width();
        split->setToolButtonStyle(Qt::ToolButtonIconOnly);

        split->setVisible(current != 3 || paneWidth >= 400);
        if (controls) {
            controls->setVisible(current == 1);
            for (auto* w : controls->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
                const QString role = w->property("paneRole").toString();
                if (role == "scope") w->setVisible(paneWidth >= 440);
                else if (role == "label") w->setVisible(paneWidth >= 600);
                else if (role == "options") w->setVisible(paneWidth >= 720);
            }
        }
        adjustSize();
        // QTabWidget can retain a too-small bar after opening a split on Graph.
        // Reserve the labels' natural width once styles and corner controls settle.
        auto* bar = pages->tabBar();
        bar->setMinimumWidth(qMin(bar->sizeHint().width(), qMax(0, paneWidth - sizeHint().width())));
    }
};
} // namespace rcx
