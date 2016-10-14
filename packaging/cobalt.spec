Name:      cobalt
Summary:   Cobalt for Tizen
Version:   0.1
Release:   1
License:   LGPL-2.1 or BSD-2-Clause
Source0:   %{name}-%{version}.tar.gz

Requires: /usr/bin/systemctl
Requires(post): /sbin/ldconfig
Requires(post): xkeyboard-config
Requires(postun): /sbin/ldconfig

BuildRequires: cmake, bison, python, gperf
BuildRequires: libasound-devel
BuildRequires: pkgconfig(egl) pkgconfig(glesv2)
BuildRequires: pkgconfig(wayland-egl)

%description
Tizen cobalt Engine based on Cobalt

%prep
%setup -q

%build
#gyp generate
./src/cobalt/build/gyp_cobalt -v -C devel tizen-armv7l
#ninja build
src/cobalt/build/ninja/ninja-linux32.armv7l \
-C src/out/tizen-armv7l_devel cobalt

%install

%make_install

%post

%files
%manifest packaging/cobalt.manifest
%defattr(-,root,root,-)
