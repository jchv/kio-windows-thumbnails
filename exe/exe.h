#pragma once
#include <QtGlobal>

class QDataStream;

struct DosHeader {
    char signature[2];
    quint32 newHeaderOffset;
};

QDataStream &operator>>(QDataStream &s, DosHeader &v);
