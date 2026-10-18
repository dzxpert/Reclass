#pragma once
#include "core.h"
#include <QPointF>
#include <algorithm>

namespace rcx {

// A type graph is metadata only: never follows pointers into a Provider.
struct TypeGraph {
    struct Vertex { uint64_t id; QString name, kind; int members; QPointF position; };
    struct Edge { int from, to; QStringList fields; };
    QVector<Vertex> vertices;
    QVector<Edge> edges;
    int root = -1;
};

inline TypeGraph collectTypeGraph(const NodeTree& tree, uint64_t rootId) {
    TypeGraph g;
    QHash<uint64_t, int> types;
    QHash<QString, int> named;
    QSet<QString> ambiguous;
    auto add = [&](const Node& n) {
        const QString name = n.structTypeName.isEmpty() ? n.name : n.structTypeName;
        const int i = g.vertices.size();
        types.insert(n.id, i);
        g.vertices.append({n.id, name.isEmpty() ? QStringLiteral("Unnamed type") : name,
            n.classKeyword.isEmpty() ? QStringLiteral("struct") : n.classKeyword,
            int(n.enumMembers.size()), {}});
        if (!name.isEmpty()) {
            if (named.contains(name)) ambiguous.insert(name);
            else named.insert(name, i);
        }
    };
    // Definitions first. Name-based references are resolved only when unique.
    for (const Node& n : tree.nodes)
        if (!n.parentId && (n.kind == NodeKind::Struct || n.classKeyword == "enum")) add(n);
    for (const Node& n : tree.nodes) {
        if (!n.parentId || n.refId || (n.kind != NodeKind::Struct && n.classKeyword != "enum")) continue;
        if (!n.structTypeName.isEmpty() && named.contains(n.structTypeName)
            && !ambiguous.contains(n.structTypeName)) {
            types.insert(n.id, named.value(n.structTypeName));
        } else add(n);
    }
    auto owner = [&](uint64_t id) {
        QSet<uint64_t> seen;
        while (id && !seen.contains(id)) {
            if (types.contains(id)) return types.value(id);
            seen.insert(id);
            const int ni = tree.indexOfId(id);
            if (ni < 0) break;
            id = tree.nodes[ni].parentId;
        }
        return -1;
    };
    QHash<quint64, int> pairs;
    for (const Node& n : tree.nodes) {
        const int from = owner(n.parentId);
        if (from < 0) continue;
        ++g.vertices[from].members;
        int to = n.refId ? types.value(n.refId, -1) : -1;
        if (to < 0 && types.contains(n.id)) to = types.value(n.id);
        if (to < 0 && !n.structTypeName.isEmpty() && !ambiguous.contains(n.structTypeName))
            to = named.value(n.structTypeName, -1);
        if (to < 0) continue;
        const quint64 key = (quint64(from) << 32) | quint32(to);
        if (!pairs.contains(key)) {
            pairs.insert(key, g.edges.size());
            g.edges.append({from, to, {}});
        }
        auto& fields = g.edges[pairs.value(key)].fields;
        if (!n.name.isEmpty() && !fields.contains(n.name)) fields.append(n.name);
    }
    g.root = types.value(rootId, -1);
    if (g.root < 0 && !g.vertices.isEmpty()) g.root = 0;
    return g;
}

// Rooted breadth-first layers keep every edge within one layer or between
// adjacent layers, allowing routing through empty gutters without crossing cards.
// Alternating barycentric sweeps reduce crossings. Stable ID tie breaks ensure
// repeatable output; cycles need no recursion or force-simulation convergence.
inline QVector<int> layoutTypeGraph(TypeGraph& g, bool allTypes) {
    const int n = g.vertices.size();
    QVector<QVector<int>> neighbors(n);
    for (const auto& e : g.edges) {
        if (e.from == e.to) continue;
        neighbors[e.from].append(e.to);
        neighbors[e.to].append(e.from);
    }
    QVector<int> seeds;
    if (g.root >= 0) seeds.append(g.root);
    for (int i = 0; i < n; ++i) if (i != g.root) seeds.append(i);
    if (seeds.size() > 1) std::sort(seeds.begin() + 1, seeds.end(), [&](int a, int b) {
        return g.vertices[a].name == g.vertices[b].name ? g.vertices[a].id < g.vertices[b].id
            : g.vertices[a].name < g.vertices[b].name;
    });
    QVector<int> depth(n, -1), order(n, 0), visible;
    qreal top = 0;
    for (int seed : seeds) {
        if (depth[seed] >= 0) continue;
        if (!allTypes && !visible.isEmpty()) break;
        QVector<int> queue{seed};
        QVector<QVector<int>> layers(1);
        depth[seed] = 0;
        for (int head = 0; head < queue.size(); ++head) {
            const int v = queue[head];
            layers[depth[v]].append(v);
            for (int w : neighbors[v]) if (depth[w] < 0) {
                depth[w] = depth[v] + 1;
                if (layers.size() <= depth[w]) layers.resize(depth[w] + 1);
                queue.append(w);
            }
        }
        for (auto& layer : layers) {
            std::sort(layer.begin(), layer.end(), [&](int a, int b) { return g.vertices[a].id < g.vertices[b].id; });
            for (int i = 0; i < layer.size(); ++i) order[layer[i]] = i;
        }
        for (int pass = 0; pass < 8; ++pass) {
            const bool forward = !(pass & 1);
            for (int step = 0; step < layers.size(); ++step) {
                const int d = forward ? step : layers.size() - step - 1;
                QHash<int, double> barycenter;
                for (int v : layers[d]) {
                    double sum = 0; int count = 0;
                    for (int w : neighbors[v]) if (depth[w] == d + (forward ? -1 : 1)) {
                        sum += order[w]; ++count;
                    }
                    barycenter[v] = count ? sum / count : order[v];
                }
                std::stable_sort(layers[d].begin(), layers[d].end(), [&](int a, int b) {
                    return barycenter[a] < barycenter[b];
                });
                for (int i = 0; i < layers[d].size(); ++i) order[layers[d][i]] = i;
            }
        }
        int rows = 1;
        for (const auto& layer : layers) rows = qMax(rows, int(layer.size()));
        for (int d = 0; d < layers.size(); ++d)
            for (int i = 0; i < layers[d].size(); ++i) {
                const int v = layers[d][i];
                g.vertices[v].position = QPointF(d * 400.0, top + (i + (rows - layers[d].size()) * .5) * 112.0);
                visible.append(v);
            }
        top += rows * 112.0 + 100;
    }
    return visible;
}
} // namespace rcx
