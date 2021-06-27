#!/usr/bin/bash

if [[ "$1" == "1" ]] ; then
source ./setenv.sh 1
elif [[ "$1" == "0" ]] ; then
source ./setenv.sh 0
else
  echo "Please pass 1 (64-bit compilation) or 0 (32-bit compilation) as first argument"
  exit
fi
if [[ "$2" == "" ]] ; then
  echo "Please pass number of parallel builds as second argument"
  exit
fi
if [[ "$3" == "" ]] ; then
  echo "Please pass build type as third argument.  D for DEBUG, R for RELEASE, A for ALL"
  exit
fi
if [[ "$3" == "A" ]] ; then
BUILDRELEASE=1
BUILDDEBUG=1
fi
if [[ "$3" == "R" ]] ; then
BUILDRELEASE=1
BUILDDEBUG=0
fi
if [[ "$3" == "D" ]] ; then
BUILDRELEASE=0
BUILDDEBUG=1
fi
BUILDDEPS=1
if [[ "$4" == "N" ]] ; then
BUILDDEPS=0
fi
function check-error {
    if [ $? -ne 0 ]; then
        echo $1
        exit
    fi
}

which nasm > /dev/null 2>&1
check-error 'Please install nasm'

which MSBuild.exe > /dev/null 2>&1
check-error 'Please install/set environment for visual studio 2017'
which python.exe > /dev/null 2>&1
check-error 'Make sure that python.exe is in the PATH. (e.g. cp /usr/bin/python2.7.exe /usr/bin/python.exe)'

# c:\perl should have a copy of strawberry perl portable edition
which /c/perl/perl/bin/perl.exe > /dev/null 2>&1
check-error 'Please install strawberry perl portable edition into c:\perl'
ORIPATH=$PATH
export PATH=/c/perl/perl/bin:$PATH

# echo script lines from now one
#set -v
if [[ "$BUILDDEPS" == "1" ]] ; then

if [[ "$IS64" == "1" ]]; then
	if [[ "$BUILDRELEASE" == "1" ]] ; then
		MSBuild.exe freetype/freetypevc10.sln -t:Build -p:Configuration="Release Multithreaded" -p:Platform=x64 -m:$2
		check-error 'Error compiling freetype'
	fi
	if [[ "$BUILDDEBUG" == "1" ]] ; then
		MSBuild.exe freetype/freetypevc10.sln -t:Build -p:Configuration="Debug Multithreaded" -p:Platform=x64 -m:$2
		check-error 'Error compiling freetype'
	fi
else
	if [[ "$BUILDRELEASE" == "1" ]] ; then
		MSBuild.exe freetype/freetypevc10.sln -t:Build -p:Configuration="Release Multithreaded" -p:Platform=Win32 -m:$2
		check-error 'Error compiling freetype'
	fi
	if [[ "$BUILDDEBUG" == "1" ]] ; then
		MSBuild.exe freetype/freetypevc10.sln -t:Build -p:Configuration="Debug Multithreaded" -p:Platform=Win32 -m:$2
		check-error 'Error compiling freetype'
	fi
fi

if [[ "$BUILDRELEASE" == "1" ]] ; then
	cd openssl

	if [[ "$IS64" == "1" ]]; then

		if [[ ! -d "release64" ]]; then
		  mkdir release64
		fi
		cd release64

		perl ../Configure VC-WIN64A --release
	else

		if [[ ! -d "release32" ]]; then
		  mkdir release32
		fi
		cd release32

		perl ../Configure VC-WIN32 --release
	fi
	check-error 'Error executing perl'

	nmake
	check-error 'Error compiling openssl for release'

	cd ../..
fi

if [[ "$BUILDDEBUG" == "1" ]] ; then
	cd openssl
	if [[ "$IS64" == "1" ]]; then
		if [[ ! -d "debug64" ]]; then
		  mkdir debug64
		fi
		cd debug64
		perl ../Configure VC-WIN64A --debug
	else
		if [[ ! -d "debug32" ]]; then
		  mkdir debug32
		fi
		cd debug32
		perl ../Configure VC-WIN32 --debug
	fi
	check-error 'Error executing perl'

	nmake
	check-error 'Error compiling openssl for debug'

	cd ../..
fi

cd pthreads
if [[ "$BUILDRELEASE" == "1" ]] ; then
	nmake VC-static
	check-error 'Error compiling pthreads for release'
fi
if [[ "$BUILDDEBUG" == "1" ]] ; then
	nmake VC-static-debug
	check-error 'Error compiling pthreads for debug'
fi
cd ..

fi
# fi BUILDDEPS

#reuse the cygwin perl again
export PATH=$ORIPATH


if [[ "$IS64" == "1" ]]; then

if [[ "$BUILDDEPS" == "1" ]] ; then

	if [[ "$BUILDRELEASE" == "1" ]] ; then
		MSBuild.exe tools/mhmake/mhmakevc10.sln -t:Build -p:Configuration=Release -p:Platform=x64 -m:$2
		check-error 'Error compiling mhmake for release'
	fi

	if [[ "$BUILDDEBUG" == "1" ]] ; then
		MSBuild.exe tools/mhmake/mhmakevc10.sln -t:Build -p:Configuration=Debug -p:Platform=x64 -m:$2
		check-error 'Error compiling mhmake for debug'
	fi
fi
	export MHMAKECONF=`cygpath -da .`

	if [[ "$BUILDRELEASE" == "1" ]] ; then
		tools/mhmake/Release64/mhmake.exe -P$2 -C xorg-server MAKESERVER=1
		check-error 'Error compiling vcxsrv for release'
	fi

	if [[ "$BUILDDEBUG" == "1" ]] ; then
		tools/mhmake/Debug64/mhmake -P$2 -C xorg-server MAKESERVER=1 DEBUG=1
		check-error 'Error compiling vcxsrv for debug'
	fi

	cd xorg-server/installer
	./packageall.bat nox86
else
if [[ "$BUILDDEPS" == "1" ]] ; then

	if [[ "$BUILDRELEASE" == "1" ]] ; then
		MSBuild.exe tools/mhmake/mhmakevc10.sln -t:Build -p:Configuration=Release -p:Platform=Win32 -m:$2
		check-error 'Error compiling mhmake for release'
	fi
	if [[ "$BUILDDEBUG" == "1" ]] ; then
		MSBuild.exe tools/mhmake/mhmakevc10.sln -t:Build -p:Configuration=Debug -p:Platform=Win32 -m:$2
		check-error 'Error compiling mhmake for debug'
	fi

	export MHMAKECONF=`cygpath -da .`

	if [[ "$BUILDRELEASE" == "1" ]] ; then
		tools/mhmake/Release/mhmake.exe -P$2 -C xorg-server MAKESERVER=1
		check-error 'Error compiling vcxsrv for release'
	fi
	if [[ "$BUILDDEBUG" == "1" ]] ; then
		tools/mhmake/Debug/mhmake -P$2 -C xorg-server MAKESERVER=1 DEBUG=1
		check-error 'Error compiling vcxsrv for debug'
	fi

	cd xorg-server/installer
	./packageall.bat nox64
fi

fi




