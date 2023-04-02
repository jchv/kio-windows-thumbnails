#pragma once
#include <QtGlobal>

class QDataStream;
class QImage;

struct BitmapInfoHeader {
    quint32 biSize;
    qint32  biWidth;
    qint32  biHeight;
    quint16 biPlanes;
    quint16 biBitCount;
    quint32 biCompression;
    quint32 biSizeImage;
    qint32  biXPelsPerMeter;
    qint32  biYPelsPerMeter;
    quint32 biClrUsed;
    quint32 biClrImportant;

    int colorTableCount() const;
};

QDataStream &operator>>(QDataStream &s, BitmapInfoHeader &v);
bool readIconDibBody(QDataStream &s, const BitmapInfoHeader &bi, QImage &image);
