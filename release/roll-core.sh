#!/bin/bash

source /devel/common/functions.sh

TESTING=no
[[ -z "${*/*--testing*/}" && -n "$*" ]] && TESTING=yes

cdir=`pwd`

ebegin Exporting data from repository
tmpdir=`mktemp -d /tmp/splexp.XXXXXXXXXX`
rmdir ${tmpdir}
cd ..
cg-export ${tmpdir}
eend $?
cd ${tmpdir}

ver=$(grep "^PKG_VERSION" core/Makefile | grep -o '[0-9]\+\.[0-9]\+\S*')
einfo Got version: ${ver}

mv core "splashutils-${ver}"

ebegin "Creating a tarball"
tar cf - "splashutils-${ver}" | bzip2 -f > "${cdir}/splashutils-${ver}.tar.bz2"
eend $?

ebegin "Creating a -lite tarball"
rm -rf splashutils-${ver}/libs/*
tar cf - "splashutils-${ver}" | bzip2 -f > "${cdir}/splashutils-lite-${ver}.tar.bz2"
eend $?

ebegin Removing the working copy
rm -rf ${tmpdir}
eend $?

cd ${cdir}

if [[ ${TESTING} == "no" ]]; then
	ebegin Copying the tarballs to dev.gentoo.org
	scp splashutils-${ver}.tar.bz2 spock@dev.gentoo.org:/home/spock
	scp splashutils-lite-${ver}.tar.bz2 spock@dev.gentoo.org:/home/spock
	eend $?

	ebegin Copying the tarballs to the SDS
	cp splashutils-${ver}.tar.bz2 ${SDS_ROOT}/htdocs/projects/gensplash/archive
	cp splashutils-lite-${ver}.tar.bz2 ${SDS_ROOT}/htdocs/projects/gensplash/archive
	eend $?
fi

