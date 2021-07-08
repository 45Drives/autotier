%global debug_package %{nil}

Name: ::package_name::
Version: ::package_version::
Release: ::package_build_version::%{?dist}
Summary: ::package_description_short::
License: ::package_licence::
URL: ::package_url::
Source0: %{name}-%{version}.tar.gz
BuildArch: ::package_architecture_el::

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

%description
::package_title::
::package_description_long::

%prep
%setup -q

%build
make EXTRA_CFLAGS="-DEL8" EXTRA_LIBS="-lz -lzstd -llz4 -lbz2 -lsnappy" -j$(nproc)

%install
make DESTDIR=%{buildroot} PACKAGING=1 install

%clean
make DESTDIR=%{buildroot} clean

%files
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/autotier.conf
%{_bindir}/*
/opt/45drives/%{name}/*
%docdir /usr/share/man/man8
%doc /usr/share/man/man8/*
/usr/share/bash-completion/completions/*

%post
groupadd -f autotier

%changelog
* Thu Jul 08 2021 Joshua Boudreau <jboudreau@45drives.com> 1.1.6-3
- Add postinst script to add autotier group
* Mon Jul 05 2021 Joshua Boudreau <jboudreau@45drives.com> 1.1.6-2
- first build with auto-packaging
* Wed May 12 2021 Josh Boudreau <jboudreau@45drives.com> 1.1.6-1
- On file conflict, only one file is renamed now, and a bug was fixed where
  the conflicting file's path was wrong in the database.

* Thu May 06 2021 Josh Boudreau <jboudreau@45drives.com> 1.1.5-1
- No longer reports 'cannot open directory' if missing from one of the tiers.
- On file name conflict while moving files between tiers, both files are
  renamed with '.autotier_conflict' and '.autotier_conflict_orig' appended.
- Added `autotier-init-dirs` script to clone directories only across each
  tier for if data is already present before setting up autotier.
- Ensures that every call to open() contains a file mode, fixing a fatal
  bug in Ubuntu.

* Mon Apr 12 2021 Josh Boudreau <jboudreau@45drives.com> 1.1.4-1
- Tiering of files is automatically triggered if tier is over quota after
  writing to a file. To disable, added config option `Strict Period = true`.
- Added disabling of tier period by setting to a negative number so a cron
  job for `autotier oneshot` can be used to trigger tiering.
- `autotier config` now dumps configuration values from memory of mounted
  filesystem rather than just printing contents of file.
- Implemented parallel moving of files while tiering with one thread per
  tier for maximum concurrency.
- Fix bug where readdir was still showing backend autotier files.
- Tiering of files is automatically triggered if tier is out of space, and
  write() call blocks until tiering is done and the write is tried again.
- Added Copy Buffer Size configration parameter to specify block size
  while moving files between tiers.

* Thu Apr 08 2021 Josh Boudreau <jboudreau@45drives.com> 1.1.3-1
- First EL8 build.