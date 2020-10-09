unameOut="$(uname -s)"

case "${unameOut}" in
    Linux*) CFLAGS="CFLAGS=-fPIC";;
    *)      CFLAGS="";;
esac

./configure ${CFLAGS}
make installapitest DESTDIR=$cur__install
