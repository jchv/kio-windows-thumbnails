kcoreaddons_add_plugin(pethumbnail INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/thumbcreator")

target_sources(pethumbnail PRIVATE
    exethumb.cc
    exeutil.cc
)

target_link_libraries(pethumbnail
    KF${QT_MAJOR_VERSION}::KIOGui
    Qt::Core
)
