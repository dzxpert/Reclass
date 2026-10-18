#pragma once
#include "core.h"

namespace rcx {

inline int valueComponentCount(NodeKind kind) {
    switch (kind) {
    case NodeKind::Vec2: return 2;
    case NodeKind::Vec3: return 3;
    case NodeKind::Vec4:
    case NodeKind::Mat4x4: return 4;
    default: return 0;
    }
}

inline uint64_t valueComponentRowAddress(const LineMeta& lm) {
    return lm.offsetAddr + (lm.nodeKind == NodeKind::Mat4x4
        ? uint64_t(qBound(0, lm.subLine, 3)) * 16 : 0);
}

// Columns of the numeric tokens only. Width padding, row labels, brackets
// and separators are presentation, and must never acquire change heat.
inline QVector<ColumnSpan> valueComponentSpans(const QString& text,
                                              ColumnSpan value, NodeKind kind) {
    QVector<ColumnSpan> result;
    const int count = valueComponentCount(kind);
    if (!count || !value.valid) return result;
    const int limit = qMin(value.end, int(text.size()));
    const int open = text.indexOf('[', value.start);
    const int close = text.indexOf(']', open + 1);
    if (open < value.start || open >= limit || close < open || close >= limit)
        return result;
    int start = open + 1;
    for (int i = start; i <= close; ++i) {
        if (i != close && text[i] != ',') continue;
        int first = start, last = i;
        while (first < last && text[first].isSpace()) ++first;
        while (last > first && text[last - 1].isSpace()) --last;
        if (first == last) return {};
        result.append({first, last, true});
        start = i + 1;
    }
    if (result.size() != count) return {};
    return result;
}

} // namespace rcx
