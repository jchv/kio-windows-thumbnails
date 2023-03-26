#pragma once
#include <KIO/ThumbnailCreator>

class ExeCreator : public KIO::ThumbnailCreator
{
public:
    ExeCreator(QObject *parent, const QVariantList &args);
    ~ExeCreator() override;

    KIO::ThumbnailResult create(const KIO::ThumbnailRequest &request) override;
};
