#include "exeutil.h"

#include <QDataStream>
#include <QMap>
#include <QVector>
#include <QImageReader>

#include <optional>

namespace {

constexpr quint16 OPTIONAL_HEADER_MAGIC_PE32 = 0x010b;
constexpr quint16 OPTIONAL_HEADER_MAGIC_PE32_PLUS = 0x020b;
constexpr quint32 SUBDIR_BIT_MASK = 0x80000000;
const QByteArray PNG_SIGNATURE{"\x89PNG\x0D\x0A\x1A\x0A",8};

enum class ResourceType : quint32 {
    Icon = 3,
    GroupIcon = 14,
};

enum class DataDirectory {
    Resource = 2,
};

struct DosHeader {
    char signature[2];
    quint32 newHeaderOffset;

    static DosHeader read(QDataStream &ds) {
        DosHeader result;
        ds.readRawData(result.signature, sizeof(result.signature));
        ds.device()->skip(58);
        ds >> result.newHeaderOffset;
        return result;
    }
};

struct NeFileHeader {
    quint16 offsetOfResourceTable;
    quint16 numberOfResourceSegments;

    static NeFileHeader read(QDataStream &ds) {
        NeFileHeader result;
        ds.device()->skip(34);
        ds >> result.offsetOfResourceTable;
        ds.device()->skip(14);
        ds >> result.numberOfResourceSegments;
        return result;
    }
};

struct NeResource {
    quint16 dataOffsetShifted;
    quint16 dataLength;
    quint16 flags;
    quint16 resourceId;
    quint16 resource[2];

    static NeResource read(QDataStream &ds) {
        NeResource result;
        ds >> result.dataOffsetShifted;
        ds >> result.dataLength;
        ds >> result.flags;
        ds >> result.resourceId;
        ds >> result.resource[0];
        ds >> result.resource[1];
        result.resourceId ^= 0x8000;
        return result;
    }
};

struct NeResourceTable {
    struct Type {
        quint16 typeId;
        quint16 numResources;
        quint16 resource[2];
        QVector<NeResource> resources;

        static std::optional<Type> read(QDataStream &ds) {
            Type result;
            ds >> result.typeId;
            // Sentinel value.
            if (result.typeId == 0) { return {}; }
            ds >> result.numResources;
            ds >> result.resource[0];
            ds >> result.resource[1];
            for (int i = 0; i < result.numResources; i++) {
                result.resources.append(NeResource::read(ds));
            }
            return result;
        }
    };

    quint16 alignmentShiftCount;
    QMap<ResourceType, Type> types;

    static NeResourceTable read(QDataStream &ds) {
        NeResourceTable result;
        ds >> result.alignmentShiftCount;
        while(1) {
            auto type = Type::read(ds);
            if (!type.has_value()) break;
            result.types[ResourceType(type->typeId ^ 0x8000)] = *type;
        }
        return result;
    }
};

struct PeFileHeader {
    quint16 machine;
    quint16 numSections;
    quint32 timestamp;
    quint32 offsetToSymbolTable;
    quint32 numberOfSymbols;
    quint16 sizeOfOptionalHeader;
    quint16 fileCharacteristics;

    static PeFileHeader read(QDataStream &ds) {
        PeFileHeader result;
        ds >> result.machine;
        ds >> result.numSections;
        ds >> result.timestamp;
        ds >> result.offsetToSymbolTable;
        ds >> result.numberOfSymbols;
        ds >> result.sizeOfOptionalHeader;
        ds >> result.fileCharacteristics;
        return result;
    }
};

struct PeDataDirectory {
    quint32 virtualAddress;
    quint32 size;

    static PeDataDirectory read(QDataStream &ds) {
        PeDataDirectory result;
        ds >> result.virtualAddress;
        ds >> result.size;
        return result;
    }
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

    static PeSection read(QDataStream &ds) {
        PeSection result;
        ds.readRawData(result.name, sizeof(result.name));
        ds >> result.virtualSize;
        ds >> result.virtualAddress;
        ds >> result.sizeOfRawData;
        ds >> result.pointerToRawData;
        ds >> result.pointerToRelocs;
        ds >> result.pointerToLineNums;
        ds >> result.numRelocs;
        ds >> result.numLineNums;
        ds >> result.characteristics;
        return result;
    }
};

struct PeResourceDirectoryTable {
    quint32 characteristics;
    quint32 timestamp;
    quint16 majorVersion;
    quint16 minorVersion;
    quint16 numNameEntries;
    quint16 numIDEntries;

