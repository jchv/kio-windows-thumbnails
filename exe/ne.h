#pragma once
#include "common.h"
#include "exe.h"
#include "resource.h"

#include <QtGlobal>
#include <QVector>
#include <QMap>

class QDataStream;

struct NeFileHeader {
    quint16 offsetOfResourceTable;
    quint16 numberOfResourceSegments;
};

struct NeResource {
    quint16 dataOffsetShifted;
    quint16 dataLength;
    quint16 flags;
    quint16 resourceId;
    quint16 resource[2];

};

struct NeResourceTable {
    struct Type {
        quint16 typeId;
        quint16 numResources;
        quint16 resource[2];
        QVector<NeResource> resources;
    };

    quint16 alignmentShiftCount;
    QMap<ResourceType, Type> types;
};


QDataStream &operator>>(QDataStream &s, NeFileHeader &v);
QDataStream &operator>>(QDataStream &s, NeResource &v);
QDataStream &operator>>(QDataStream &s, NeResourceTable &v);
QDataStream &operator>>(QDataStream &s, NeResourceTable::Type &v);


class NewExecutableResourceReader {
public:
    NewExecutableResourceReader(QDataStream *ds, DosHeader dosHeader)
        : ds{*ds}, dosHeader{dosHeader} {}

    bool parseHeaders();
    bool findIconResource(quint32 ordinal, NeResource &out);
    bool getIconInfo(RtGroupIconDirectoryEntry entry, IconInfo &info);
    QVector<IconInfo> readMainIconGroup();

private:
    QDataStream &ds;

    DosHeader dosHeader;
    NeFileHeader fileHeader;
    NeResourceTable resources;
};
