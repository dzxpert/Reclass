#pragma once
#include "type_graph.h"
#include "panel_search_field.h"
#include "pane_controls.h"
#include "themes/thememanager.h"
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QTimer>
#include <functional>
#include <cmath>

namespace rcx {

inline QPainterPath graphPath(const QVector<QPointF>& points) {
    QPainterPath path;
    if (points.isEmpty()) return path;
    path.moveTo(points.first());
    for (int i = 1; i < points.size(); ++i) path.lineTo(points[i]);
    return path;
}

class TypeGraphCanvas : public QGraphicsView {
public:
    explicit TypeGraphCanvas(QWidget* parent) : QGraphicsView(parent) {
        setScene(new QGraphicsScene(this));
        setFrameShape(QFrame::NoFrame);
        setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
        setDragMode(ScrollHandDrag);
        setTransformationAnchor(AnchorUnderMouse);
        setResizeAnchor(AnchorViewCenter);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setAccessibleName("Type relationship graph");
        setToolTip("Drag to pan. Scroll to zoom. Double-click a type to open it.");
    }
    void zoomBy(qreal factor) {
        autoFit = false;
        const qreal scaleNow = transform().m11();
        const qreal next = qBound(.15, scaleNow * factor, 2.5);
        scale(next / scaleNow, next / scaleNow);
    }
    void fitGraph() {
        autoFit = true;
        if (scene()->items().isEmpty()) return;
        fitInView(scene()->itemsBoundingRect().adjusted(-16, -16, 16, 16), Qt::KeepAspectRatio);
        const qreal s = transform().m11();
        if (s > 1.0) scale(1.0 / s, 1.0 / s);
    }
protected:
    void resizeEvent(QResizeEvent* e) override {
        QGraphicsView::resizeEvent(e);
        if (autoFit) fitGraph();
    }
    void mousePressEvent(QMouseEvent* e) override {
        autoFit = false;
        QGraphicsView::mousePressEvent(e);
    }
    void wheelEvent(QWheelEvent* e) override {
        zoomBy(std::pow(1.0015, e->angleDelta().y()));
        e->accept();
    }
private:
    bool autoFit = true;
};

class TypeGraphCard : public QGraphicsItem {
public:
    TypeGraph::Vertex vertex;
    Theme theme;
    bool root = false, hovered = false;
    std::function<void()> activated;
    TypeGraphCard(const TypeGraph::Vertex& v, bool isRoot, const Theme& t) : vertex(v), theme(t), root(isRoot) {
        setPos(v.position);
        setFlag(ItemIsSelectable);
        setAcceptHoverEvents(true);
        setCursor(Qt::PointingHandCursor);
        setToolTip(v.kind + " " + v.name + "\nDouble-click to open in Structure");
        setZValue(2);
    }
    QRectF boundingRect() const override { return QRectF(-2, -2, 240, 72); }
    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        const auto& t = theme;
        const QRectF frame(0, 0, 236, 68);
        p->setRenderHint(QPainter::Antialiasing, false);
        p->fillRect(frame, isSelected() ? t.selected : hovered ? t.hover : editorPaperColor(t));
        const QColor edge = isSelected() ? t.borderFocused : containerBorderColor(t);
        fillTopDeviceRowOfRect(*p, frame, edge);
        fillBottomDeviceRowOfRect(*p, frame, edge);
        fillLeftDeviceColOfRect(*p, frame, edge);
        fillRightDeviceColOfRect(*p, frame, edge);
        if (root) fillLeftDeviceColsOfRect(*p, frame, kUnderlineRows, t.indHoverSpan);
        QFont name = chromeFont();
        p->setFont(name); p->setPen(t.text);
        p->drawText(QRectF(kGutter, 8, 220, 24), Qt::AlignLeft | Qt::AlignVCenter,
            QFontMetrics(name).elidedText(vertex.name, Qt::ElideRight, 220));
        QFont detail = chromeFont(); detail.setPointSizeF(9);
        p->setFont(detail); p->setPen(t.textDim);
        p->drawText(QRectF(kGutter, 38, 158, 19), Qt::AlignLeft | Qt::AlignVCenter,
            vertex.kind + " / " + QString::number(vertex.members) + (vertex.kind == "enum" ? " values" : " fields"));
        if (root) p->drawText(QRectF(166, 38, 54, 19), Qt::AlignRight | Qt::AlignVCenter, "Current");
    }
protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override { hovered = true; update(); }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override { hovered = false; update(); }
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e) override {
        e->accept(); if (activated) activated();
    }
};

