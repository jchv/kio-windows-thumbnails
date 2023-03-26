#pragma once
#include <QIODevice>
#include <QImage>

QImage iconForWindowsExecutable(QIODevice *file, QSize targetSize);
