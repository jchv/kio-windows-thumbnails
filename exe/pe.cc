#include "pe.h"

#include "common.h"
#include "dib.h"

#include <QDataStream>
#include <QImageReader>

namespace {

constexpr quint16 OPTIONAL_HEADER_MAGIC_PE32 = 0x010b;
constexpr quint16 OPTIONAL_HEADER_MAGIC_PE32_PLUS = 0x020b;
constexpr quint32 SUBDIR_BIT_MASK = 0x80000000;
const QByteArray PNG_SIGNATURE{"\x89PNG\x0D\x0A\x1A\x0A",8};

}

QDataStream &operator>>(QDataStream &s, PeFileHeader &v) {
    s >> v.machine >> v.numSections >> v.timestamp
      >> v.offsetToSymbolTable >> v.numberOfSymbols
      >> v.sizeOfOptionalHeader >> v.fileCharacteristics;
    return s;
}

QDataStream &operator>>(QDataStream &s, PeDataDirectory &v) {
    s >> v.virtualAddress >> v.size;
    return s;
}

QDataStream &operator>>(QDataStream &s, PeSection &v) {
    s.readRawData(v.name, sizeof(v.name));
    s >> v.virtualSize >> v.virtualAddress
      >> v.sizeOfRawData >> v.pointerToRawData
      >> v.pointerToRelocs >> v.pointerToLineNums
      >> v.numRelocs >> v.numLineNums
      >> v.characteristics;
    return s;
}

QDataStream &operator>>(QDataStream &s, PeResourceDirectoryTable &v) {
    s >> v.characteristics >> v.timestamp
      >> v.majorVersion >> v.minorVersion
      >> v.numNameEntries >> v.numIDEntries;
    return s;
}

QDataStream &operator>>(QDataStream &s, PeResourceDirectoryEntry &v) {
    s >> v.ordinalOrNameOffset >> v.dataOrSubdirOffset;
    return s;
}

QDataStream &operator>>(QDataStream &s, PeResourceDataEntry &v) {
    s >> v.dataAddress >> v.size >> v.codepage >> v.reserved;
    return s;
}

qint64 PortableExecutableResourceReader::addressToOffset(quint32 rva) {
    for (int i = 0; i < sections.size(); i++) {
        auto sectionBegin = sections[i].virtualAddress;
        auto sectionEnd = sections[i].virtualAddress + sections[i].sizeOfRawData;
        if (rva >= sectionBegin && rva < sectionEnd) {
            return rva - sectionBegin + sections[i].pointerToRawData;
        }
    }
    return -1;
}

bool PortableExecutableResourceReader::seekToAddress(quint32 rva) {
    return ds.device()->seek(addressToOffset(rva));
}

bool PortableExecutableResourceReader::parseHeaders() {
    // Seek to + verify PE header. We're at the file header after this.
    if (!ds.device()->seek(dosHeader.newHeaderOffset)) {
        return false;
    }

    char signature[4];
    ds.readRawData(signature, sizeof(signature));

    if (signature[0] != 'P' || signature[1] != 'E' || signature[2] != 0 || signature[3] != 0) {
        return false;
    }

    ds >> fileHeader;

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
        PeSection section;
        ds >> section;
        sections.append(section);
    }

    // Read resource tree, too.
    if (!parseResourcesTree()) {
        return false;
    }

    return true;
}