    static PeResourceDirectoryTable read(QDataStream &ds) {
        PeResourceDirectoryTable result;
        ds >> result.characteristics;
        ds >> result.timestamp;
        ds >> result.majorVersion;
        ds >> result.minorVersion;
        ds >> result.numNameEntries;
        ds >> result.numIDEntries;
        return result;
    }

    static QString readString(QDataStream &ds) {
        quint16 length;
        ds >> length;
        QString result;
        result.resize(length);
        for (int i = 0; i < length; i++) {
            quint16 codepoint;
            ds >> codepoint;
            result[i] = QChar{codepoint};
        }
        return result;
    }
};

struct PeResourceDirectoryEntry {
    quint32 ordinalOrNameOffset;
    quint32 dataOrSubdirOffset;

    static PeResourceDirectoryEntry read(QDataStream &ds) {
        PeResourceDirectoryEntry result;
        ds >> result.ordinalOrNameOffset;
        ds >> result.dataOrSubdirOffset;
        return result;
    }
};

struct PeResourceDataEntry {
    quint32 dataAddress;
    quint32 size;
    quint32 codepage;
    quint32 reserved;

    static PeResourceDataEntry read(QDataStream &ds) {
        PeResourceDataEntry result;
        ds >> result.dataAddress;
        ds >> result.size;
        ds >> result.codepage;
        ds >> result.reserved;
        return result;
    }
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

    static RtGroupIconDirectoryEntry read(QDataStream &ds) {
        RtGroupIconDirectoryEntry result;
        ds >> result.width;
        ds >> result.height;
        ds >> result.colorCount;
        ds >> result.reserved;
        ds >> result.numPlanes;
        ds >> result.bpp;
        ds >> result.size;
        ds >> result.resourceId;
        return result;
    }
};

struct RtGroupIconDirectory {
    quint16 reserved;
    quint16 type;
    quint16 count;

    static QVector<RtGroupIconDirectoryEntry> readDirectory(QDataStream &ds) {
        QVector<RtGroupIconDirectoryEntry> result;
        RtGroupIconDirectory header;
        ds >> header.reserved;
        ds >> header.type;
        ds >> header.count;

        for (int i = 0; i < header.count; i++) {
            result.append(RtGroupIconDirectoryEntry::read(ds));
        }

        return result;
    }
};

struct BitmapImageHeader {
    quint32 headerSize;
    quint32 width;
    quint32 height;
    quint16 planes;
    quint16 bpp;
    quint32 compression;
    quint32 sizeImage;
    quint32 xppm;
    quint32 yppm;
    quint32 colorsUsed;
    quint32 colorsImportant;

