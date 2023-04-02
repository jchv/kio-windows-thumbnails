#include "ne.h"

#include "dib.h"

#include <QDataStream>

QDataStream &operator>>(QDataStream &s, NeFileHeader &v) {
    s.device()->skip(34);
    s >> v.offsetOfResourceTable;
    s.device()->skip(14);
    s >> v.numberOfResourceSegments;
    return s;
}

QDataStream &operator>>(QDataStream &s, NeResource &v) {
    s >> v.dataOffsetShifted 
      >> v.dataLength 
      >> v.flags 
      >> v.resourceId 
      >> v.resource[0] 
      >> v.resource[1];
    v.resourceId ^= 0x8000;
    return s;
}

QDataStream &operator>>(QDataStream &s, NeResourceTable::Type &v) {
    s >> v.typeId;
    if (v.typeId == 0) {
        return s;
    }
    s >> v.numResources >> v.resource[0] >> v.resource[1];
    for (int i = 0; i < v.numResources; i++) {
        NeResource resource;
        s >> resource;
        v.resources.append(resource);
    }
    return s;
}

QDataStream &operator>>(QDataStream &s, NeResourceTable &v) {
    s >> v.alignmentShiftCount;
    while(1) {
        NeResourceTable::Type type;
        s >> type;
        if (!type.typeId) { break; }
        v.types[ResourceType(type.typeId ^ 0x8000)] = type;
    }
    return s;
}

bool NewExecutableResourceReader::parseHeaders() {
    if (!ds.device()->seek(dosHeader.newHeaderOffset)) {
        return false;
    }

    char signature[2];
    ds.readRawData(signature, sizeof(signature));

    if (signature[0] != 'N' || signature[1] != 'E') {
        return false;
    }

    ds >> fileHeader;
    if (!ds.device()->seek(dosHeader.newHeaderOffset + fileHeader.offsetOfResourceTable)) {
        return false;
    }
    
    ds >> resources;

    return true;
}

bool NewExecutableResourceReader::findIconResource(quint32 ordinal, NeResource &out) {
    if (!resources.types.contains(ResourceType::Icon)) { return {}; }
    for (auto resource : resources.types[ResourceType::Icon].resources) {
        if (resource.resourceId == ordinal) {
            out = resource;
            return true;
        }
    }
    return false;
}

bool NewExecutableResourceReader::getIconInfo(RtGroupIconDirectoryEntry entry, IconInfo &info) {
    NeResource resource;
    
    if (!findIconResource(entry.resourceId, resource)) {
        return false;
    }

    info.dataOffset = resource.dataOffsetShifted << resources.alignmentShiftCount;
    info.dataLength = resource.dataLength;

    if (!ds.device()->seek(info.dataOffset)) { return {}; }

    BitmapInfoHeader dibHeader;
    ds >> dibHeader;
    info.bpp = dibHeader.biBitCount;
    info.size = {int(dibHeader.biWidth), int(dibHeader.biHeight / 2)};
    return true;
}

QVector<IconInfo> NewExecutableResourceReader::readMainIconGroup() {
    if (!resources.types.contains(ResourceType::GroupIcon)) { return {}; }
    auto entries = resources.types[ResourceType::GroupIcon];
    if (entries.resources.empty()) { return {}; }
    auto res = entries.resources.first(); // App icon should always be first
    if (!ds.device()->seek(res.dataOffsetShifted << resources.alignmentShiftCount)) { return {}; }
    QVector<IconInfo> result;
    for (auto entry : readResourceDirectory(ds)) {
        IconInfo info;
        if (getIconInfo(entry, info)) {
            result.append(info);
        }
    }
    return result;
}
