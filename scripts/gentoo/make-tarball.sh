#!/bin/bash

PN="glibc"
PV=${1%/}
pver=$2

if [[ -z ${PV} ]] ; then
	echo "Usage: $0 glibc-version patchset-version-to-be-created"
	echo "Please read the script before trying to use it :)"
	exit 1
fi

# check that we're in the root of a glibc git repo

if [[ ! -f libc-abis ]] || [[ ! -d .git ]] ; then
	echo "Error: You need to call this script in the main directory of a Gentoo glibc git clone"
	exit 1
fi

# check that we're on a branch gentoo/${PV}

mybranchinfo=$(git status --porcelain -b|grep '^##')
mybranch=$(echo ${mybranchinfo}|sed -e 's:^## ::' -e 's:\.\.\..*$::')
if [[ ! "gentoo/${PV}" == "${mybranch}" ]] ; then
	echo "Error: Your git repository is on the incorrect branch ${mybranch}; should be gentoo/${PV}"
	exit 1
fi

# check that the working directory is clean

mystatusinfo=$(git status --porcelain)
if [[ ! -z "${mystatusinfo}" ]] ; then
	echo "Error: Your working directory is not clean"
	exit 1
fi

# check if the tag already exists

mytaginfo=$(git tag -l|grep "gentoo/glibc-${PV}-${pver}")
if [[ ! -z "${mytaginfo}" ]] ; then
	echo "Error: A tag corresponding to this patch level already exists (gentoo/glibc-${PV}-${pver})"
	exit 1
fi

# luckily glibc git has no /tmp dir and no tar.xz files, but let's better check and be pathologically careful

if [[ -e tmp ]] || [[ -e ${PN}-${PV}-patches-${pver}.tar.xz ]] ; then
	echo "Error: tmp or ${PN}-${PV}-patches-${pver}.tar.xz exists in git"
	exit 1
fi
rm -rf tmp
rm -f ${PN}-${PV}-*.tar.xz

for myname in 0*.patch ; do
	if [[ -e ${myname} ]]; then
		echo "Error: ${myname} exists in git"
		exit 1
	fi
done
rm -f 0*.patch

mkdir -p tmp/patches

# copy README.Gentoo.patches

cp scripts/gentoo/README.Gentoo.patches tmp/ || exit 1

# create and rename patches

if [[ "${PV}" == "9999" ]]; then
	# we're working with master, start from upstream master
	startpoint="master"
else
	# release branch, start from upstream release tag
	startpoint="glibc-${PV}"
fi

git format-patch ${startpoint}..HEAD > /dev/null

# remove all patches where the summary line starts with [no-tarball] or [no-patch]

rm -f 0???-no-tarball-*.patch
rm -f 0???-no-patch-*.patch

# move patches into temporary directory

mv 0*.patch tmp/patches/ || exit 1

# copy support files

cp -r scripts/gentoo/extra tmp/ || exit 1

# add a history file

git log --stat --decorate ${startpoint}..HEAD > tmp/patches/README.history || exit 1

# package everything up

tar -Jcf ${PN}-${PV}-patches-${pver}.tar.xz \
	-C tmp patches extra README.Gentoo.patches || exit 1
rm -r tmp

du -b *.tar.xz

# tag the commit

git tag -s -m "Gentoo patchset ${PV}-${pver}" "gentoo/glibc-${PV}-${pver}"
