Setup For Windows 11 desktop or Windows server 2025 development system
as a Linux KVM virtual machine


On the Linux KVM host:

> sudo -i
# cd /var/lib/libvirt/images
# qemu-img create -f qcow2 win11_dev_system.qcow2 500G
# qemu-img create -f qcow2 win11_dev_system_data_drive.qcow2 500G

# virt-manager

CPUs = 8
Memory = 16384
SATA disk =  var/lib/libvirt/images/win11_dev_system.qcow2
NIC=e1000e

NOTE: Before building change BOOT from BIOS to UEFI

NOTE: For Window 11 TPM is required
Add Hardware = TPM (default settings)

Start install from ISO image mounted to SATA CDROM
The installation ISO images can be downloaded from:
https://www.microsoft.com/en-us/software-download/windows11 
https://www.microsoft.com/en-us/evalcenter/download-windows-server-2025

After the install completes setup your username and password on the
Windows 11 VM. 

Set the computer name:
System->About->Rename this PC

Set up the automatic updates to download and install patches when Available.
Control Panel->System->Windows Update
Install latest patches and reboot. 

For Windows 2025 server:
To add services, programs and features go to Server Manager and click on Add roles and features.
Server Manager->Dashboard->Add roles and features
Role-based or feature-based installation

Check all the roles needed:
Active Directory (all services except rights management)
File and Storage Service (expand and install all other file services)
Network Policy 
Print and Documentation Services 
Remote Desktop Services
Volume Activation Services
Windows Deployment Services

After the initial setup is complete shutdown the VM and add the second
hard drive: 

SATA disk =  var/lib/libvirt/images/win11_dev_system_data_drive.qcow2

Unmount the ISO image from the SATA CDROM and start the Windows 11 VM.

System drive = C:
Optical drive = D:
Data drive = E:

Using a separate data drive for all git repos and build so the drive
can be copied and used on other development VMs. 

On the Window VM install the virtualization guest tools. The latest
Virtio drivers can be downloaded from: 

https://fedorapeople.org/groups/virt/virtio-win/direct-downloads/latest-virtio 

Download and install the following:

virtio-win-guest-tools.exe

On the Windows VM install the following applications need for
development work: 

7zip
https://www.7-zip.org/

Emacs
https://ftp.gnu.org/gnu/emacs/windows/
EMACS profile per user: %USERPROFILE%\AppData\Roaming\.emacs

GIT
https://git-scm.com/install/windows

Firefox
https://www.firefox.com/en-US/

Chrome
https://www.google.com/chrome/dr/download/

On the Windows VM if you need to add any additional ENV paths:

Search->Edit environment variables for your account
User variables for username->Path->Edit->New

Or 

Search->Edit the system environment variables
System Properties->User Profiles->Environment Variables
System Variables->Path->Edit->New

Microsoft Visual Studio Install
On the Windows 11 VM or 2025 server VM install visual studio, free
version (VS Community 2026) 

Reference links:
https://visualstudio.microsoft.com/downloads/
https://visualstudio.microsoft.com/free-developer-offers/ 

Download the Visual Studio 2026 Insiders installer from:

https://visualstudio.microsoft.com/insiders/ 

Run the VS installer and select:

Desktop development with C++
Under installation details select all options

WinUI application development
Under installation details select all options

After installation with the above options there will be 4 command
prompts you can open with all the C/C++ environmental variables set: 

%comspec% /k "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
%comspec% /k "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvarsamd64_x86.bat"
%comspec% /k "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars32.bat"
%comspec% /k "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvarsx86_amd64.bat"

To test the installation:

In a Run dialog paste the vcvars32.bat command prompt to start a build
environment: 

Search->Run
%comspec% /k "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars32.bat"

C:\Windows\System32>mkdir c:\tmp
C:\Windows\System32>cd \tmp

Create a test program for compiler testing:

C:\tmp>notepad testprog.c

#include <stdio.h>

int main() 
{
  printf("Test program\n");
  return 0;
}

C:\tmp>cl testprog.c
C:\tmp>testprog.exe

OpenSSL Build for Windows 

For the OpenSSL build you will need to install Perl for Windows and
NASM if you build OpenSSL with machine language optimizations.  

To install Perl for Windows:

Download the latest portable version strawberry Perl:

https://github.com/StrawberryPerl/Perl-Dist-Strawberry/releases 

Unzip the 64-bit portable package to C:\strawberry-perl

To setup the perl environment in a command prompt window run:

C:\strawberry-perl\portableshell.bat

Install NASM for Windows:

Download the latest release version:

