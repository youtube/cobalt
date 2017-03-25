Name:      com.youtube.cobalt
Summary:   Cobalt for Tizen
Version:   0.1
Release:   1
License:   MPL or BSD-3-clause
Source0:   %{name}-%{version}.tar.gz

Requires: /usr/bin/systemctl
Requires(post): /sbin/ldconfig
Requires(post): xkeyboard-config
Requires(postun): /sbin/ldconfig

#####################
# rpm require package
#####################
BuildRequires: cmake, bison, python, gperf
BuildRequires: libasound-devel
BuildRequires: pkgconfig(aul)
BuildRequires: pkgconfig(capi-appfw-application)
BuildRequires: pkgconfig(capi-media-audio-io)
BuildRequires: pkgconfig(capi-system-info)
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(edbus)
BuildRequires: pkgconfig(elementary)
BuildRequires: pkgconfig(evas)
BuildRequires: pkgconfig(gles20)
BuildRequires: pkgconfig(icu-i18n)
BuildRequires: pkgconfig(libxml-2.0)
BuildRequires: pkgconfig(openssl)
BuildRequires: pkgconfig(tizen-extension-client)
BuildRequires: pkgconfig(wayland-client)
BuildRequires: pkgconfig(wayland-egl)
%if "%{?target}" == "samsungtv"
BuildRequires: pkgconfig(capi-media-player)
BuildRequires: pkgconfig(drmdecrypt)
%else
BuildRequires: pkgconfig(libavcodec)
BuildRequires: pkgconfig(libavformat)
BuildRequires: pkgconfig(libavutil)
%endif

%description
Youtube Engine based on Cobalt

%prep
%setup -q

#define specific parameters
%if "%{?build_type}"
%define _build_type %{build_type}
%else
%define _build_type devel
%endif

%if "%{?target}"
%define _target %{target}
%else
%define _target tizen
%endif

%if "%{?chipset}"
%define _chipset %{chipset}
%else
%define _chipset armv7l
%endif

%define _name cobalt
%define _pkgname com.youtube.cobalt
%define _outdir src/out/%{_target}-%{_chipset}_%{_build_type}
%define _manifestdir /usr/share/packages
%define _pkgdir /usr/apps/%{_pkgname}
%define _bindir %{_pkgdir}/bin
%define _contentdir %{_pkgdir}/content
%define _build_create_debug 0
echo "Name: %{_name} / Target: %{_target} / Chipset: %{_chipset} / Build Type: %{_build_type}"

#####################
# rpm build
#####################
%build
#gyp generate
%if 0%{?nogyp}
echo "Skip GYP"
%else
./src/cobalt/build/gyp_cobalt -v -C %{_build_type} %{_target}-%{_chipset}
%endif
#ninja build
src/cobalt/build/ninja/ninja-linux32.armv7l \
-C %{_outdir} %{_name}

%clean
#Don't delete src/out
#rm -rf src/out

#####################
# rpm install
#####################
%install
rm -rf %{buildroot}
install -d %{buildroot}%{_bindir}
install -d %{buildroot}%{_manifestdir}
install -d %{buildroot}%{_contentdir}/data/fonts/
install -m 0755 %{_outdir}/%{_name} %{buildroot}%{_bindir}
%if "%{?target}" == "samsungtv"
cp -rd %{_outdir}/content/data/fonts/fonts.xml %{buildroot}%{_contentdir}/data/fonts/
%else
cp -rd %{_outdir}/content/data/fonts %{buildroot}%{_contentdir}/data/
%endif
cp -rd %{_outdir}/content/data/ssl %{buildroot}%{_contentdir}/data/
%if %{_build_type} != "gold"
cp -rd %{_outdir}/content/data/web %{buildroot}%{_contentdir}/data/
%endif
cp src/starboard/tizen/packaging/%{_pkgname}.xml %{buildroot}%{_manifestdir}

%post

%postun

#####################
# rpm files
#####################
%files
%manifest src/starboard/tizen/packaging/%{_pkgname}.manifest
%defattr(-,root,root,-)
%{_bindir}/%{_name}
%{_contentdir}/*
%{_manifestdir}/*

