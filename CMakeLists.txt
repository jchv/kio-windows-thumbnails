cmake_minimum_required(VERSION 3.16)
project(kio-windows-thumbnails)

set(QT_MIN_VERSION "5.15.2")
set(KF_MIN_VERSION "5.92.0")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(ECMOptionalAddSubdirectory)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)
include(ECMDeprecationSettings)

find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Gui)
find_package(KF${QT_MAJOR_VERSION} ${KF_MIN_VERSION} REQUIRED COMPONENTS KIO)
add_definitions(-DQT_USE_QSTRINGBUILDER)

install(FILES io.jchw.kio-windows-thumbnails.metainfo.xml
        DESTINATION ${KDE_INSTALL_METAINFODIR})

add_subdirectory(exe)
