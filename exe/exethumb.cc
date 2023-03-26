#include "exethumb.h"
#include "exeutil.h"

#include <QFile>

#include <KPluginFactory>
#include <kio/thumbnailcreator.h>

K_PLUGIN_CLASS_WITH_JSON(ExeCreator, "exethumbnail.json")

ExeCreator::ExeCreator(QObject *parent, const QVariantList &args)
    : KIO::ThumbnailCreator(parent, args)
{
}

ExeCreator::~ExeCreator() = default;

KIO::ThumbnailResult ExeCreator::create(const KIO::ThumbnailRequest &request)
{
    QFile file{request.url().toLocalFile()};
    if (!file.open(QIODevice::ReadOnly)) {
        return KIO::ThumbnailResult::fail();
    }

    auto result = iconForWindowsExecutable(&file, request.targetSize());
    if (result.isNull()) {
        return KIO::ThumbnailResult::fail();
    }

    return KIO::ThumbnailResult::pass(result);
}

#include "exethumb.moc"
