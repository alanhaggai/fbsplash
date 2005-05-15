#!/bin/bash

source /devel/common/functions.sh

TESTING=no
[[ -z "${*/*--testing*/}" && -n "$*" ]] && TESTING=yes

ebegin Exporting data from repository
svn export file:///devel/repos/splashutils/core >/dev/null
eend $?

ver=$(grep "^PKG_VERSION" core/Makefile | grep -o '[0-9]\+\.[0-9]\+..*')
einfo Got version: ${ver}

mv core "splashutils-${ver}"

ebegin "Creating a tarball"
tar cf - "splashutils-${ver}" | bzip2 -f > "splashutils-${ver}.tar.bz2"
eend $?

ebegin "Creating a -lite tarball"
rm -rf splashutils-${ver}/libs/*
tar cf - "splashutils-${ver}" | bzip2 -f > "splashutils-lite-${ver}.tar.bz2"
eend $?

ebegin Removing the working copy
rm -rf "splashutils-${ver}"
eend $?

if [[ ${TESTING} == "no" ]]; then
	ebegin Copying the tarball to dev.gentoo.org
	scp splashutils-${ver}.tar.bz2 spock@dev.gentoo.org:/home/spock
	eend $?

	ebegin Copying the tarball to the SDS
	cp splashutils-${ver}.tar.bz2 ${SDS_ROOT}/htdocs/projects/gensplash/archive
	eend $?
fi

