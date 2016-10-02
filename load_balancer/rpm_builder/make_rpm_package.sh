umask 0022

if [ -z $USERNAME ]; then USERNAME=$(whoami); fi
BUILDDIR_tmp="/tmp/drlb_build_${USERNAME}"

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

cp -fv drlb_${OS}.spec ${BUILDDIR_tmp}/rpmbuild/SPECS/drlb.spec
Version=$(head -15 ${BUILDDIR_tmp}/rpmbuild/SPECS/drlb.spec | grep 'Version:' | awk -F: '{ print $2 }')
Version=$( echo "${Version}" | sed s/' '//g)
Release=$(head -15 ${BUILDDIR_tmp}/rpmbuild/SPECS/drlb.spec | grep 'Release:' | awk -F: '{ print $2 }')
Release=$( echo "${Release}" | sed s/' '//g)

Build="${BUILDDIR_tmp}/rpmbuild/BUILD/drlb_server"
mkdir -pv "${Build}"

if [ ! -d "${Build}" ]; then
    echo "ERROR - Cannot make Build: ${Build}"
    exit 1;
fi

source ../../env/gnu_env.sh
cd ../drlb_server
make clean_lib
make clean
make

mkdir -p ${Build}/etc/drlb
chmod 755 ${Build}/etc/drlb
mkdir -p ${Build}/var/log/drlb
chmod 755 ${Build}/var/log/drlb
mkdir -p ${Build}/var/run/drlb
chmod 755 ${Build}/var/run/drlb
mkdir -p ${Build}/usr/sbin
chmod 755 ${Build}/usr/sbin
cp -fv ../drlb_server/drlb_server ${Build}/usr/sbin/drlb_server
chmod 755 ${Build}/usr/sbin/drlb_server
mkdir -p ${Build}/etc/sysconfig
chmod 755 ${Build}/etc/sysconfig
cp -fv ../etc/drlb_server_sysconfig  ${Build}/etc/sysconfig/drlb_server
chmod 644 ${Build}/etc/sysconfig/drlb_server
cp -fv ../etc/active_configs.list_template ${Build}/etc/drlb/active_configs.list
chmod 644 ${Build}/etc/drlb/active_configs.list
cp -fv ../etc/http_rr_test.cfg ${Build}/etc/drlb/http_rr_test.cfg
chmod 644 ${Build}/etc/drlb/http_rr_test.cfg
mkdir -p ${Build}/etc/rc.d/init.d
chmod 755 ${Build}/etc/rc.d/init.d
cp -fv ../system5_init/drlb_server ${Build}/etc/rc.d/init.d/drlb_server
chmod 755 ${Build}/etc/rc.d/init.d/drlb_server
mkdir -p ${Build}/usr/share/doc/drlb
chmod 755 ${Build}/usr/share/doc/drlb
cp -fv ../README.txt ${Build}/usr/share/doc/drlb/README.txt
chmod 644 ${Build}/usr/share/doc/drlb/README.txt
cp -fv ../COPYING ${Build}/usr/share/doc/drlb/COPYING
chmod 644 ${Build}/usr/share/doc/drlb/COPYING
cp -fv ../version.txt ${Build}/usr/share/doc/drlb/version.txt
chmod 644 ${Build}/usr/share/doc/drlb/version.txt
mkdir -p ${Build}/usr/share/man/man8
chmod 755 ${Build}/usr/share/man/man8
gzip -c ../man/drlb_server.8 > ${Build}/usr/share/man/man8/drlb_server.8.gz
chmod 644  ${Build}/usr/share/man/man8/drlb_server.8.gz
if [ "${OS}" == "rhel7" ]; then
    mkdir -p ${Build}/etc/systemd/system
    chmod 755 ${Build}/etc/systemd/system
    cp -fv ../systemd_init/drlb_server.service  ${Build}/etc/systemd/system/drlb_server.service
    chmod 644 ${Build}/etc/systemd/system/drlb_server.service
fi

cd ${Build}
if [ -f ${HOME}/rpmbuild/BUILD/drlb_server.tar.gz ]; then
    rm -f ${HOME}/rpmbuild/BUILD/drlb_server.tar.gz
fi
tar cfz ${HOME}/rpmbuild/BUILD/drlb_server.tar.gz *

cd ${BUILDDIR_tmp}/rpmbuild/SPECS
rpmbuild -ba --buildroot=${BUILDDIR_tmp}/rpmbuild/BUILDROOT drlb.spec

rm -f ${HOME}/rpmbuild/BUILD/drlb_server.tar.gz
rm -rf ${BUILDDIR_tmp}

