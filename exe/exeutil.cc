#include "exeutil.h"
#include "dib.h"
#include "ne.h"
#include "pe.h"

#include <QDataStream>
#include <QMap>
#include <QVector>
#include <QImageReader>

#include <optional>
#include <qglobal.h>

namespace {

static QImage parseIcon(QDataStream &s, IconInfo info) {
    if (info.dataOffset == 0) return {};

    if (!s.device()->seek(info.dataOffset)) { return {}; }

    if (info.png) {
        QImageReader r{s.device(), "PNG"};
        return r.read();
    }

    BitmapInfoHeader header;
    s >> header;

    QImage image;
    if (!readIconDibBody(s, header, image)) {
        return {};
    }

    return image;
}

QVector<IconInfo> getIconsForWindowsExecutable(QDataStream &ds) {
    // Read DOS header.
    DosHeader dosHeader;
    ds >> dosHeader;

    // Verify the MZ header.
    if (dosHeader.signature[0] != 'M' || dosHeader.signature[1] != 'Z') {
        return {};
    }

    PortableExecutableResourceReader pe{&ds, dosHeader};
    if (pe.parseHeaders()) {
        return pe.readMainIconGroup();
    }

    NewExecutableResourceReader ne{&ds, dosHeader};
    if (ne.parseHeaders()) {
        return ne.readMainIconGroup();
    }

    return {};
}

}

QImage getIconForWindowsExecutable(QIODevice *file, QSize targetSize) {
    QDataStream ds{file};
    ds.setByteOrder(QDataStream::LittleEndian);

    IconInfo best{};

    for (auto icon : getIconsForWindowsExecutable(ds)) {
        // Always prefer greater bpp
        if (icon.bpp > best.bpp) {
            best = icon;
            continue;
        }

        // Don't consider lower bpp
        if (icon.bpp < best.bpp) {
            continue;
        }

        // Prefer bigger thumbnails only when we don't have one as big as the target size.
        if ((best.size.width() < targetSize.width() &&
             icon.size.width() > best.size.width())) {
            best = icon;
            continue;
        }

        // Prefer smaller thumbnails if they are closer to the target size.
        if ((best.size.width() > targetSize.width() &&
             icon.size.width() >= targetSize.width() &&
             icon.size.width() < best.size.width())) {
            best = icon;
            continue;
        }
    }

    return parseIcon(ds, best);
}
