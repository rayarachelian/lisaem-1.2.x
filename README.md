------------------------------------------------------------------------------
Lisa Emulator Source Build README                    http://lisaem.sunder.net/
------------------------------------------------------------------------------

# Deprecation Warning

*Note* this code has now moved to: https://github.com/rayarachelian/lisaem
This repo will continue remain here incase someone needs older stable (but deprecated)
versions of 1.2.6.

Versions 1.2.7 and above will continue to accumulate updates at the new location, and once a stable version of 1.2.7 is released, the master branch there will only point to stable releases as is customary, rather than to development releases.

The reason for this change is that the proper lisaem repo was reserved for this change, whose time has now arrived, in order to provide a better code organization, and also to switch to the newer build system.
The source code is effectively the same as in the new repo as of today, but over time, will of course be updated only in the https://github.com/rayarachelian/lisaem repo and not here.

# old status
2019.10.13 Finally got it working on macos X 10.11+ but had to recompile
both wxWidgets and LisaEm with -stdlib=libc++, and LisaEm with -lstdc++.6,
there are still lots of bugs, there are stubs for HQX but this feature is
incomplete so not yet included in the code. I've added a scripts directory
to help the end user be able to build wxWidgets properly. Much more testing
is needed on macos X, and certainly I need to rewrite all the Windows building
code.


2019.09.29 This is a developer grade preview, you can expect tons of bugs and
incomplete features, likely it will turn into a release in a month or two
depending on free time, etc.

This is a bit of a rant here as I've not worked out all the details of all
the moving pieces yet as well as to explain some of the decisions made,
etc.

Note that the Widget code is incomplete, don't attempt to use this with a
2/10 88 ROM as it will fail to work. See the Changelog for descriptions of
what has been worked on.

Some planned features may not make it into 1.2.7 but may be moved to 1.2.8.

The main driver for this release is the impending release of macosX Catalina
which removes 32 bit support, thus forcing a 64 bit binary release of LisaEm

There are also weird issues based on which version of wxWidgets you use.
If you use wxWidgets earlier than 3.12, such as 3.11 or so, you may see
lots of menu issues now that I've switched some of the menu items to Radio
Button rather than checkbox.

However, wx3.1.2 introduces a snag, at least in GTK on HiDPI ("retina" in
Apple-speak) 4K+ displays. Instead of reporting the actual display resolution
it returns 1920x1080p - not sure if this a GTK thing or a wxWidgets thing,
however, wx3.1.1 does not behave this way. Ironically, I went into building
1.2.7 with the goal of addressing HiDPI issues as in 2017 my 2011 17" Macbook
Pro died due to GPU failure, and due to concerns about repairability of the
newer machines which feature glued in batteries, soldered in RAM, soldered
in SSD, non-pro high priced laptops branded as pro, but limited in RAM and
CPU, and other user-unfriendly anti-features, I decided to give up on
Apple Inc. and go to Linux. In doing so I went to a 17" Acer Predator with
a GTX1070 GPU and a wonderful 17" 4K display and 64G of RAM, on which LisaEm
1.2.6 and earlier look very tiny and unusable. So now with wx3.1.2 doing 
weird things with HiDPI mode, I've come full circle. :)


I highly recommend NOT using any wxWidgets that ships with your OS and
instead compiling wx3.1.2 yourself, as they're going to be older and not
as featureful.

I've not YET tested this code on macOS X and Windows, but will do so over
the next few days and release a few binary releases, hopefully before
Apple releases Catalina, though it may wind up being after. Once vetted
through a few beta and release-candidate versions on win10 and macosx, the
final 1.2.7 will be released in binary form as well.

------------------------------------------------------------------------------

Compiling for Linux:

You will need netpbm as well as wxWdigets 2.8.4 installed.

You will want to install/compile wxWidgets without the
shared library option.      

After installing/compiling wxWidgets, ensure that wx-config
is in your path, cd to the source code directly and run

	./build.sh clean build
	./build.sh install  


This will install the lisaem and lisafsh-tool binaries to
/usr/local/bin, and will install sound files to 
/usr/local/share/LisaEm/

If your system has the upx command available, it will
also compress the resulting binary with upx in order to save
space.

------------------------------------------------------------------------------
Here's a script I use for building wxWidgets 3.1.2 and 3.1.1, where I keep
the source code for wxWidgets in /home/ray/code - edit as needed, this is
meant as an example.

#!/bin/bash -x

