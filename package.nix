{ self
, mkDerivation
, lib
, extra-cmake-modules
, kio
}:

mkDerivation {
  name = "kio-windows-thumbnails";
  meta = {
    license = [ lib.licenses.lgpl21 ];
    maintainers = [ lib.maintainers.jchw ];
  };
  nativeBuildInputs = [ extra-cmake-modules ];
  buildInputs = [ kio ];
  src = self;
}
