#include "dib.h"

#include <QImage>

#include <memory>

namespace {

struct DibScanner {
    QRgb *colorTable;
    int w, h;

    using LineScanner = void (*)(DibScanner *s, uchar *in, QRgb *out);

    template<LineScanner ScanLine>
    bool scan(QIODevice *in, uchar *out, int bpp, int outStride) {
        int inStride = (((w * bpp) + 31) & ~31) / 8;
        std::unique_ptr<uchar[]> buf{new uchar[inStride]};
        for (int y = 0; y < h; y++) {
            if (in->read(reinterpret_cast<char*>(buf.get()), inStride) != inStride) {
                return false;
            }
            ScanLine(this, buf.get(), reinterpret_cast<QRgb *>(out));
            out += outStride;
        }
        return true;
    }
};

template<int bits>
void indexedLine(DibScanner *s, uchar *in, QRgb *out) {
    constexpr int fullByteMask = ~((8 / bits) - 1);
    constexpr int pixelMask = (1 << bits) - 1;

    int x = 0;
    for (int n = s->w & fullByteMask; x < n; in++) {
        for (int i = 8 - bits; i >= 0; i -= bits, x++) {
            *out++ = s->colorTable[(*in >> i) & pixelMask];
        }
    }
    for (int i = 8 - bits; x < s->w; i -= bits, x++) {
        *out++ = s->colorTable[(*in >> i) & pixelMask];
    }
}

void bgr555Line(DibScanner *s, uchar *in, QRgb *out) {
    for (int x = 0; x < s->w; x++, in += 2) {
        int c = (in[0]) | (in[1] << 8);
        *out++ = qRgb(
            (c & 0b0'11111'00000'00000) >> 7,
            (c & 0b0'00000'11111'00000) >> 2,
            (c & 0b0'00000'00000'11111) << 3
        );
    }
}

void bgr888Line(DibScanner *s, uchar *in, QRgb *out) {
    for (int x = 0; x < s->w; x++, in += 3) {
        *out++ = qRgb(in[2], in[1], in[0]);
    }
}

void bgra8888Line(DibScanner *s, uchar *in, QRgb *out) {
    for (int x = 0; x < s->w; x++, in += 4) {
        *out++ = qRgba(in[2], in[1], in[0], in[3]);
    }
}

void maskLine(DibScanner *s, uchar *in, QRgb *out) {
    int x = 0;
    for (int n = s->w & ~7; x < n; in++) {
        for (int i = 8 - 1; i >= 0; i--, x++, out++) {
            if ((*in >> i) & 1) { *out = 0; }
        }
    }
    for (int i = 7; x < s->w; i--, x++, out++) {
        if ((*in >> i) & 1) { *out = 0; }
    }
}

}

int BitmapInfoHeader::colorTableCount() const {
    if (biClrUsed > 0 && biClrUsed <= 256) { return biClrUsed; }
    else if (biBitCount == 4) { return 16; }
    else if (biBitCount == 8) { return 256; }
    return 0;
}

QDataStream &operator>>(QDataStream &s, BitmapInfoHeader &v) {
    s >> v.biSize >> v.biWidth >> v.biHeight
      >> v.biPlanes >> v.biBitCount
      >> v.biCompression >> v.biSizeImage
      >> v.biXPelsPerMeter >> v.biYPelsPerMeter
      >> v.biClrUsed >> v.biClrImportant;
    return s;
}

bool readIconDibBody(QDataStream &s, const BitmapInfoHeader &bi, QImage &image) {
    // We only handle v3. Maybe *very* old executables have older DIBs?
    if (bi.biSize != 40) {
        return false;
    }

    // TODO: investigate if RLE support is needed.
    if (bi.biCompression != 0) {
        return false;
    }

    QIODevice *in = s.device();
    int w = bi.biWidth, h = bi.biHeight, colors = bi.colorTableCount(), bpp = bi.biBitCount;

    // Top-down DIB
    if (h < 0) {
        h = -h;
    }

    // Icons have the height set to double to store the AND mask.
    h /= 2;

    image = QImage{w, h, QImage::Format_ARGB32};
    if (image.isNull()) {
        return false;
    }

    image.setDotsPerMeterX(bi.biXPelsPerMeter);
    image.setDotsPerMeterY(bi.biYPelsPerMeter);

    // Load color table.
    QRgb colorTable[256] = {
        qRgb(0x00, 0x00, 0x00),
        qRgb(0xFF, 0xFF, 0xFF),
    };
    quint8 rgb[4];
    for (int i = 0; i < colors; i++) {
        if (in->read((char *)rgb, sizeof(rgb)) != sizeof(rgb)) {
            return false;
        }
        colorTable[i] = qRgb(rgb[2], rgb[1], rgb[0]);
    }

    uchar *out = image.bits();
    int outStride = image.bytesPerLine();

    // Bottom-up DIB: scan from last line up.
    if (bi.biHeight > 0) {
        out += outStride * (h - 1);
        outStride = -outStride;
    }

    // Scan in XOR mask/main DIB image
    DibScanner scanner{colorTable, w, h};
    switch (bi.biBitCount) {
        case  1: if (!scanner.scan<indexedLine<1>>(in, out, bpp, outStride)) return false; break;
        case  4: if (!scanner.scan<indexedLine<4>>(in, out, bpp, outStride)) return false; break;
        case  8: if (!scanner.scan<indexedLine<8>>(in, out, bpp, outStride)) return false; break;
        case 16: if (!scanner.scan<bgr555Line    >(in, out, bpp, outStride)) return false; break;
        case 24: if (!scanner.scan<bgr888Line    >(in, out, bpp, outStride)) return false; break;
        case 32: if (!scanner.scan<bgra8888Line  >(in, out, bpp, outStride)) return false; break;
    }

    // Scan in AND mask
    if (!scanner.scan<maskLine>(in, out, 1, outStride)) {
        return false;
    }

    return true;
}
