#!/usr/bin/env bash

#    Copyright (C) 2019-2021 Joshua Boudreau <jboudreau@45drives.com>
#    
#    This file is part of autotier.
# 
#    autotier is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
# 
#    autotier is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
# 
#    You should have received a copy of the GNU General Public License
#    along with autotier.  If not, see <https://www.gnu.org/licenses/>.

if [[ "$#" == 1 && "$1" == "clean" ]]; then
	pushd debian
	rm -f autotier.postrm.debhelper autotier.substvars debhelper-build-stamp files
	rm -rf .debhelper autotier
	popd
	rm -rf dist/ubuntu
	exit 0
fi

command -v docker > /dev/null 2>&1 || {
	echo "Please install docker.";
	exit 1;
}

# if docker image DNE, build it
if [[ "$(docker images -q autotier-ubuntu-builder 2> /dev/null)" == "" ]]; then
	docker build -t autotier-ubuntu-builder - < docker/ubuntu
	res=$?
	if [ $res -ne 0 ]; then
		echo "Building docker image failed."
		exit $res
	fi
fi

make clean

mkdir -p dist/ubuntu

# mirror current directory to working directory in container, and mirror dist/ubuntu to .. for deb output
docker run -u $(id -u):$(id -g) -w /home/deb/build -it -v$(pwd):/home/deb/build -v$(pwd)/dist/ubuntu:/home/deb --rm autotier-ubuntu-builder dpkg-buildpackage -us -uc -b
res=$?
if [ $res -ne 0 ]; then
	echo "Packaging failed."
	exit $res
fi

rmdir dist/ubuntu/build

echo "deb is in dist/ubuntu/"

exit 0
