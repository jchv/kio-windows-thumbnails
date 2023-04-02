#pragma once
#include <QtGlobal>
#include <QSize>
#include <QVector>

struct ResourceId {
    quint32 ordinal;
};

struct ResourceDir {
    struct Entry {
        ResourceId id;
        quint32 dataOrSubdirOffset;
    };
    QVector<Entry> entries;
};

struct IconInfo {
    QSize size{0, 0};
    int bpp = 0;
    bool png = false;
    int dataOffset = 0;
    int dataLength = 0;
};