kcoreaddons_add_plugin(pethumbnail INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/thumbcreator")

target_sources(pethumbnail PRIVATE
    dib.cc
    exe.cc
    exethumb.cc
    exeutil.cc
    ne.cc
    pe.cc
    resource.cc
)

target_link_libraries(pethumbnail
    KF${QT_MAJOR_VERSION}::KIOGui
    Qt::Core
)