export VER=3.1.1
export TYPE=gtk
cd /home/ray/code/wxWidgets-${VER} || exit 1
cd wxWidgets-${VER}
mkdir build-${TYPE}
cd    build-${TYPE}
export CFLAGS="-fPIC" CXXFLAGS="-fPIC"
../configure --enable-unicode    --disable-debug        \
             --disable-shared    --without-expat        \
             --disable-richtext  --with-libxpm=builtin  \
             --prefix=/usr/local/wx${VER}-${TYPE} &&    \
             make && sudo make install || exit 2
export PATH=/usr/local/wx${VER}-${TYPE}/bin/:$PATH
wx-config --list

-->8 cut here 8<-------------------------------------------------------------

------------------------------------------------------------------------------
On windows and macos x we'll also want a static build so we don't have to ship
a copy of wxWidgets.

It appears that the IDE I depended on, wxDSGN.sourceforge.net hasn't been
updated since 2011, so I'll need to do something else with cygwin and ming64,
so that may complicate things. I could depend on the Ubuntu subsystem, but then
that's likely limited to just the pro versions of windows and not the home ones
so likely I'll continue to depend on cygwin, will have to see.

------------------------------------------------------------------------------
Compiling for Raspbian:

**NOTE:** the wx2.8 packages do not work with LisaEm, and custom ones are needed.

Download prebuilt wxWidgets (I used 2.9.5) from
http://lisaem.sunder.net/downloads/wxWidgets-2.9.5-gtk-raspbian-2015.09.01.tar.bz2                                                                                                                                                                                           

(If your raspbian matches Raspbian GNU/Linux 7
Linux raspberrypi 3.18.7-v7+ #755 SMP PREEMPT Thu Feb 12 17:20:48 GMT 2015 armv7l GNU/Linux )

or:

Build wxWidgets and LisaEm yourself like so (assuming your user is pi):
(Likely you should use wx3.1.2 and not 2.9.5 - this text is old)

```
  sudo -s
  apt-get install libgtk-3-dev netpbm upx  #upx is optional but will shrink the binary by ~30%
  cd ~
  VER=2.9.5
  TYPE=gtk
  cd wxWidgets-${VER}
  mkdir build-${TYPE}
  cd    build-${TYPE}
  ../configure --enable-unicode --enable-debug --disable-shared --without-expat --without-regexp --disable-richtext          \
             --with-libtiff=builtin --with-libpng=builtin --with-libjpeg=builtin --with-libxpm=builtin --with-zlib=builtin \
             --prefix=/usr/local/wx${VER}-${TYPE} && make && make install
  cd ~pi
  export PATH=/usr/local/wx2.9.5-gtk/bin:$PATH
  export LD_LIBRARY_PATH=/usr/local/wx2.9.5-gtk/lib:/lib:/usr/lib:/usr/local/lib
  cd lisaem-1.2.6.2
  ./build.sh clean --without-static --without-rawbitmap  build install
  cd ~
  chown -R pi ~pi
  ln -s /usr/local/share/LisaEm /usr/local/share/lisaem
```

There's a bug where it warns on startup about wxScroll, you can ignore it.

Once LisaEm is built with --with-static, wxWidgets should no longer be needed.

TODO: convert wxWidgets and LisaEm to proper Rpi .debs
------------------------------------------------------------------------------

Compiling for Mac OS X 10.3 and higher:

You will need to download the source code for wxWidgets 2.8.x
from www.wxwidgets.org.  After extracting it, you'll need
to modify it as follows:


IMPORTANT!

In your wxMac-2.8.0 dir, edit the file

include/wx/mac/carbon/chkconf.h

```
change the line with '#define wxMAC_USE_CORE_GRAPHICS 1' 
to                   '#define wxMAC_USE_CORE_GRAPHICS 0'
```

(Many thanks to Brian Foley for finding this!)

If you do not do this, the display will look very ugly.


You will want to install/compile wxWidgets without the
shared library option.      


	./build.sh clean build

The application will be inside of the source code
directory under ./lisa as LisaEm.app



------------------------------------------------------------------------------

Compiling for win32:

You will need the following:

A fairly full install of the Cygwin environment in order to be
able to run the build.sh script.   Be sure to also install
the netpbm package under Cygwin.  If you would like the build
script to compress the resulting lisaem.EXE, you should also
install the Cygwin UPX package as well.


Download and install the wxDev C++ 6.10 Environment.  Please
download it http://wxdsgn.sourceforge.net/ and install it in 
c:\\wxDev-Cpp

You should then be able to just run build.sh from under the
Cygwin shell after changing into the source code directory.


	./build.sh clean build
	./build.sh install  

The application will be copied to C:\Program Files\LisaEm\

We do not yet have an installer program for LisaEm, but future
versions will support that.


If your system has the upx command available, it will
also compress the resulting binary with upx in order to save
space.

------------------------------------------------------------------------------
