unameOut="$(uname -s)"

case "${unameOut}" in
    Linux*) CFLAGS="CFLAGS=-fPIC";;
    *)      CFLAGS="";;
esac

./configure --disable-selinux ${CFLAGS}
make installlibvim DESTDIR=$cur__install
