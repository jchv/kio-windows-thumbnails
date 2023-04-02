#pragma once
#include <QtGlobal>

enum class ResourceType : quint32 {
    Icon = 3,
    GroupIcon = 14,
};

struct RtGroupIconDirectory {
    quint16 reserved;
    quint16 type;
    quint16 count;
};

struct RtGroupIconDirectoryEntry {
    quint8  width;
    quint8  height;
    quint8  colorCount;
    quint8  reserved;
    quint16 numPlanes;
    quint16 bpp;
    quint32 size;
    quint16 resourceId;
};

QDataStream &operator>>(QDataStream &s, RtGroupIconDirectoryEntry &v);
QDataStream &operator>>(QDataStream &s, RtGroupIconDirectory &v);
QVector<RtGroupIconDirectoryEntry> readResourceDirectory(QDataStream &s);
