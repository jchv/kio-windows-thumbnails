#pragma once
#include "common.h"
#include "exe.h"
#include "resource.h"

#include <QtGlobal>
#include <QMap>
#include <QVector>

class QDataStream;

enum class PeDataDirectoryIndex {
    Resource = 2,
};

struct PeFileHeader {
    quint16 machine;
    quint16 numSections;
    quint32 timestamp;
    quint32 offsetToSymbolTable;
    quint32 numberOfSymbols;
    quint16 sizeOfOptionalHeader;
    quint16 fileCharacteristics;
};

struct PeDataDirectory {
    quint32 virtualAddress;
    quint32 size;
};

struct PeSection {
    char name[8];
    quint32 virtualSize;
    quint32 virtualAddress;
    quint32 sizeOfRawData;
    quint32 pointerToRawData;
    quint32 pointerToRelocs;
    quint32 pointerToLineNums;
    quint16 numRelocs;
    quint16 numLineNums;
    quint32 characteristics;
};

struct PeResourceDirectoryTable {
    quint32 characteristics;
    quint32 timestamp;
    quint16 majorVersion;
    quint16 minorVersion;
    quint16 numNameEntries;
    quint16 numIDEntries;
};

struct PeResourceDirectoryEntry {
    quint32 ordinalOrNameOffset;
    quint32 dataOrSubdirOffset;
};

struct PeResourceDataEntry {
    quint32 dataAddress;
    quint32 size;
    quint32 codepage;
    quint32 reserved;
};

QDataStream &operator>>(QDataStream &s, PeFileHeader &v);
QDataStream &operator>>(QDataStream &s, PeDataDirectory &v);
QDataStream &operator>>(QDataStream &s, PeResourceDirectoryTable &v);
QDataStream &operator>>(QDataStream &s, PeResourceDirectoryEntry &v);
QDataStream &operator>>(QDataStream &s, PeResourceDataEntry &v);

class PortableExecutableResourceReader {
public:
    struct Resource {
        ResourceId id1, id2, id3;
        PeResourceDataEntry entry;
    };

    PortableExecutableResourceReader(QDataStream *ds, DosHeader dosHeader)
        : ds{*ds}, dosHeader{dosHeader} {}

    qint64 addressToOffset(quint32 rva);
    bool seekToAddress(quint32 rva);
    bool parseHeaders();
    bool parseResourcesTree();
    ResourceDir readResourceDataDirectoryEntry();
    PeDataDirectory readDataDirectoryEntry(PeDataDirectoryIndex index);
    bool findIconResource(quint32 ordinal, Resource &out);
    QByteArray readResource(Resource res);
    bool getIconInfo(RtGroupIconDirectoryEntry entry, IconInfo &info);
    QVector<IconInfo> readMainIconGroup();

private:
    QDataStream &ds;

    DosHeader dosHeader;
    PeFileHeader fileHeader;
    bool isPe32Plus;

    QVector<PeSection> sections;
    QMap<ResourceType, QVector<Resource>> resources;
};
