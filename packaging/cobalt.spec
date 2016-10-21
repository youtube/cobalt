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
BuildRequires: pkgconfig(wayland-egl)
BuildRequires: pkgconfig(wayland-client)
BuildRequires: pkgconfig(dlog)

%if "%{?use_public_repo}" == "1"
BuildRequires: pkgconfig(egl)
BuildRequires: pkgconfig(glesv2)
%else
BuildRequires: pkgconfig(gles20)
BuildRequires: pkgconfig(libavcodec)
BuildRequires: pkgconfig(libavformat)
BuildRequires: pkgconfig(libavresample)
BuildRequires: pkgconfig(libavutil)
%endif

%description
Youtube Engine based on Cobalt

%prep
%setup -q

%define _name cobalt
%define _outdir src/out/tizen-armv7l_devel

%build
#gyp generate
./src/cobalt/build/gyp_cobalt -v -C devel tizen-armv7l
#ninja build
src/cobalt/build/ninja/ninja-linux32.armv7l \
-C src/out/tizen-armv7l_devel cobalt

%clean
rm -rf src/out

%install
rm -rf %{buildroot}
install -d %{buildroot}%{_bindir}
install -m 0755 %{_outdir}/%{_name} %{buildroot}%{_bindir}

%post

%postun

%files
%manifest packaging/cobalt.manifest
%defattr(-,root,root,-)
%{_bindir}/%{_name}
