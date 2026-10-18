#include <QtTest/QTest>
#include <QPointer>
#include "widgets/pane_layout.h"
#include "widgets/pane_view_strip.h"

using namespace rcx;
class TestPaneLayout : public QObject {
    Q_OBJECT
private slots:
    static PaneViewStrip* makeTabs(QTabWidget& pages) {
        pages.setTabPosition(QTabWidget::South);
        pages.setDocumentMode(true);
        pages.setStyleSheet(paneTabStyle(ThemeManager::instance().current(), "Consolas"));
        for (const auto& tab : kPaneTabs) pages.addTab(new QWidget, tab.name);
        pages.tabBar()->setTabVisible(3, false);
        auto* strip = new PaneViewStrip(&pages);
        pages.setCornerWidget(strip, Qt::BottomRightCorner);
        QObject::connect(&pages, &QTabWidget::currentChanged, strip,
                         [strip](int i) { strip->setView(i); });
        strip->chooseView = [&pages](int i) {
            pages.tabBar()->setTabVisible(3, i == 3);
            pages.setCurrentIndex(i);
        };
        return strip;
    }
    void bottomTabsSwitchPagesAtNarrowWidths() {
        PaneTabs pages;
        auto* strip = makeTabs(pages);
        strip->setCanClose(true);
        pages.resize(90, 400);
        pages.setCurrentIndex(2);
        pages.show();
        QCoreApplication::processEvents();
        pages.resize(300, 400);
        QCoreApplication::processEvents();
        auto* tabs = pages.tabBar();
        for (int width : {1080, 420, 300}) {
            pages.resize(width, 400); QCoreApplication::processEvents();
            for (int i = 0; i < 3; ++i) {
                QVERIFY(tabs->rect().contains(tabs->tabRect(i)));
                QCOMPARE(tabs->tabRect(i).top(), 0);
                if (i > 0) QCOMPARE(tabs->tabRect(i - 1).right() + 1, tabs->tabRect(i).left());
                QTest::mouseClick(tabs, Qt::LeftButton, Qt::NoModifier, tabs->tabRect(i).center());
                QCOMPARE(pages.currentIndex(), i);
                QVERIFY(pages.widget(i)->isVisible());
                QCOMPARE(tabs->y(), pages.widget(i)->geometry().bottom() + 1);
            }
        }
        tabs->setFocus(); QTest::keyClick(tabs, Qt::Key_Left);
        QCOMPARE(pages.currentIndex(), 1);
    }
    void nestedSplitsAndClosingSpecificPane() {
        QSplitter root(Qt::Horizontal);
        root.resize(1200, 700);
        auto* a = new QWidget; root.addWidget(a);
        root.show();
        auto* b = new QWidget;
        auto* horizontal = splitPaneLeaf(a, b, Qt::Horizontal);
        QVERIFY(horizontal); QCOMPARE(root.count(), 1);
        const auto before = horizontal->sizes();
        auto* c = new QWidget;
        QPointer<QSplitter> vertical = splitPaneLeaf(a, c, Qt::Vertical);
        QCOMPARE(horizontal->orientation(), Qt::Horizontal);
        QCOMPARE(vertical->orientation(), Qt::Vertical);
        QCOMPARE(horizontal->widget(1), b);
        QCOMPARE(horizontal->sizes(), before);
        QCOMPARE(vertical->widget(0), a); QCOMPARE(vertical->widget(1), c);
        // Closing the first-created leaf must not remove the last-created leaf.
        QPointer<QWidget> removed = a;
        removePaneLeaf(a, &root);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(removed.isNull()); QVERIFY(vertical.isNull());
        QCOMPARE(horizontal->widget(0), c); QCOMPARE(horizontal->widget(1), b);
        removePaneLeaf(b, &root);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(root.count(), 1); QCOMPARE(root.widget(0), c);
    }

