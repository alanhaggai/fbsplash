#!/bin/bash

. /sbin/functions.sh
. ver-gentoo
. config

if [[ "$1" == "minor" ]]; then
	major=$(($major+1))
	minor=0
	sub=0
elif [[ "$1" == "minor" ]]; then
	minor=$(($minor+1))
else 
	sub=$(($sub+1))
fi

ebegin Exporting data from repository
svn export file:///devel/repos/splashutils/gentoo >/dev/null
eend $?

ver=${major}.${minor}.${sub}
mv gentoo "splashutils-gentoo-${ver}"

ebegin Creating a tarball
tar cf - "splashutils-gentoo-${ver}" | bzip2 -f > "splashutils-gentoo-${ver}.tar.bz2"
eend $?

ebegin Removing the working copy
rm -rf "splashutils-gentoo-${ver}"
eend $?

ebegin Updating version data
echo "major=${major}" > ver-gentoo
echo "minor=${minor}" >> ver-gentoo
echo "sub=${sub}" >> ver-gentoo
eend $?

ebegin Copying the tarball to dev.gentoo.org
scp splashutils-gentoo-${ver}.tar.bz2 spock@dev.gentoo.org:/home/spock
eend $?

ebegin Copying the tarball to the SDS
cp splashutils-gentoo-${ver}.tar.bz2 ${SDSROOT}/projects/gensplash/archive
eend $?
