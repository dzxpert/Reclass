#pragma once
#include <QSplitter>
#include <QSplitterHandle>
#include <QPainter>
#include "themes/thememanager.h"

namespace rcx {
class PaneDivider : public QSplitterHandle {
public:
    PaneDivider(Qt::Orientation o, QSplitter* p) : QSplitterHandle(o, p), theme(ThemeManager::instance().current()) {
        setMouseTracking(true);
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this](const Theme& t) { theme = t; update(); });
    }
protected:
    void enterEvent(QEnterEvent* e) override { hovered = true; update(); QSplitterHandle::enterEvent(e); }
    void leaveEvent(QEvent* e) override { hovered = false; update(); QSplitterHandle::leaveEvent(e); }
    void paintEvent(QPaintEvent*) override {
        const auto& t = theme;
        QPainter p(this); p.fillRect(rect(), t.background);
        p.setPen(hovered ? t.borderFocused : t.border);
        if (orientation() == Qt::Horizontal) p.drawLine(width()/2, 0, width()/2, height());
        else p.drawLine(0, height()/2, width(), height()/2);
    }
    Theme theme;
    bool hovered = false;
};
class PaneSplitter : public QSplitter {
public:
    explicit PaneSplitter(Qt::Orientation o, QWidget* parent = nullptr) : QSplitter(o, parent) { setHandleWidth(8); setChildrenCollapsible(false); }
protected:
    QSplitterHandle* createHandle() override { return new PaneDivider(orientation(), this); }
};

// Hide siblings along the path, preserving every splitter's exact sizes.
inline void restorePaneLayout(QSplitter* root) {
    auto splitters = root->findChildren<QSplitter*>(); splitters.prepend(root);
    for (auto* sp : splitters) {
        if (!sp->property("paneSavedSizes").isValid()) continue;
        for (int i = 0; i < sp->count(); ++i) sp->widget(i)->show();
    }
    for (auto* sp : splitters) {
        const auto saved = sp->property("paneSavedSizes"); if (!saved.isValid()) continue;
        QList<int> sizes; for (const auto& v : saved.toList()) sizes.append(v.toInt());
        sp->setSizes(sizes); sp->setProperty("paneSavedSizes", QVariant());
    }
}
inline void maximizePaneLeaf(QWidget* leaf, QSplitter* root) {
    restorePaneLayout(root);
    QWidget* child = leaf;
    while (auto* sp = qobject_cast<QSplitter*>(child->parentWidget())) {
        QVariantList saved; for (int size : sp->sizes()) saved.append(size);
        sp->setProperty("paneSavedSizes", saved);
        for (int i = 0; i < sp->count(); ++i) sp->widget(i)->setVisible(sp->widget(i) == child);
        if (sp == root) break;
        child = sp;
    }
}
inline void equalizePaneLayout(QSplitter* root) {
    auto splitters = root->findChildren<QSplitter*>(); splitters.prepend(root);
    for (auto* sp : splitters) { QList<int> sizes; const int available = (sp->orientation() == Qt::Horizontal ? sp->width() : sp->height()) - sp->handleWidth() * (sp->count() - 1); for (int i = 0; i < sp->count(); ++i) sizes.append(qMax(1, available / qMax(1, sp->count()))); sp->setSizes(sizes); }
}
} // namespace rcx

namespace rcx {
// Preserve ancestor proportions: only the chosen leaf becomes a new branch.
inline QSplitter* splitPaneLeaf(QWidget* source, QWidget* added, Qt::Orientation direction) {
    auto* parent = qobject_cast<QSplitter*>(source->parentWidget());
    if (!parent) return nullptr;
    const int index = parent->indexOf(source);
    const auto sizes = parent->sizes();
    auto* branch = new PaneSplitter(direction);
    branch->setChildrenCollapsible(false);
    branch->setHandleWidth(8);
    branch->setStyleSheet(parent->styleSheet());
    parent->replaceWidget(index, branch);
    branch->addWidget(source);
    branch->addWidget(added);
    parent->setSizes(sizes);
    const int span = direction == Qt::Horizontal ? branch->width() : branch->height();
    branch->setSizes({qMax(1, (span - branch->handleWidth()) / 2), qMax(1, (span - branch->handleWidth()) / 2)});
    source->show(); added->show(); branch->show();
    return branch;
}

inline void removePaneLeaf(QWidget* leaf, QSplitter* root) {
    auto* parent = qobject_cast<QSplitter*>(leaf->parentWidget());
    leaf->hide(); leaf->setParent(nullptr); leaf->deleteLater();
    while (parent && parent != root && parent->count() == 1) {
        auto* grand = qobject_cast<QSplitter*>(parent->parentWidget());
        if (!grand) break;
        const int index = grand->indexOf(parent);
        const auto sizes = grand->sizes();
        auto* survivor = parent->widget(0);
        survivor->setParent(nullptr);
        grand->replaceWidget(index, survivor);
        parent->deleteLater(); survivor->show(); grand->setSizes(sizes);
        parent = grand;
    }
}
} // namespace rcx