class TypeGraphView : public QWidget {
public:
    // The owner supplies document metadata. No Provider, address, or live reads.
    std::function<const NodeTree*()> treeSource;
    std::function<uint64_t()> rootSource;
    std::function<void(uint64_t)> openType;
    explicit TypeGraphView(QWidget* parent = nullptr) : QWidget(parent) {
        auto* layout = new QVBoxLayout(this);
        layout->setSizeConstraint(QLayout::SetNoConstraint);
        layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0);
        setFont(chromeFont());
        auto* toolbar = new PaneToolbar(this);
        auto* bar = new QHBoxLayout(toolbar);
        bar->setContentsMargins(kGutter, 2, kGutter, 2); bar->setSpacing(kGutter);
        scope = new PaneComboBox(this);
        scope->setFixedHeight(kEditorBarControlHeight);
        scope->addItems({"Related types", "All types"});
        scope->setAccessibleName("Graph scope");
        scope->setToolTip("Show types connected to the current type, or every type in the document");
        bar->addWidget(scope);
        search = new PanelSearchField(QStringLiteral(":/vsicons/search.svg"),
                                      QStringLiteral("Find type"), this);
        search->setAccessibleName("Find a type in the graph");
        search->setFixedHeight(kEditorBarControlHeight);
        search->setMaximumWidth(260);
        search->setMinimumWidth(0);
        search->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        search->setToolTip("Find by name. Press Enter for the next match.");
        bar->addWidget(search, 1); bar->addStretch();
        canvas = new TypeGraphCanvas(this);
        auto* fit = new QToolButton(this);
        fit->setText("Fit graph"); fit->setAccessibleName("Fit graph");
        fit->setToolTip("Fit all visible types in the canvas");
        fit->setFixedHeight(kEditorBarControlHeight); buttons.append(fit); bar->addWidget(fit);
        connect(fit, &QToolButton::clicked, this, [this] { canvas->fitGraph(); });
        layout->addWidget(toolbar); layout->addWidget(canvas);
        auto* footer = new QHBoxLayout;
        footer->setContentsMargins(kGutter, 0, kGutter, 0);
        hint = new QLabel("Double-click a type to open", this);
        hint->setFixedHeight(26);
        hint->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        footer->addWidget(hint, 1);
        count = new QLabel(this); footer->addWidget(count);
        layout->addLayout(footer);
        connect(scope, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { rebuild(true); });
        connect(search, &QLineEdit::textChanged, this, [this] { matchIndex = -1; findNext(); });
        connect(search, &QLineEdit::returnPressed, this, [this] { findNext(); });
        connect(canvas->scene(), &QGraphicsScene::selectionChanged, this, [this] { emphasize(); });
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this](const Theme& t) {
            theme = t;
            for (auto* card : cards) card->theme = t;
            applyTheme(); canvas->scene()->update(); emphasize();
        });
        auto* timer = new QTimer(this); timer->setInterval(300);
        connect(timer, &QTimer::timeout, this, [this] { if (isVisible()) refresh(); }); timer->start();
        applyTheme();
    }
    void refresh() {
        const auto* tree = treeSource ? treeSource() : nullptr;
        if (!tree) return;
        const uint64_t root = rootSource ? rootSource() : 0;
        if (tree->generation() != generation || root != currentRoot) {
            const bool recenter = generation == 0 || root != currentRoot;
            generation = tree->generation(); currentRoot = root; rebuild(recenter);
        }
    }
    void fitGraph() { canvas->fitGraph(); }
    void zoomBy(qreal factor) { canvas->zoomBy(factor); }
    void copyCameraFrom(const TypeGraphView& other) {
        scope->setCurrentIndex(other.scope->currentIndex());
        refresh();
        const auto transform = other.canvas->transform();
        const auto center = other.canvas->mapToScene(other.canvas->viewport()->rect().center());
        QTimer::singleShot(0, this, [this, transform, center] {
            canvas->setTransform(transform); canvas->centerOn(center);
        });
    }