bool PortableExecutableResourceReader::parseResourcesTree() {
    auto resourceDirectory = readDataDirectoryEntry(PeDataDirectoryIndex::Resource);
    auto resourceOffset = addressToOffset(resourceDirectory.virtualAddress);
    if (resourceOffset < 0) {
        return false;
    }
    if (!ds.device()->seek(resourceOffset)) { return false; }

    auto level1 = readResourceDataDirectoryEntry();

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
        auto level2 = readResourceDataDirectoryEntry();

        for (auto entry2 : level2.entries) {
            {
                // Ignore second-level resources, if any exist.
                if ((entry2.dataOrSubdirOffset & SUBDIR_BIT_MASK) == 0) continue;
                auto subdirOffset = entry2.dataOrSubdirOffset & ~SUBDIR_BIT_MASK;
                if (!ds.device()->seek(resourceOffset + subdirOffset)) { return false; }
            }

            // Read subdirectory.
            auto level3 = readResourceDataDirectoryEntry();

            for (auto entry3 : level3.entries) {
                {
                    // Ignore deeper subdirectories.
                    if ((entry3.dataOrSubdirOffset & SUBDIR_BIT_MASK) == SUBDIR_BIT_MASK) continue;
                    auto dataOffset = entry3.dataOrSubdirOffset & ~SUBDIR_BIT_MASK;
                    if (!ds.device()->seek(resourceOffset + dataOffset)) { return false; }
                }

                // Read data.
                PeResourceDataEntry dataEntry;
                ds >> dataEntry;

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

ResourceDir PortableExecutableResourceReader::readResourceDataDirectoryEntry() {
    PeResourceDirectoryTable table;
    ds >> table;
    QVector<ResourceDir::Entry> entries;
    for (int i = 0; i < table.numNameEntries; i++) {
        PeResourceDirectoryEntry entry;
        ds >> entry;
        entries.append({{entry.ordinalOrNameOffset}, entry.dataOrSubdirOffset});
    }
    for (int i = 0; i < table.numIDEntries; i++) {
        PeResourceDirectoryEntry entry;
        ds >> entry;
        entries.append({{entry.ordinalOrNameOffset}, entry.dataOrSubdirOffset});
    }
    return {entries};
}

PeDataDirectory PortableExecutableResourceReader::readDataDirectoryEntry(PeDataDirectoryIndex index) {
    qint64 dataDirOffset = dosHeader.newHeaderOffset + 0x78 + qint64(index) * 0x8;

    // On PE32+, this is 0x10 bytes further down.
    if (isPe32Plus) { dataDirOffset += 0x10; }

    ds.device()->seek(dataDirOffset);

    PeDataDirectory directory;
    ds >> directory;
    return directory;
}

bool PortableExecutableResourceReader::findIconResource(quint32 ordinal, Resource &out) {
    if (!resources.contains(ResourceType::Icon)) { return false; }

    for (auto res : resources[ResourceType::Icon]) {
        if (res.id2.ordinal == ordinal) {
            out = res;
            return true;
        }
    }

    return false;
}

QByteArray PortableExecutableResourceReader::readResource(Resource res) {
    if (!seekToAddress(res.entry.dataAddress)) { return {}; }
    return ds.device()->read(res.entry.size);
}

bool PortableExecutableResourceReader::getIconInfo(RtGroupIconDirectoryEntry entry, IconInfo &info) {
    Resource resource;
    
    if (!findIconResource(entry.resourceId, resource)) {
        return false;
    }

    info.dataOffset = addressToOffset(resource.entry.dataAddress);
    info.dataLength = resource.entry.size;

    if (!ds.device()->seek(info.dataOffset)) { return false; }

    if (ds.device()->peek(8).startsWith(PNG_SIGNATURE)) {
        QImageReader r{ds.device(), "PNG"};
        info.bpp = 32;
        info.size = r.size();
        info.png = true;
        return true;
    }

    BitmapInfoHeader dibHeader;
    ds >> dibHeader;
    info.bpp = dibHeader.biBitCount;
    info.size = {int(dibHeader.biWidth), int(dibHeader.biHeight / 2)};
    info.png = false;
    return true;
}

QVector<IconInfo> PortableExecutableResourceReader::readMainIconGroup() {
    if (!resources.contains(ResourceType::GroupIcon)) { return {}; }
    auto entries = resources[ResourceType::GroupIcon];
    if (entries.empty()) { return {}; }
    auto res = entries.first(); // App icon should always be first
    if (!seekToAddress(res.entry.dataAddress)) { return {}; }
    QVector<IconInfo> result;
    for (auto entry : readResourceDirectory(ds)) {
        IconInfo info;
        if (getIconInfo(entry, info)) {
            result.append(info);
        }
    }
    return result;
}
