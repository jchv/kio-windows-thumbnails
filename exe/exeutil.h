#pragma once
#include <QIODevice>
#include <QImage>

QImage getIconForWindowsExecutable(QIODevice *file, QSize targetSize);
