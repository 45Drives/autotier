%define        __spec_install_post %{nil}
%define          debug_package %{nil}
%define        __os_install_post %{_dbpath}/brp-compress

Name:		autotier
Version:	0.3
Release:	1%{?dist}
Summary:	Automatic storage tiering

License:	GPL+
URL:		github.com/45drives/autotier/blob/master/README.md
Source0:	%{name}-%{version}.tar.gz

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

%description
Automatic storage tiering.
If tier usage is higher than watermark or if the watermark is disabled, 
moves files with an atime older than the configured threshold to a lower 
tier of storage et vice versa if the file is recently accessed.

%prep
%setup -q

%build
# empty

%install
# rm -rf %{buildroot}
mkdir -p  %{buildroot}

# in builddir
cp -a * %{buildroot}

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/%{name}.conf
%{_bindir}/*

%changelog
* Mon Dec 9 2019  Josh Boudreau <jboudreau@45drives.com> 0.3
- First Build
