# Contributor: Jaroslav Lichtblau <dragonlord@aur.archlinux.org>
# Contributor: Vinay S Shastry <vinayshastry@gmail.com>

pkgname=swftools
pkgver=0.9.3
pkgrel=5
pkgdesc="A collection of SWF manipulation and creation utilities"
arch=('i686' 'x86_64')
url="http://www.swftools.org/"
license=('GPL')
depends=('giflib' 'freeglut' 'lame' 't1lib' 'libjpeg' 'fontconfig' 'xpdf')
makedepends=('bison' 'flex' 'zlib' 'patch')

prepare() {
#  cd ${srcdir}/$pkgname-$pkgver

#  patch -Np0 -i ../giflib-5.1.patch
#  sed -i 's#PrintGifError()#fprintf(stderr, "%s\\n", GifErrorString())#g' src/gif2swf.c
cd ../

}

build() {
cd ../
#  cd ${srcdir}/$pkgname-$pkgver

  ./configure --prefix=/usr
  make -j4
}

package() {
cd ../
#  cd ${srcdir}/$pkgname-$pkgver

  #patch -Np0 -i ${srcdir}/$pkgname-$pkgver.patch

  make prefix=${pkgdir}/usr install

  cd ${pkgdir}/usr/share/$pkgname/swfs
  rm -f default_loader.swf default_viewer.swf
  ln -s tessel_loader.swf default_loader.swf
  ln -s simple_viewer.swf default_viewer.swf
}