    void maximizeRestoreNestedSizes() {
        PaneSplitter root(Qt::Horizontal); root.resize(1200, 700);
        auto* a = new QWidget; root.addWidget(a); root.show();
        auto* b = new QWidget; auto* h = splitPaneLeaf(a, b, Qt::Horizontal);
        auto* c = new QWidget; auto* v = splitPaneLeaf(a, c, Qt::Vertical);
        h->setSizes({700, 500}); v->setSizes({250, 450});
        QCoreApplication::processEvents();
        const auto hs = h->sizes(), vs = v->sizes();
        maximizePaneLeaf(c, &root); QCoreApplication::processEvents();
        QVERIFY(a->isHidden()); QVERIFY(b->isHidden()); QVERIFY(c->isVisible());
        restorePaneLayout(&root); QCoreApplication::processEvents();
        QVERIFY(a->isVisible()); QVERIFY(b->isVisible());
        QCOMPARE(h->sizes(), hs); QCOMPARE(v->sizes(), vs);
        equalizePaneLayout(&root);
        QVERIFY(qAbs(h->sizes()[0] - h->sizes()[1]) <= 1);
        QVERIFY(qAbs(v->sizes()[0] - v->sizes()[1]) <= 1);
    }
    void stripSelectionOverflowAndActions() {
        PaneTabs pages;
        PaneViewStrip& strip = *makeTabs(pages);
        pages.resize(800, 400); pages.show();
        QCoreApplication::processEvents();
        int chosen = -1, mode = -2, preview = 0, closed = 0;
        Qt::Orientation direction = Qt::Vertical;
        connect(&pages, &QTabWidget::currentChanged, &pages, [&](int i) { chosen = i; });
        strip.splitView = [&](Qt::Orientation d, int m) { direction = d; mode = m; };
        strip.previewPlacement = [&](int d) { preview = d; };
        strip.closePane = [&] { ++closed; };
        auto* tabs = pages.tabBar(); QVERIFY(tabs);
        QTest::mouseClick(tabs, Qt::LeftButton, Qt::NoModifier, tabs->tabRect(1).center());
        QCOMPARE(chosen, 1);
        tabs->setFocus(); QTest::keyClick(tabs, Qt::Key_Right); QCOMPARE(chosen, 2);
        QToolButton *split = nullptr, *close = nullptr, *options = nullptr;
        for (auto* b : strip.findChildren<QToolButton*>()) {
            if (b->accessibleName() == "Split view") split = b;
            if (b->accessibleName() == "Close view") close = b;
            if (b->accessibleName() == "View options") options = b;
        }
        QVERIFY(split); QVERIFY(close); QVERIFY(options);
        QCOMPARE(split->menu()->actions().size(), 2);
        auto* side = split->menu()->actions()[0]; side->hover(); QCOMPARE(preview, int(Qt::Horizontal));
        side->trigger(); QCOMPARE(mode, 0); QCOMPARE(direction, Qt::Horizontal); QCOMPARE(preview, 0);
        strip.setView(0);
        split->menu()->actions()[1]->trigger(); QCOMPARE(mode, 1); QCOMPARE(direction, Qt::Vertical);
        strip.canSplit = [](Qt::Orientation d) { return d == Qt::Vertical; };
        QMetaObject::invokeMethod(split->menu(), "aboutToShow");
        QVERIFY(!side->isEnabled()); QVERIFY(split->menu()->actions()[1]->isEnabled());
        strip.setCanClose(true); close->click(); QCOMPARE(closed, 1);
        for (int width : {800, 420, 300, 800, 300}) {
            pages.resize(width, 400); QCoreApplication::processEvents();
            QVERIFY(strip.rect().contains(split->geometry()));
            QVERIFY(strip.rect().contains(close->geometry()));
            QVERIFY(tabs->isVisible());
            for (int i = 0; i < 3; ++i) QVERIFY(tabs->rect().contains(tabs->tabRect(i)));
        }
        QMetaObject::invokeMethod(options->menu(), "aboutToShow");
        options->menu()->actions().last()->trigger(); QCOMPARE(chosen, 3);
        QVERIFY(tabs->isVisible()); QCOMPARE(tabs->currentIndex(), 3);
        // A user can leave diagnostics with the same Structure tab at narrow widths.
        QCoreApplication::processEvents();
        QTest::mouseClick(tabs, Qt::LeftButton, Qt::NoModifier, tabs->tabRect(0).center());
        QCOMPARE(chosen, 0); QVERIFY(!tabs->isTabVisible(3));
        int maximized = 0, kept = 0;
        strip.maximize = [&] { ++maximized; }; strip.keepOnly = [&] { ++kept; };
        QMetaObject::invokeMethod(options->menu(), "aboutToShow");
        for (auto* a : options->menu()->actions()) {
            if (a->text() == "Expand this view" || a->text() == "Show only this view") a->trigger();
        }
        QCOMPARE(maximized, 1); QCOMPARE(kept, 1);
        strip.setCanClose(false);
        QMetaObject::invokeMethod(options->menu(), "aboutToShow");
        QCOMPARE(options->menu()->actions().size(), 1); // No irrelevant layout actions.

    }
};
QTEST_MAIN(TestPaneLayout)
#include "test_pane_layout.moc"
