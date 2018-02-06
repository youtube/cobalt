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
BuildRequires: pkgconfig(appcore-common)
BuildRequires: pkgconfig(app-control-api)
BuildRequires: pkgconfig(capi-media-player)
BuildRequires: pkgconfig(drmdecrypt)
BuildRequires: pkgconfig(soc-pq-interface)
BuildRequires: pkgconfig(vconf-internal-keys-tv)
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

%if "%{?bin_name}"
%define _name %{bin_name}
%else
%define _name cobalt
%endif

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
%if "%{_name}" == "all"
install -m 0755 %{_outdir}/accessibility_test %{_outdir}/audio_test %{_outdir}/base_test \
                %{_outdir}/base_unittests %{_outdir}/bindings_sandbox %{_outdir}/bindings_test \
                %{_outdir}/browser_test %{_outdir}/cobalt %{_outdir}/crypto_unittests %{_outdir}/csp_test \
                %{_outdir}/cssom_test %{_outdir}/css_parser_test %{_outdir}/dom_parser_test \
                %{_outdir}/dom_test %{_outdir}/eztime_test %{_outdir}/image_decoder_sandbox \
                %{_outdir}/layout_benchmarks %{_outdir}/layout_test %{_outdir}/layout_tests \
                %{_outdir}/loader_test %{_outdir}/math_test %{_outdir}/media2_sandbox \
                %{_outdir}/media_source_sandbox %{_outdir}/mozjs %{_outdir}/mozjs_engine_test \
                %{_outdir}/mozjs_keyword_header_gen %{_outdir}/mozjs_opcode_length_header_gen \
                %{_outdir}/nb_test %{_outdir}/network_test %{_outdir}/nplb %{_outdir}/nplb_blitter_pixel_tests \
                %{_outdir}/poem_unittests %{_outdir}/renderer_benchmark %{_outdir}/renderer_sandbox \
                %{_outdir}/renderer_test %{_outdir}/render_tree_test %{_outdir}/reuse_allocator_benchmark \
                %{_outdir}/sample_benchmark %{_outdir}/scaling_text_sandbox %{_outdir}/simple_example \
                %{_outdir}/simple_example_test %{_outdir}/snapshot_app_stats %{_outdir}/speech_sandbox \
                %{_outdir}/sql_unittests %{_outdir}/starboard_blitter_example %{_outdir}/starboard_glclear_example \
                %{_outdir}/starboard_window_example %{_outdir}/storage_test %{_outdir}/trace_event_test \
                %{_outdir}/web_animations_test %{_outdir}/webdriver_test %{_outdir}/web_media_player_sandbox \
                %{_outdir}/web_platform_tests %{_outdir}/websocket_test %{_outdir}/xhr_test \
                %{buildroot}%{_bindir}
%else
install -m 0755 %{_outdir}/%{_name} %{buildroot}%{_bindir}
%endif

%if "%{?target}" == "samsungtv"
cp -rd src/third_party/starboard/samsungtv/%{_chipset}/fonts/fonts.xml %{buildroot}%{_contentdir}/data/fonts/
%else
cp -rd %{_outdir}/content/data/fonts %{buildroot}%{_contentdir}/data/
%endif

cp -rd %{_outdir}/content/data/ssl %{buildroot}%{_contentdir}/data/

%if %{_build_type} != "gold"
cp -rd %{_outdir}/content/data/web %{buildroot}%{_contentdir}/data/
%endif

%if %{_name} != "cobalt"
cp -rd %{_outdir}/content/dir_source_root %{buildroot}%{_contentdir}/
%endif

cp src/starboard/contrib/tizen/packaging/%{_pkgname}.xml %{buildroot}%{_manifestdir}

%post

%postun

#####################
# rpm files
#####################
%files
%manifest src/starboard/contrib/tizen/packaging/%{_pkgname}.manifest
%defattr(-,root,root,-)
%if "%{_name}" == "all"
%{_bindir}/*
%else
%{_bindir}/%{_name}
%endif
%{_contentdir}/*
%{_manifestdir}/*

