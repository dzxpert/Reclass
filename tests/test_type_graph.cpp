#include <QtTest/QTest>
#include "type_graph.h"

using namespace rcx;
class TestTypeGraph : public QObject {
    Q_OBJECT
    static uint64_t type(NodeTree& t, const QString& name, const QString& kind = "struct") {
        Node n; n.kind = NodeKind::Struct; n.name = name; n.classKeyword = kind;
        return t.nodes[t.addNode(n)].id;
    }
    static void ref(NodeTree& t, uint64_t from, uint64_t to, const QString& name) {
        Node n; n.kind = NodeKind::Pointer64; n.parentId = from; n.refId = to; n.name = name;
        t.addNode(n);
    }
private slots:
    void referencesAndEnums() {
        NodeTree t;
        const auto root = type(t, "Ball", "class"), world = type(t, "World"), state = type(t, "State", "enum");
        ref(t, root, world, "world"); ref(t, root, world, "previousWorld");
        ref(t, world, root, "ball"); ref(t, root, root, "next");
        Node n; n.kind = NodeKind::UInt32; n.parentId = root; n.structTypeName = "State"; n.name = "state"; t.addNode(n);
        auto g = collectTypeGraph(t, root);
        QCOMPARE(g.vertices.size(), 3); QCOMPARE(g.edges.size(), 4);
        QCOMPARE(g.vertices[g.root].id, root);
        QCOMPARE(g.edges[0].fields.size(), 2);
        QCOMPARE(g.vertices[2].kind, QString("enum"));
        QCOMPARE(layoutTypeGraph(g, false).size(), 3);
        for (const auto& e : g.edges)
            QVERIFY(qAbs(g.vertices[e.from].position.x() - g.vertices[e.to].position.x()) <= 400);
    }
    void incomingDisconnectedAndStable() {
        NodeTree t;
        const auto a = type(t, "A"), b = type(t, "B"), c = type(t, "C"), d = type(t, "Isolated");
        ref(t, b, a, "incoming"); ref(t, b, c, "outgoing"); ref(t, c, a, "cycle");
        auto g = collectTypeGraph(t, a);
        QCOMPARE(layoutTypeGraph(g, false).size(), 3);
        const auto first = g.vertices;
        layoutTypeGraph(g, false);
        for (int i = 0; i < 3; ++i) QCOMPARE(first[i].position, g.vertices[i].position);
        QCOMPARE(layoutTypeGraph(g, true).size(), 4);
        for (int i = 0; i < g.vertices.size(); ++i)
            for (int j = i + 1; j < g.vertices.size(); ++j)
                QVERIFY(g.vertices[i].position != g.vertices[j].position);
        QCOMPARE(g.vertices[3].id, d);
    }
    void embeddedAndAmbiguousNames() {
        NodeTree t;
        const auto a = type(t, "A"); type(t, "Duplicate"); type(t, "Duplicate");
        Node n; n.kind = NodeKind::Struct; n.parentId = a; n.name = "embedded"; t.addNode(n);
        n = Node{}; n.kind = NodeKind::Pointer64; n.parentId = a; n.structTypeName = "Duplicate"; t.addNode(n);
        auto g = collectTypeGraph(t, a);
        QCOMPARE(g.vertices.size(), 4); QCOMPARE(g.edges.size(), 1);
        QCOMPARE(layoutTypeGraph(g, false).size(), 2);
    }
    void largeCycleAndEmpty() {
        NodeTree t;
        auto empty = collectTypeGraph(t, 0); QVERIFY(layoutTypeGraph(empty, true).isEmpty());
        QVector<uint64_t> ids;
        for (int i = 0; i < 10000; ++i) ids.append(type(t, QString::number(i)));
        for (int i = 0; i < ids.size(); ++i) ref(t, ids[i], ids[(i + 1) % ids.size()], "next");
        auto g = collectTypeGraph(t, ids[0]);
        QCOMPARE(layoutTypeGraph(g, false).size(), 10000);
        for (const auto& e : g.edges)
            QVERIFY(qAbs(g.vertices[e.from].position.x() - g.vertices[e.to].position.x()) <= 400);
    }
};
QTEST_GUILESS_MAIN(TestTypeGraph)
#include "test_type_graph.moc"
