#include "exe.h"

#include <QDataStream>

QDataStream &operator>>(QDataStream &s, DosHeader &v) {
    s.readRawData(v.signature, sizeof(v.signature));
    s.device()->skip(58);
    s >> v.newHeaderOffset;
    return s;
}
