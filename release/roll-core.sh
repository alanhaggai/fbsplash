#!/bin/bash

source /devel/common/functions.sh

TESTING=no
[[ -z "${*/*--testing*/}" && -n "$*" ]] && TESTING=yes

cdir=`pwd`

cd ..

ver=$(grep "^PKG_VERSION" core/Makefile | grep -o '[0-9]\+\.[0-9]\+\S*')
einfo Got version: ${ver}

ebegin "Creating a lite tarball"
git archive --prefix=splashutils-${ver}/ HEAD:core/ | tar --delete splashutils-${ver}/libs | bzip2 -f > "${cdir}/splashutils-lite-${ver}.tar.bz2"
eend $?

ebegin "Creating a tarball"
git archive --prefix=splashutils-${ver}/ HEAD:core/ | bzip2 -f > "${cdir}/splashutils-${ver}.tar.bz2"
eend $?

cd ${cdir}

if [[ ${TESTING} == "no" ]]; then
	ebegin Copying the tarballs to dev.gentoo.org
	scp splashutils-${ver}.tar.bz2 spock@dev.gentoo.org:/home/spock
	scp splashutils-lite-${ver}.tar.bz2 spock@dev.gentoo.org:/home/spock
	eend $?

	ebegin Copying the tarballs to the SDS
	cp splashutils-${ver}.tar.bz2 ${SDS_ROOT}/htdocs/projects/fbnsplash/archive
	cp splashutils-lite-${ver}.tar.bz2 ${SDS_ROOT}/htdocs/projects/fbsplash/archive
	eend $?
fi

