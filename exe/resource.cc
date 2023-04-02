#include "resource.h"

#include <QDataStream>

QDataStream &operator>>(QDataStream &s, RtGroupIconDirectory &v) {
    s >> v.reserved >> v.type >> v.count;
    return s;
}

QDataStream &operator>>(QDataStream &s, RtGroupIconDirectoryEntry &v) {
    s >> v.width >> v.height >> v.colorCount >> v.reserved
      >> v.numPlanes >> v.bpp >> v.size >> v.resourceId;
    return s;
}

QVector<RtGroupIconDirectoryEntry> readResourceDirectory(QDataStream &s) {
    RtGroupIconDirectory header;
    s >> header;

    QVector<RtGroupIconDirectoryEntry> result;
    for (int i = 0; i < header.count; i++) {
        RtGroupIconDirectoryEntry entry;
        s >> entry;
        result.append(entry);
    }

    return result;
}