    static BitmapImageHeader read(QDataStream &ds) {
        BitmapImageHeader result;
        ds >> result.headerSize;
        ds >> result.width;
        ds >> result.height;
        ds >> result.planes;
        ds >> result.bpp;
        ds >> result.compression;
        ds >> result.sizeImage;
        ds >> result.xppm;
        ds >> result.yppm;
        ds >> result.colorsUsed;
        ds >> result.colorsImportant;
        return result;
    }
};

struct ResourceId {
    quint32 ordinal;
};

struct Resource {
    ResourceId id1, id2, id3;
    PeResourceDataEntry entry;
};

struct ResourceDir {
    struct Entry {
        ResourceId id;
        quint32 dataOrSubdirOffset;
    };
    QVector<Entry> entries;
};

struct IconInfo {
    QSize size{0, 0};
    int bpp = 0;
    bool png = false;
    int dataOffset = 0;
    int dataLength = 0;
};

int calcPaletteEntries(BitmapImageHeader dibHeader) {
    int paletteEntries = 0;
    if (dibHeader.colorsUsed > 0) { paletteEntries = dibHeader.colorsUsed; }
    else if (dibHeader.bpp == 4) { paletteEntries = 16; }
    else if (dibHeader.bpp == 8) { paletteEntries = 256; }
    return paletteEntries;
}

int calcDibPaletteSize(BitmapImageHeader dibHeader) {
    return calcPaletteEntries(dibHeader) * 4;
}

int calcDibIconImageSize(BitmapImageHeader dibHeader) {
    int bytesPerLine = (((dibHeader.width * dibHeader.bpp) + 31) & ~31) / 8;
    return bytesPerLine * dibHeader.height / 2;
}

QByteArray makeBmpFromRawIcon(QByteArray rawIconData, BitmapImageHeader dibHeader) {
    // This is an ugly hack that lets us utilize code from Qt's own BMP loader.
    // This is inefficient and ugly. Needs to be replaced with a proper DIB
    // loader that supports ICO's particularities.

    QByteArray bmp;
    QDataStream bmpStream{&bmp, QIODevice::ReadWrite};
    bmpStream.setByteOrder(QDataStream::LittleEndian);

    // BMP header
    bmpStream << quint8('B');
    bmpStream << quint8('M');
    bmpStream << quint32(0x0E + 0x28 + rawIconData.size());
    bmpStream << quint16(0);
    bmpStream << quint16(0);
    bmpStream << quint32(0x0E + dibHeader.headerSize + calcDibPaletteSize(dibHeader));

    // DIB header
    bmpStream << quint32(0x28);
    bmpStream << dibHeader.width;
    bmpStream << dibHeader.height / 2;
    bmpStream << dibHeader.planes;
    bmpStream << dibHeader.bpp;
    bmpStream << dibHeader.compression;
    bmpStream << dibHeader.sizeImage;
    bmpStream << dibHeader.xppm;
    bmpStream << dibHeader.yppm;
    bmpStream << dibHeader.colorsUsed;
    bmpStream << dibHeader.colorsImportant;
    bmp.append(rawIconData);

    return bmp;
}

static QImage parseDibIcon(QIODevice *file, IconInfo info) {
    if (!file->seek(info.dataOffset)) { return {}; }

    QImage img;

    int maskBytesPerLine = ((info.size.width() + 31) & ~31) / 8;
    int maskSize = maskBytesPerLine * info.size.height();
    QByteArray maskData;

    if (info.bpp == 32) {
        // Load RGBA data. Qt can't handle this in a BMPv3, so we'll just do it manually.
        img = {info.size, QImage::Format_RGBA8888};
        file->skip(sizeof(BitmapImageHeader));
        file->read(reinterpret_cast<char*>(img.bits()), img.sizeInBytes());
        img = img.mirrored(false, true).rgbSwapped();

        // Load AND mask.
        maskData = file->read(maskSize);
        if (maskData.length() < maskSize) {
            return img;
        }
    } else {
        // Parse header.
        QDataStream ds{file};
        ds.setByteOrder(QDataStream::LittleEndian);
        auto dibHeader = BitmapImageHeader::read(ds);

        // Read raw image data.
        QByteArray rawIconData = file->read(calcDibPaletteSize(dibHeader) + calcDibIconImageSize(dibHeader));

        // Load data as BMP.
        QByteArray bmpData = makeBmpFromRawIcon(rawIconData, dibHeader);
        img.loadFromData(bmpData, "BMP");

        // Load AND mask.
        maskData = file->read(maskSize);
        if (maskData.length() < maskSize) {
            return img;
        }
    }

    QImage mask{
        reinterpret_cast<uchar*>(maskData.data()),
        info.size.width(),
        info.size.height(),
        maskBytesPerLine,
        QImage::Format_Mono
    };
    mask = mask.mirrored(false, true);
    mask.invertPixels();
    img.setAlphaChannel(mask);

    return img;
}

static QImage parseIcon(QIODevice *file, IconInfo info) {
    if (info.dataOffset == 0) return {};

    if (!file->seek(info.dataOffset)) { return {}; }

    if (info.png) {
        QImageReader r{file, "PNG"};
        return r.read();
    }

    return parseDibIcon(file, info);
}

class NewExecutableResourceReader {
public:
    NewExecutableResourceReader(QDataStream *ds, DosHeader dosHeader)
        : ds{*ds}, dosHeader{dosHeader} {}


    bool parseHeaders() {
        // Seek to + verify PE header. We're at the file header after this.
        if (!ds.device()->seek(dosHeader.newHeaderOffset)) {
            return false;
        }

        char signature[2];
        ds.readRawData(signature, sizeof(signature));

        if (signature[0] != 'N' || signature[1] != 'E') {
            return false;
        }

        fileHeader = NeFileHeader::read(ds);
        if (!ds.device()->seek(dosHeader.newHeaderOffset + fileHeader.offsetOfResourceTable)) {
            return false;
        }
        
        resources = NeResourceTable::read(ds);

        return true;
    }

    std::optional<NeResource> iconResource(quint32 ordinal) {
        if (!resources.types.contains(ResourceType::Icon)) { return {}; }
        for (auto res : resources.types[ResourceType::Icon].resources) {
            if (res.resourceId != ordinal) {
                continue;
            }
            return res;
        }
        return {};
    }

    std::optional<IconInfo> getIconInfo(RtGroupIconDirectoryEntry entry) {
        IconInfo result;

        auto resource = iconResource(entry.resourceId);

        if (!resource.has_value()) return {};

        result.dataOffset = resource->dataOffsetShifted << resources.alignmentShiftCount;
        result.dataLength = resource->dataLength;

        if (!ds.device()->seek(result.dataOffset)) { return {}; }

        auto dibHeader = BitmapImageHeader::read(ds);
        result.bpp = dibHeader.bpp;
        result.size = {int(dibHeader.width), int(dibHeader.height / 2)};
        return result;
    }

