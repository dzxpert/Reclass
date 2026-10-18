#include <QtTest/QTest>
#include "widgets/type_graph_view.h"

using namespace rcx;
class TestTypeGraphView : public QObject {
    Q_OBJECT
private slots:
    void routingAndInteraction() {
        NodeTree tree;
        QVector<uint64_t> ids;
        for (int i = 0; i < 8; ++i) {
            Node n; n.kind = NodeKind::Struct; n.name = QString("Type%1").arg(i);
            ids.append(tree.nodes[tree.addNode(n)].id);
        }
        auto ref = [&](int a, int b) {
            Node n; n.kind = NodeKind::Pointer64; n.parentId = ids[a]; n.refId = ids[b]; n.name = "link";
            tree.addNode(n);
        };
        ref(0, 1); ref(0, 2); ref(0, 3); ref(1, 0); ref(1, 1);
        ref(1, 2); ref(2, 3); ref(3, 4); ref(4, 5); ref(5, 0);
        uint64_t root = ids[0];
        TypeGraphView widget;
        widget.treeSource = [&]() { return &tree; };
        widget.rootSource = [&]() { return root; };
        widget.resize(1000, 700); widget.show(); widget.refresh();
        QCoreApplication::processEvents();
        auto* scopeControl = widget.findChild<QComboBox*>();
        for (auto* button : widget.findChildren<QToolButton*>())
            if (button->accessibleName() == "Fit graph") {
                QVERIFY(widget.rect().contains(QRect(button->mapTo(&widget, QPoint()), button->size())));
                QVERIFY(qAbs(button->geometry().center().y() - scopeControl->geometry().center().y()) <= 1);
            }
        auto* canvas = widget.findChild<QGraphicsView*>(); QVERIFY(canvas);
        QVector<TypeGraphCard*> cards;
        QVector<QGraphicsPathItem*> paths;
        for (auto* item : canvas->scene()->items()) {
            if (auto* c = dynamic_cast<TypeGraphCard*>(item)) cards.append(c);
            if (auto* p = dynamic_cast<QGraphicsPathItem*>(item)) paths.append(p);
        }
        QCOMPARE(cards.size(), 6); QCOMPARE(paths.size(), 10);
        bool hasCurve = false;
        for (auto* wire : paths) {
            const auto path = wire->path();
            for (int i = 0; i < path.elementCount(); ++i) hasCurve |= path.elementAt(i).isCurveTo();
            for (int i = 1; i < 500; ++i) {
                const auto p = path.pointAtPercent(i / 500.0);
                for (const auto* card : cards)
                    QVERIFY2(!QRectF(card->pos() + QPointF(.1, .1), QSizeF(235.8, 67.8)).contains(p), "Wire crossed a card");
            }
        }
        QVERIFY(!hasCurve); // Orthogonal connectors match the square node geometry.
        // A metadata refresh without a structural edit must retain scene items and zoom.
        canvas->scale(.8, .8); const auto transform = canvas->transform();
        widget.refresh(); QCOMPARE(canvas->transform(), transform);
        QVERIFY(canvas->scene()->items().contains(cards.first()));
        auto* search = widget.findChild<QLineEdit*>(); QVERIFY(search);
        search->setText("Type3");
        QCOMPARE(canvas->scene()->selectedItems().size(), 1);
        QCOMPARE(dynamic_cast<TypeGraphCard*>(canvas->scene()->selectedItems().first())->vertex.id, ids[3]);
        widget.findChild<QComboBox*>()->setCurrentIndex(1);
        int total = 0;
        for (auto* item : canvas->scene()->items()) if (dynamic_cast<TypeGraphCard*>(item)) ++total;
        QCOMPARE(total, 8);
        root = ids[4]; widget.refresh();
        int roots = 0;
        for (auto* item : canvas->scene()->items()) if (auto* c = dynamic_cast<TypeGraphCard*>(item))
            if (c->root) { ++roots; QCOMPARE(c->vertex.id, root); }
        QCOMPARE(roots, 1);
        const auto original = ThemeManager::instance().current();
        auto changed = original; changed.background = QColor("#eeeeee");
        ThemeManager::instance().previewTheme(changed);
        QCOMPARE(canvas->backgroundBrush().color(), editorPaperColor(changed));
        ThemeManager::instance().revertPreview();
    }
};
QTEST_MAIN(TestTypeGraphView)
#include "test_type_graph_view.moc"