https://www.nasm.us/pub/nasm/releasebuilds/2.16.03

Copy the binary EXE files to:

C:\nasm\bin

Add the NASM bin path to the path for your account:

Search->Edit environment variables for your account
User variables for username->Path->Edit->New->C:\nasm\bin

For the OpenSSL build you will need to build the ZLIB compression library:

https://zlib.net/

For all the build examples this document will reference
E:\3plibs\distrib for the location of source code download packages
and reference E:\3plibs as the build location. 

The examples use a command line version of unzip.exe to unpack the zip
archives. 

To build ZLIB from a Windows command prompt:

E:
E:\>cd 3plibs
E:\3plibs>cd distrib
E:\3plibs\distrib>unzip -d e:\3plibs zlib131.zip
E:\3plibs\distrib>cd e:\3plibs
E:\3plibs>move zlib-1.3.1 zlib32-1.3.1
E:\3plibs>cd zlib32-1.3.1
E:\3plibs\zlib32-1.3.1> "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars32.bat"
E:\3plibs\zlib32-1.3.1>nmake -f win32\Makefile.msc

For the OpenSSL build you will need to download the release you wish
to use from: 

https://github.com/openssl/openssl
https://github.com/openssl/openssl/releases 

In this document we will be using version 3.2.2 to match the RHEL 9
and 10 version. 

To build OpenSSL from a Windows command prompt:

E:
E:\>cd e:\3plibs
E:\3plibs\distrib>unzip -d e:\3plibs openssl-openssl-3.2.2.zip
E:\3plibs\distrib>cd e:\3plibs
E:\3plibs>move openssl-openssl-3.2.2 openssl32-openssl-3.2.2
E:\3plibs>cd openssl32-openssl-3.2.2
E:\3plibs\openssl32-openssl-3.2.2>"C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars32.bat"
E:\3plibs\openssl32-openssl-3.2.2>C:\strawberry-perl\portableshell.bat
E:\3plibs\openssl32-openssl-3.2.2>perl Configure VC-WIN32 --prefix=e:\3plibs\openssl --openssldir=e:\3plibs\openssl --with-zlib-include=e:\3plibs\zlib32-1.3.1 --with-zlib-lib=e:\3plibs\zlib32-1.3.1 enable-fips no-shared
E:\3plibs\openssl32-openssl-3.2.2>perl configdata.pm --dump
E:\3plibs\openssl32-openssl-3.2.2>nmake
E:\3plibs\openssl32-openssl-3.2.2>nmake test

All tests successful.
Files=298, Tests=3164, 978 wallclock secs ( 7.58 usr +  1.17 sys =  8.75 CPU)
Result: PASS

E:\3plibs\openssl32-openssl-3.2.2>nmake install
E:\3plibs\openssl32-openssl-3.2.2>move e:\3plibs\openssl e:\3plibs\openssl32
E:\3plibs\openssl32-openssl-3.2.2>e:\3plibs\openssl32\bin\openssl.exe version
OpenSSL 3.2.2 4 Jun 2024 (Library: OpenSSL 3.2.2 4 Jun 2024)

NOTE: To build a debug version of OpenSSL run the perl configure script with the following option: 
>perl Configure debug-VC-WIN32

For the cross platform GUI library we will be using wxWidgets.

For the wxWidgets build you will need to download the release you with
to use from: 

https://github.com/wxWidgets/wxWidgets
https://github.com/wxWidgets/wxWidgets/releases

In this document we will be using version 3.2.4

E:
E:\>cd 3plibs
E:\3plibs>mkdir e:\3plibs\wxWidgets32-3.2.4
E:\3plibs>cd e:\3plibs\distrib
E:\3plibs\distrib>unzip -d e:\3plibs\wxWidgets32-3.2.4 wxWidgets-3.2.4.zip
E:\3plibs\distrib>cd e:\3plibs\wxWidgets32-3.2.4\build\msw
E:\3plibs\wxWidgets32-3.2.4\build\msw>"C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars32.bat"
E:\3plibs\wxWidgets32-3.2.4\build\msw>nmake -f makefile.vc BUILD=release SHARED=0 UNICODE=1

To test the build:

E:\3plibs\wxWidgets32-3.2.4\build\msw>cd ..\..\samples\minimal
E:\3plibs\wxWidgets32-3.2.4\samples\minimal>nmake -f makefile.vc BUILD=release SHARED=0 UNICODE=1

NOTE: To build a debug version of wxWigets use the following options:

> nmake -f makefile.vc BUILD=debug SHARED=0 UNICODE=1

