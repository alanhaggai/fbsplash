#!/bin/bash

source ~/devel/common/functions.sh

TESTING=no
[[ -z "${*/*--testing*/}" && -n "$*" ]] && TESTING=yes

ver=$(grep AC_INIT ../core/configure.ac | sed -e 's/[^,]*, \[//' -e 's/\])//')
cdir=`pwd`

cd ..
rm -rf tmp/*
cd core
git archive --format=tar --prefix=splashutils/ HEAD | (cd ../tmp && tar xf -)

ebegin "Creating a tarball"
cd ../tmp/splashutils
mkdir -p m4
autoreconf -i
./configure
make dist-bzip2
mv splashutils*.tar.bz2 ${cdir}
eend $?

ebegin "Adding Debian data"
cd ${cdir}
tar jxf splashutils-${ver}.tar.bz2
cp -r ../debian splashutils-${ver}
tar cf - "splashutils-${ver}" | bzip2 -9 -f > splashutils-${ver}.tar.bz2
rm -rf splashutils-${ver}
eend $?

ebegin "Creating a lite tarball"
cp splashutils-${ver}.tar.bz2 splashutils-lite-${ver}.tar.bz2
rm -f splashutils-lite-${ver}.tar
bunzip2 splashutils-lite-${ver}.tar.bz2
tar --delete --wildcards -f splashutils-lite-${ver}.tar 'splashutils*/libs/libpng*' 'splashutils*/libs/zlib*' 'splashutils*/libs/jpeg*' 'splashutils*/libs/freetype*'
bzip2 splashutils-lite-${ver}.tar
eend $?

#ebegin "Creating a lite tarball"
#git archive --prefix=splashutils-${ver}/ HEAD:core/ | tar --delete splashutils-${ver}/libs | bzip2 -f > "${cdir}/splashutils-lite-${ver}.tar.bz2"
#eend $?

#ebegin "Creating a tarball"
#git archive --prefix=splashutils-${ver}/ HEAD:core/ | bzip2 -f > "${cdir}/splashutils-${ver}.tar.bz2"
#eend $?

cd ${cdir}

if [[ ${TESTING} == "no" ]]; then
	ebegin Copying the tarballs to dev.gentoo.org
	scp splashutils-${ver}.tar.bz2 spock@dev.gentoo.org:/home/spock
	scp splashutils-lite-${ver}.tar.bz2 spock@dev.gentoo.org:/home/spock
	eend $?

	ebegin Copying the tarballs to the SDS
	cp splashutils-${ver}.tar.bz2 ${SDS_ROOT}/htdocs/projects/fbsplash/archive
	cp splashutils-lite-${ver}.tar.bz2 ${SDS_ROOT}/htdocs/projects/fbsplash/archive
	eend $?
fi