protected:
    void showEvent(QShowEvent* e) override {
        QWidget::showEvent(e); refresh();
        QTimer::singleShot(0, this, [this] {
            layout()->invalidate();
            layout()->activate();
        });
    }
    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        count->setVisible(width() >= 440);
        buttons.first()->setText(width() >= 440 ? "Fit graph" : "Fit");
    }
private:
    TypeGraphCanvas* canvas;
    QComboBox* scope;
    PanelSearchField* search;
    QVector<QToolButton*> buttons;
    QLabel *count, *hint;
    TypeGraph graph;
    QHash<int, TypeGraphCard*> cards;
    struct Wire { int from, to; QGraphicsPathItem* line; QGraphicsPolygonItem* arrow; };
    QVector<Wire> wires;
    quint64 generation = 0;
    uint64_t currentRoot = 0;
    int matchIndex = -1;
    bool rebuilding = false;
    Theme theme = ThemeManager::instance().current();
    void applyTheme() {
        const auto& t = theme;
        canvas->setBackgroundBrush(editorPaperColor(t));
        setObjectName("rcxTypeGraph");
        setStyleSheet(QStringLiteral(
            "QWidget#rcxTypeGraph { background: %1; } QLabel { color: %2; }")
            .arg(editorPaperColor(t).name(), t.textDim.name()));
        const auto font = chromeFont();
        scope->setStyleSheet(paneComboStyle(t, font.family()));
        search->applyTheme(t);
        hint->setFont(font); count->setFont(font);
        for (auto* button : buttons) {
            button->setFont(font);
            button->setStyleSheet(paneToolButtonStyle(t));
        }
    }
    void centerRoot() {
        if (cards.contains(graph.root)) {
            canvas->resetTransform(); canvas->centerOn(cards.value(graph.root));
        }
    }
    void findNext() {
        const QString query = search->text().trimmed();
        if (query.isEmpty()) { canvas->scene()->clearSelection(); return; }
        canvas->scene()->clearSelection();
        for (int step = 1; step <= graph.vertices.size(); ++step) {
            const int i = (matchIndex + step) % graph.vertices.size();
            if (cards.contains(i) && graph.vertices[i].name.contains(query, Qt::CaseInsensitive)) {
                matchIndex = i; canvas->scene()->clearSelection(); cards[i]->setSelected(true);
                canvas->centerOn(cards[i]); return;
            }
        }
    }
    void emphasize() {
        if (rebuilding) return;
        const auto& t = theme;
        int selected = -1;
        for (auto it = cards.cbegin(); it != cards.cend(); ++it) if (it.value()->isSelected()) { selected = it.key(); break; }
        for (const auto& w : wires) {
            const bool active = selected >= 0 && (w.from == selected || w.to == selected);
            const QColor color = active ? t.borderFocused : t.textMuted;
            QPen pen(color, active ? 2.0 : 1.0); pen.setCosmetic(true);
            w.line->setPen(pen); w.arrow->setBrush(color); w.arrow->setPen(Qt::NoPen);
            const qreal opacity = selected < 0 || active ? .9 : .18;
            w.line->setOpacity(opacity); w.arrow->setOpacity(opacity);
        }
    }
    void rebuild(bool fit) {
        const auto* tree = treeSource ? treeSource() : nullptr;
        if (!tree) return;
        const QPointF center = canvas->mapToScene(canvas->viewport()->rect().center());
        rebuilding = true; cards.clear(); wires.clear(); canvas->scene()->clear();
        graph = collectTypeGraph(*tree, rootSource ? rootSource() : 0);
        const auto visible = layoutTypeGraph(graph, scope->currentIndex() == 1);
        for (int i : visible) {
            auto* card = new TypeGraphCard(graph.vertices[i], i == graph.root, theme);
            const uint64_t id = graph.vertices[i].id;
            card->activated = [this, id] { if (openType) openType(id); };
            canvas->scene()->addItem(card); cards.insert(i, card);
        }
        // Allocate separate ports on each side, sorted by opposite endpoint Y.
        QHash<int, QVector<int>> ports;
        QVector<int> drawn;
        QVector<bool> rightFrom(graph.edges.size()), rightTo(graph.edges.size());
        for (int i = 0; i < graph.edges.size(); ++i) {
            const auto& e = graph.edges[i];
            if (!cards.contains(e.from) || !cards.contains(e.to)) continue;
            drawn.append(i);
            const auto a = graph.vertices[e.from].position, b = graph.vertices[e.to].position;
            rightFrom[i] = b.x() >= a.x(); rightTo[i] = a.x() >= b.x();
            ports[e.from * 2 + int(rightFrom[i])].append(i * 2);
            ports[e.to * 2 + int(rightTo[i])].append(i * 2 + 1);
        }
        QHash<int, QPointF> endpoints;
        for (auto it = ports.begin(); it != ports.end(); ++it) {
            auto& entries = it.value();
            std::stable_sort(entries.begin(), entries.end(), [&](int a, int b) {
                const auto& ea = graph.edges[a / 2]; const auto& eb = graph.edges[b / 2];
                return graph.vertices[(a & 1) ? ea.from : ea.to].position.y()
                     < graph.vertices[(b & 1) ? eb.from : eb.to].position.y();
            });
            const auto pos = graph.vertices[it.key() / 2].position;
            for (int j = 0; j < entries.size(); ++j)
                endpoints[entries[j]] = pos + QPointF((it.key() & 1) ? 236 : 0, 12 + 44.0 * (j + 1) / (entries.size() + 1));
        }
        QHash<int, int> laneCounts, nextLane;
        for (int i : drawn) {
            const auto& e = graph.edges[i];
            const int column = qRound(qMin(graph.vertices[e.from].position.x(), graph.vertices[e.to].position.x()) / 400);
            ++laneCounts[column];
        }
        for (int i : drawn) {
            const auto& e = graph.edges[i];
            const QPointF a = endpoints[i * 2], b = endpoints[i * 2 + 1];
            QVector<QPointF> points{a};
            const int column = qRound(qMin(graph.vertices[e.from].position.x(), graph.vertices[e.to].position.x()) / 400);
            const qreal lane = column * 400 + 260 + 116.0 * (++nextLane[column]) / (laneCounts[column] + 1);
            points << QPointF(lane, a.y()) << QPointF(lane, b.y()) << b;
            auto* line = canvas->scene()->addPath(graphPath(points));
            line->setZValue(-1);
            line->setToolTip(graph.vertices[e.from].name + " → " + graph.vertices[e.to].name
                + "\n" + e.fields.join("\n"));
            const qreal sign = rightTo[i] ? 1 : -1;
            QPolygonF arrow; arrow << b << b + QPointF(sign * 7, -3.5) << b + QPointF(sign * 7, 3.5);
            auto* tip = canvas->scene()->addPolygon(arrow); tip->setZValue(1);
            wires.append({e.from, e.to, line, tip});
        }
        count->setText(QStringLiteral("%1 types / %2 links").arg(visible.size()).arg(drawn.size()));
        if (visible.isEmpty()) {
            auto* empty = canvas->scene()->addText("No class, struct, or enum definitions in this document.");
            empty->setDefaultTextColor(theme.textMuted);
        }
        canvas->scene()->setSceneRect(canvas->scene()->itemsBoundingRect().adjusted(-16, -16, 16, 16));
        rebuilding = false; emphasize();
        if (fit) {
            QTimer::singleShot(0, this, [this, large = visible.size() > 40] {
                canvas->fitGraph();
                // Fit after the new pane has its final dimensions.
                if (large && canvas->transform().m11() < .45) centerRoot();
            });
        } else canvas->centerOn(center);
    }
};
} // namespace rcx