    QVector<IconInfo> readMainIconGroup() {
        if (!resources.types.contains(ResourceType::GroupIcon)) { return {}; }
        auto entries = resources.types[ResourceType::GroupIcon];
        if (entries.resources.empty()) { return {}; }
        auto res = entries.resources.first(); // App icon should always be first
        if (!ds.device()->seek(res.dataOffsetShifted << resources.alignmentShiftCount)) { return {}; }
        QVector<IconInfo> result;
        for (auto entry : RtGroupIconDirectory::readDirectory(ds)) {
            auto info = getIconInfo(entry);
            if (info.has_value()) {
                result.append(*info);
            }
        }
        return result;
    }

private:
    QDataStream &ds;

    DosHeader dosHeader;
    NeFileHeader fileHeader;
    NeResourceTable resources;
};

class PortableExecutableResourceReader {
public:
    PortableExecutableResourceReader(QDataStream *ds, DosHeader dosHeader)
        : ds{*ds}, dosHeader{dosHeader} {}

    qint64 addressToOffset(quint32 rva) {
        for (int i = 0; i < sections.size(); i++) {
            auto sectionBegin = sections[i].virtualAddress;
            auto sectionEnd = sections[i].virtualAddress + sections[i].sizeOfRawData;
            if (rva >= sectionBegin && rva < sectionEnd) {
                return rva - sectionBegin + sections[i].pointerToRawData;
            }
        }
        return -1;
    }

    bool seekToAddress(quint32 rva) {
        return ds.device()->seek(addressToOffset(rva));
    }

    bool parseHeaders() {
        // Seek to + verify PE header. We're at the file header after this.
        if (!ds.device()->seek(dosHeader.newHeaderOffset)) {
            return false;
        }

        char signature[4];
        ds.readRawData(signature, sizeof(signature));

        if (signature[0] != 'P' || signature[1] != 'E' || signature[2] != 0 || signature[3] != 0) {
            return false;
        }

        fileHeader = PeFileHeader::read(ds);

        // Read optional header magic to determine if this is PE32 or PE32+.
        // We don't really care about most of the optional header.
        quint16 optMagic;
        ds >> optMagic;

        switch (optMagic) {
        case OPTIONAL_HEADER_MAGIC_PE32:
            isPe32Plus = false;
            break;

        case OPTIONAL_HEADER_MAGIC_PE32_PLUS:
            isPe32Plus = true;
            break;

        default:
            return false;
        }

        // We need to read the section table to be able to convert RVAs to file offsets.
        if (!ds.device()->seek(dosHeader.newHeaderOffset + 24 + fileHeader.sizeOfOptionalHeader)) {
            return false;
        }

        for (int i = 0; i < fileHeader.numSections; i++) {
            sections.append(PeSection::read(ds));
        }

        // Read resource tree, too.
        if (!parseResourcesTree()) {
            return false;
        }

        return true;
    }

    bool parseResourcesTree() {
        auto resourceDirectory = readDirectory(DataDirectory::Resource);
        auto resourceOffset = addressToOffset(resourceDirectory.virtualAddress);
        if (resourceOffset < 0) {
            return false;
        }
        if (!ds.device()->seek(resourceOffset)) { return false; }

        auto level1 = readResourceDir();

        for (auto entry1 : level1.entries) {
            {
                // Ignore top-level resources, if any exist.
                if ((entry1.dataOrSubdirOffset & SUBDIR_BIT_MASK) == 0) continue;
                auto subdirOffset = entry1.dataOrSubdirOffset & ~SUBDIR_BIT_MASK;
                if (!ds.device()->seek(resourceOffset + subdirOffset)) { return false; }
            }

            auto resType = ResourceType(entry1.id.ordinal);
            QVector<Resource> resourcesForType;

            // Read subdirectory.
            auto level2 = readResourceDir();

            for (auto entry2 : level2.entries) {
                {
                    // Ignore second-level resources, if any exist.
                    if ((entry2.dataOrSubdirOffset & SUBDIR_BIT_MASK) == 0) continue;
                    auto subdirOffset = entry2.dataOrSubdirOffset & ~SUBDIR_BIT_MASK;
                    if (!ds.device()->seek(resourceOffset + subdirOffset)) { return false; }
                }

                // Read subdirectory.
                auto level3 = readResourceDir();

                for (auto entry3 : level3.entries) {
                    {
                        // Ignore deeper subdirectories.
                        if ((entry3.dataOrSubdirOffset & SUBDIR_BIT_MASK) == SUBDIR_BIT_MASK) continue;
                        auto dataOffset = entry3.dataOrSubdirOffset & ~SUBDIR_BIT_MASK;
                        if (!ds.device()->seek(resourceOffset + dataOffset)) { return false; }
                    }

                    // Read data.
                    auto dataEntry = PeResourceDataEntry::read(ds);

                    Resource resource;
                    resource.id1.ordinal = entry1.id.ordinal;
                    resource.id2.ordinal = entry2.id.ordinal;
                    resource.id3.ordinal = entry3.id.ordinal;
                    resource.entry = dataEntry;
                    resourcesForType.append(resource);
                }
            }

            resources[resType] = resourcesForType;
        }

        return true;
    }

