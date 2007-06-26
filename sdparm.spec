%define	name	sdparm
%define	version	0.90
%define	release	1

Summary:	List or change SCSI disk parameters
Name:		%{name}
Version:	%{version}
Release:	%{release}
License:	FreeBSD
Group:		Utilities/System
URL:		http://www.torque.net/sg/sdparm.html
Source0:	http://www.torque.net/sg/p/%{name}-%{version}.tgz
BuildRoot:	%{_tmppath}/%{name}-%{version}-root
Packager:	Douglas Gilbert <dgilbert at interlog dot com>

%description
SCSI disk parameters are held in mode pages. This utility lists or
changes those parameters. Other SCSI devices (or devices that use
the SCSI command set) such as CD/DVD and tape drives may also find
parts of sdparm useful. Requires the linux kernel 2.4 series or later.
In the 2.4 series SCSI generic device names (e.g. /dev/sg0)
must be used. In the 2.6 series other device names may be used as
well (e.g. /dev/sda).

Warning: It is possible (but unlikely) to change SCSI disk settings
that stop of slow it down. Use with care.

%prep

%setup -q

%build

./configure --prefix=%{_prefix} --mandir=%{_mandir}

%install
[ "%{buildroot}" != "/" ] && rm -rf %{buildroot}

make install \
	DESTDIR=$RPM_BUILD_ROOT

%post

%postun

%clean
[ "%{buildroot}" != "/" ] && rm -rf %{buildroot}

%files
%defattr(-,root,root)
%doc ChangeLog INSTALL README CREDITS AUTHORS COPYING
%attr(0755,root,root) %{_bindir}/*
%{_mandir}/man8/*

%changelog
* Mon Apr 18 2005 - dgilbert at interlog dot com
- initial version
  * sdparm-0.90
