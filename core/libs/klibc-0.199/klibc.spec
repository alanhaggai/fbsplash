Summary: A minimal libc subset for use with initramfs.
Name: klibc
Version: 0.199
Release: 1
License: BSD/GPL
Group: Development/Libraries
URL: http://www.zytor.com/mailman/listinfo/klibc
Source: http://www.kernel.org/pub/linux/libs/klibc-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
BuildRequires: kernel-source >= 2.6.0
Packager: H. Peter Anvin <hpa@zytor.com>
Prefix: /usr
Vendor: Starving Linux Artists

%description
%{name} is intended to be a minimalistic libc subset for use with
initramfs.  It is deliberately written for small size, minimal
entanglement, and portability, not speed.  It is definitely a work in
progress, and a lot of things are still missing.

%package kernheaders
Summary: Kernel headers used during the build of klibc.
Group: Development/Libraries

%description kernheaders
This package contains the set of kernel headers that were required to
build %{name} and the utilities that ship with it.  This may or may
not be a complete enough set to build other programs that link against
%{name}.  If in doubt, use real kernel headers instead.

%package utils
Summary: Small statically-linked utilities built with klibc.
Group: Utilities/System

%description utils

This package contains a collection of programs that are statically
linked against klibc.  These duplicate some of the functionality of a
regular Linux toolset, but are typically much smaller than their
full-function counterparts.  They are intended for inclusion in
initramfs images and embedded systems.

%prep
%setup -q
cp -as /usr/src/linux-`rpm -q kernel-source | tail -1 | cut -d- -f3-` ./linux
make -C linux defconfig ARCH=%{_target_cpu}
make -C linux prepare ARCH=%{_target_cpu}
# Deal with braindamage in RedHat's kernel-source RPM
rm -f linux/include/linux/config.h
cat <<EOF > linux/include/linux/config.h
#ifndef _LINUX_CONFIG_H
#define _LINUX_CONFIG_H

#include <linux/autoconf.h>

#endif
EOF
mkdir -p %{buildroot}

%build
make ARCH=%{_target_cpu}

%install
rm -rf %{buildroot}

dest=%{buildroot}/%{prefix}
lib=$dest/%{_lib}/klibc
inc=$dest/include/klibc
exe=$dest/libexec/klibc
doc=$dest/share/doc/%{name}-%{version}
udoc=$dest/share/doc/%{name}-utils-%{version}

# First, the library.

install -dD -m 755 $lib $inc/kernel $exe $doc $udoc
install -m 755 klibc/klibc.so $lib
install -m 644 klibc/libc.a $lib
install -m 644 klibc/crt0.o $lib
install -m 644 klibc/libc.so.hash $lib
ln $lib/klibc.so $lib/libc.so
ln $lib/klibc.so $lib/klibc-$(cat $lib/libc.so.hash).so

# Next, the generated binaries.
# These are currently static binaries, should we go for shared?

install -m 755 ash/sh $exe
install -m 755 gzip/gzip $exe
ln $exe/gzip $exe/gunzip
ln $exe/gzip $exe/zcat
install -m 755 ipconfig/ipconfig $exe
install -m 755 kinit/kinit $exe
install -m 755 nfsmount/nfsmount $exe
install -m 755 utils/static/* $exe

# The docs.

install -m 444 README $doc
install -m 444 klibc/README $doc/README.klibc
install -m 444 klibc/arch/README $doc/README.klibc.arch

install -m 444 gzip/COPYING $udoc/COPYING.gzip
install -m 444 gzip/README $udoc/README.gzip
install -m 444 ipconfig/README $udoc/README.ipconfig
install -m 444 kinit/README $udoc/README.kinit

# Finally, the include files.

bitsize=$(make --no-print-directory -C klibc bitsize ARCH=%{_target_cpu})
cp --parents $(find klibc/include \( -name CVS -o -name SCCS \) -prune \
    -o -name '*.h' -print) $inc
mv $inc/klibc $inc/klibc.$$
mv $inc/klibc.$$/include/* $inc
mv $inc/bits$bitsize/bitsize $inc
rm -rf $inc/klibc.$$ $inc/bits[0-9]*
pushd klibc/arch/%{_arch}/include
cp --parents -f $(find . \( -name CVS -o -name SCCS \) -prune \
    -o -name '*.h' -print) $inc
popd

# Yeugh.  Find the transitive closure over all kernel headers included
# by klibc, and copy them into place.

find . -name '.*.d' | xargs -r sed -e 's,[ \t][ \t]*,\n,g' | sed -n -e 's,^\.\./linux/include/,,p' | sort | uniq | (cd linux/include && xargs -ri cp --parents '{}' $inc/kernel)

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%docdir %{prefix}/share/doc/%{name}-%{version}
%{prefix}/%{_lib}/klibc
%dir %{prefix}/include/klibc
%{prefix}/include/klibc/*.h
%{prefix}/include/klibc/arpa
%{prefix}/include/klibc/bitsize
%{prefix}/include/klibc/klibc
%{prefix}/include/klibc/net
%{prefix}/include/klibc/netinet
%{prefix}/include/klibc/sys
%{prefix}/share/doc/%{name}-%{version}

%files kernheaders
%defattr(-,root,root,-)
%{prefix}/include/klibc/kernel

%files utils
%defattr(-,root,root,-)
%{prefix}/libexec/klibc
%docdir %{prefix}/share/doc/%{name}-utils-%{version}
%{prefix}/share/doc/%{name}-utils-%{version}

%changelog
* Tue Jul 6 2004 H. Peter Anvin <hpa@zytor.com>
- Update to use kernel-source RPM for the kernel symlink.

* Sat Nov 29 2003 Bryan O'Sullivan <bos@serpentine.com> - 
- Initial build.
