umask 0022

if [ -z $USERNAME ]; then USERNAME=$(whoami); fi
BUILDDIR_tmp="/tmp/drlb_ssl_build_${USERNAME}"

if [ -z $1 ]; then
    echo "You must supply a OS version"
    echo "Usage: ${0} rhel6|rhel7"
    exit 1
fi

OS=${1,,}

if [ ! -z "${2}" ]; then BUILDDIR_tmp="${2}"; fi
 
mkdir -p ${BUILDDIR_tmp}/rpmbuild/BUILD
mkdir -p ${BUILDDIR_tmp}/rpmbuild/BUILDROOT
mkdir -p ${BUILDDIR_tmp}/rpmbuild/RPMS
mkdir -p ${BUILDDIR_tmp}/rpmbuild/SOURCES
mkdir -p ${BUILDDIR_tmp}/rpmbuild/SPECS
mkdir -p ${BUILDDIR_tmp}/rpmbuild/SRPMS

mkdir -p ${HOME}/rpmbuild/BUILD
mkdir -p ${HOME}/rpmbuild/BUILDROOT
mkdir -p ${HOME}/rpmbuild/RPMS
mkdir -p ${HOME}/rpmbuild/SOURCES
mkdir -p ${HOME}/rpmbuild/SPECS
mkdir -p ${HOME}/rpmbuild/SRPMS

if [ ! -d ${BUILDDIR_tmp}/rpmbuild/BUILDROOT ]; then
    echo "ERROR - Cannot create BUILDROOT: ${BUILDDIR_tmp}/rpmbuild/BUILDROOT"
    exit 1
fi

if [ ! -d ${BUILDDIR_tmp}/rpmbuild/SPECS ]; then
    echo "ERROR - Cannot create SPECS dir: ${BUILDDIR_tmp}/rpmbuild/SPECS"
    exit 1
fi

cp -fv drlb_ssl_${OS}.spec ${BUILDDIR_tmp}/rpmbuild/SPECS/drlb_ssl.spec
Version=$(head -15 ${BUILDDIR_tmp}/rpmbuild/SPECS/drlb_ssl.spec | grep 'Version:' | awk -F: '{ print $2 }')
Version=$( echo "${Version}" | sed s/' '//g)
Release=$(head -15 ${BUILDDIR_tmp}/rpmbuild/SPECS/drlb_ssl.spec | grep 'Release:' | awk -F: '{ print $2 }')
Release=$( echo "${Release}" | sed s/' '//g)

Build="${BUILDDIR_tmp}/rpmbuild/BUILD/drlb_ssl_server"
mkdir -pv "${Build}"

if [ ! -d "${Build}" ]; then
    echo "ERROR - Cannot make Build: ${Build}"
    exit 1;
fi

source ../../env/gnu_env.sh
cd ../ssl_bridge
make clean_lib
make clean
make

mkdir -p ${Build}/etc/drlb
chmod 755 ${Build}/etc/drlb
mkdir -p ${Build}/var/log/drlb
chmod 755 ${Build}/var/log/drlb
mkdir -p ${Build}/var/run/drlb
chmod 755 ${Build}/var/run/drlb
mkdir -p ${Build}/etc/drlb/ssl/private 
chmod -R 700 ${Build}/etc/drlb/ssl/private
mkdir -p ${Build}/etc/drlb/ssl/certs
chmod -R 755 ${Build}/etc/drlb/ssl/certs
mkdir -p ${Build}/usr/sbin
chmod 755 ${Build}/usr/sbin
cp -f ../ssl_bridge/drlb_ssl_server ${Build}/usr/sbin/drlb_ssl_server
chmod 755 ${Build}/usr/sbin/drlb_ssl_server
cp -f ../etc/ssl_bridge.cfg  ${Build}/etc/drlb/ssl_bridge.cfg
chmod 644 ${Build}/etc/drlb/ssl_bridge.cfg
mkdir -p ${Build}/etc/sysconfig
chmod 755 ${Build}/etc/sysconfig
cp -f ../etc/drlb_ssl_server_sysconfig  ${Build}/etc/sysconfig/drlb_ssl_server
chmod 644 ${Build}/etc/sysconfig/drlb_ssl_server
mkdir -p ${Build}/etc/rc.d/init.d
chmod 755 ${Build}/etc/rc.d/init.d
cp -f ../system5_init/drlb_ssl_server ${Build}/etc/rc.d/init.d/drlb_ssl_server
chmod 755 ${Build}/etc/rc.d/init.d/drlb_ssl_server
mkdir -p ${Build}/usr/share/doc/drlb_ssl_bridge
chmod 755 ${Build}/usr/share/doc/drlb_ssl_bridge
cp -f ../README.txt ${Build}/usr/share/doc/drlb_ssl_bridge/README.txt
chmod 644 ${Build}/usr/share/doc/drlb_ssl_bridge/README.txt
cp -f ../ssl_bridge/README.txt ${Build}/usr/share/doc/drlb_ssl_bridge/README_ssl.txt
chmod 644 ${Build}/usr/share/doc/drlb_ssl_bridge/README_ssl.txt
cp -f ../COPYING ${Build}/usr/share/doc/drlb_ssl_bridge/COPYING
chmod 644 ${Build}/usr/share/doc/drlb_ssl_bridge/COPYING
cp -f ../ssl_bridge/version.txt ${Build}/usr/share/doc/drlb_ssl_bridge/version.txt
chmod 644 ${Build}/usr/share/doc/drlb_ssl_bridge/version.txt
mkdir -p ${Build}/usr/share/man/man8
chmod 755 ${Build}/usr/share/man/man8
gzip -c ../man/drlb_ssl_server.8 > ${Build}/usr/share/man/man8/drlb_ssl_server.8.gz
chmod 644  ${Build}/usr/share/man/man8/drlb_ssl_server.8.gz
if [ "${OS}" == "rhel7" ]; then
    mkdir -p ${Build}/etc/systemd/system
    chmod 755 ${Build}/etc/systemd/system
    cp -f ../systemd_init/drlb_ssl_server.service  ${Build}/etc/systemd/system/drlb_ssl_server.service
    chmod 644 ${Build}/etc/systemd/system/drlb_ssl_server.service
fi

cd ${Build}
if [ -f ${HOME}/rpmbuild/BUILD/drlb_ssl_server.tar.gz ]; then
    rm -f ${HOME}/rpmbuild/BUILD/drlb_ssl_server.tar.gz
fi
tar cfz ${HOME}/rpmbuild/BUILD/drlb_ssl_server.tar.gz *

cd ${BUILDDIR_tmp}/rpmbuild/SPECS
rpmbuild -ba --buildroot=${BUILDDIR_tmp}/rpmbuild/BUILDROOT drlb_ssl.spec

rm -f ${HOME}/rpmbuild/BUILD/drlb_ssl_server.tar.gz
rm -rf ${BUILDDIR_tmp}