    ResourceDir readResourceDir() {
        auto table = PeResourceDirectoryTable::read(ds);
        QVector<ResourceDir::Entry> entries;
        for (int i = 0; i < table.numNameEntries; i++) {
            auto entry = PeResourceDirectoryEntry::read(ds);
            entries.append({{entry.ordinalOrNameOffset}, entry.dataOrSubdirOffset});
        }
        for (int i = 0; i < table.numIDEntries; i++) {
            auto entry = PeResourceDirectoryEntry::read(ds);
            entries.append({{entry.ordinalOrNameOffset}, entry.dataOrSubdirOffset});
        }
        return {entries};
    }

    PeDataDirectory readDirectory(DataDirectory index) {
        qint64 dataDirOffset = dosHeader.newHeaderOffset + 0x78 + qint64(index) * 0x8;

        // On PE32+, this is 0x10 bytes further down.
        if (isPe32Plus) { dataDirOffset += 0x10; }

        ds.device()->seek(dataDirOffset);
        return PeDataDirectory::read(ds);
    }

    std::optional<Resource> iconResource(quint32 ordinal) {
        if (!resources.contains(ResourceType::Icon)) { return {}; }
        for (auto res : resources[ResourceType::Icon]) {
            if (res.id2.ordinal != ordinal) {
                continue;
            }
            return res;
        }
        return {};
    }

    QByteArray readResource(Resource res) {
        if (!seekToAddress(res.entry.dataAddress)) { return {}; }
        return ds.device()->read(res.entry.size);
    }

    std::optional<IconInfo> getIconInfo(RtGroupIconDirectoryEntry entry) {
        IconInfo result;

        auto resource = iconResource(entry.resourceId);

        if (!resource.has_value()) return {};

        result.dataOffset = addressToOffset(resource->entry.dataAddress);
        result.dataLength = resource->entry.size;

        if (!ds.device()->seek(result.dataOffset)) { return {}; }

        if (ds.device()->peek(8).startsWith(PNG_SIGNATURE)) {
            QImageReader r{ds.device(), "PNG"};
            result.bpp = 32;
            result.size = r.size();
            result.png = true;
            return result;
        }

        auto dibHeader = BitmapImageHeader::read(ds);
        result.bpp = dibHeader.bpp;
        result.size = {int(dibHeader.width), int(dibHeader.height / 2)};
        result.png = false;
        return result;
    }

    QVector<IconInfo> readMainIconGroup() {
        if (!resources.contains(ResourceType::GroupIcon)) { return {}; }
        auto entries = resources[ResourceType::GroupIcon];
        if (entries.empty()) { return {}; }
        auto res = entries.first(); // App icon should always be first
        if (!seekToAddress(res.entry.dataAddress)) { return {}; }
        QVector<IconInfo> result;
        for (auto entry : RtGroupIconDirectory::readDirectory(ds)) {
            auto info = getIconInfo(entry);
            if (info.has_value()) {
                result.append(*info);
            }
        }
        return result;
    }

private:
    QDataStream &ds;

    DosHeader dosHeader;
    PeFileHeader fileHeader;
    bool isPe32Plus;

    QVector<PeSection> sections;
    QMap<ResourceType, QVector<Resource>> resources;
};

}

QVector<IconInfo> iconsForExecutable(QIODevice *file) {
    QDataStream ds{file};
    ds.setByteOrder(QDataStream::LittleEndian);

    // Read DOS header.
    auto dosHeader = DosHeader::read(ds);

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

QImage iconForWindowsExecutable(QIODevice *file, QSize targetSize)
{
    IconInfo best{};

    for (auto icon : iconsForExecutable(file)) {
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

    return parseIcon(file, best);
}
