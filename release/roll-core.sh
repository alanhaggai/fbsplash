#!/bin/bash

source /devel/common/functions.sh

TESTING=no
[[ -z "${*/*--testing*/}" && -n "$*" ]] && TESTING=yes

ver=$(grep AC_INIT ../core/configure.ac | sed -e 's/[^,]*, \[//' -e 's/\])//')
cdir=`pwd`

cd ..

ebegin "Creating a tarball"
cd tmp
../core/configure
make dist-bzip2
mv splashutils*.tar.bz2 ${cdir}
eend $?


cd ${cdir}

ebegin "Creating a lite tarball"
cp splashutils-${ver}.tar.bz2 splashutils-lite-${ver}.tar.bz2
rm -f splashutils-lite-${ver}.tar
bunzip2 splashutils-lite-${ver}.tar.bz2
tar --delete --wildcards -f splashutils-lite-${ver}.tar 'splashutils*/libs'
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

