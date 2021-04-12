%define        __spec_install_post %{nil}
%define          debug_package %{nil}
%define        __os_install_post %{_dbpath}/brp-compress

Name:           autotier
Version:        1.1.4
Release:        1%{?dist}
Summary:        Tiered FUSE Passthrough Filesystem

License:        GPL-3.0+
URL:            github.com/45drives/autotier/blob/master/README.md
Source0:        %{name}-%{version}.tar.gz

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

%description
A passthrough FUSE filesystem that intelligently moves files
between storage tiers based on frequency of use, file age, and tier fullness.

%prep
%setup -q

%build
make EXTRA_CFLAGS="-DEL8" EXTRA_LIBS="-lz -lzstd -llz4 -lbz2 -lsnappy" -j8

%install
make DESTDIR=%{buildroot} PACKAGING=1 install

%clean
make DESTDIR=%{buildroot} clean

%files
%defattr(-,root,root,-)
%config %{_sysconfdir}/autotier.conf
%{_bindir}/*
/opt/45drives/%{name}/*
%docdir /usr/share/man/man8
%doc /usr/share/man/man8/*
/usr/share/bash-completion/completions/*

%post
groupadd -f autotier

%changelog
* Mon Apr 12 2021 Josh Boudreau <jboudreau@45drives.com> 1.1.4-1
- Tiering of files is automatically triggered if tier is over quota after
  writing to a file. To disable, added config option `Strict Period = true`.

* Thu Apr 08 2021 Josh Boudreau <jboudreau@45drives.com> 1.1.3-1
- First EL8 build.
