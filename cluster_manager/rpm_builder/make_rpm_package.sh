umask 0022

if [ -z $USERNAME ]; then USERNAME=$(whoami); fi
BUILDDIR_tmp="/tmp/drcm_build_${USERNAME}"

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

cp -fv drcm_${OS}.spec ${BUILDDIR_tmp}/rpmbuild/SPECS/drcm.spec
Version=$(head -15 ${BUILDDIR_tmp}/rpmbuild/SPECS/drcm.spec | grep 'Version:' | awk -F: '{ print $2 }')
Version=$( echo "${Version}" | sed s/' '//g)
Release=$(head -15 ${BUILDDIR_tmp}/rpmbuild/SPECS/drcm.spec | grep 'Release:' | awk -F: '{ print $2 }')
Release=$( echo "${Release}" | sed s/' '//g)

Build="${BUILDDIR_tmp}/rpmbuild/BUILD/drcm_server"
mkdir -pv "${Build}"

if [ ! -d "${Build}" ]; then
    echo "ERROR - Cannot make Build: ${Build}"
    exit 1;
fi

source ../../env/gnu_env.sh
cd ../drcm_server
make clean_lib
make clean
make

mkdir -p ${Build}/etc/drcm
chmod 755 ${Build}/etc/drcm
cp -fv ../etc/cm_2_node_example.cfg ${Build}/etc/drcm/.
chmod 644 ${Build}/etc/drcm/cm_2_node_example.cfg
cp -fv ../etc/cm_4_node_example.cfg ${Build}/etc/drcm/.
chmod 644 ${Build}/etc/drcm/cm_4_node_example.cfg
mkdir -p ${Build}/etc/drcm/file_system_sync
chmod 755 ${Build}/etc/drcm/file_system_sync
cp -fv ../etc/file_system_sync/* ${Build}/etc/drcm/file_system_sync/.
chmod 755 ${Build}/etc/drcm/file_system_sync/*
mkdir -p ${Build}/etc/drcm/utils
mkdir -pv ${Build}/etc/drcm/utils
chmod 755 ${Build}/etc/drcm/utils
cp -fv ../etc/utils/* ${Build}/etc/drcm/utils/.
chmod 755  ${Build}/etc/drcm/utils/*
mkdir -p ${Build}/etc/drcm/my_cluster_conf
chmod 755 ${Build}/etc/drcm/my_cluster_conf
cp -fv ../etc/my_cluster_conf/my_cluster_info.sh ${Build}/etc/drcm/my_cluster_conf/my_cluster_info.sh
chmod 755 ${Build}/etc/drcm/my_cluster_conf/my_cluster_info.sh
cp -fv ../etc/my_cluster_conf/my_cluster_info_example1.sh ${Build}/etc/drcm/my_cluster_conf/.
chmod 644 ${Build}/etc/drcm/my_cluster_conf/my_cluster_info_example1.sh
cp -fv ../etc/my_cluster_conf/testfs_sync_list.sh ${Build}/etc/drcm/my_cluster_conf/.
chmod 644 ${Build}/etc/drcm/my_cluster_conf/testfs_sync_list.sh
mkdir -p ${Build}/etc/drcm/resources
chmod 755 ${Build}/etc/drcm/resources
cp -fv ../etc/resources/* ${Build}/etc/drcm/resources/.
chmod 755 ${Build}/etc/drcm/resources/*
mkdir -p ${Build}/etc/drcm/crontabs
cp -fv ../etc/crontabs/apps_package1 ${Build}/etc/drcm/crontabs/apps_package1
cp -fv ../etc/crontabs/apps_package2 ${Build}/etc/drcm/crontabs/apps_package2
cp -fv ../etc/crontabs/apps_package3 ${Build}/etc/drcm/crontabs/apps_package3
cp -fv ../etc/crontabs/apps_package4 ${Build}/etc/drcm/crontabs/apps_package4
cp -fv ../etc/crontabs/system_crons ${Build}/etc/drcm/crontabs/system_crons
cp -fv ../etc/crontabs/user_crons ${Build}/etc/drcm/crontabs/user_crons
chmod 644 ${Build}/etc/drcm/crontabs/*
mkdir -p ${Build}/etc/drcm/.auth/
chmod 700 ${Build}/etc/drcm/.auth/
mkdir -p ${Build}/var/log/drcm
chmod 755 ${Build}/var/log/drcm
mkdir -p ${Build}/var/spool/drcm
chmod 755 ${Build}/var/spool/drcm
mkdir -p ${Build}/var/run/drcm
chmod 755 ${Build}/var/run/drcm
mkdir -p ${Build}/usr/sbin
chmod 755 ${Build}/usr/sbin
cp -fv drcm_server ${Build}/usr/sbin/drcm_server
chmod 755 ${Build}/usr/sbin/drcm_server
mkdir -p ${Build}/etc/rc.d/init.d
chmod 755 ${Build}/etc/rc.d/init.d
cp -fv ../system5_init/drcm_server ${Build}/etc/rc.d/init.d/drcm_server
chmod 755 ${Build}/etc/rc.d/init.d/drcm_server
mkdir -p ${Build}/etc/sysconfig
chmod 755 ${Build}/etc/sysconfig
cp -fv ../etc/system_templates/drcm_server_sysconfig ${Build}/etc/sysconfig/drcm_server
chmod 644 ${Build}/etc/sysconfig/drcm_server
mkdir -p ${Build}/usr/share/doc/drcm
chmod 755 ${Build}/usr/share/doc/drcm
cp -fv ../README.txt ${Build}/usr/share/doc/drcm/README.txt
chmod 644 ${Build}/usr/share/doc/drcm/README.txt
cp -fv ../COPYING ${Build}/usr/share/doc/drcm/COPYING
chmod 644 ${Build}/usr/share/doc/drcm/COPYING
cp -fv ../version.txt ${Build}/usr/share/doc/drcm/version.txt
chmod 644 ${Build}/usr/share/doc/drcm/version.txt
mkdir -p ${Build}/usr/share/man/man1
mkdir -p ${Build}/usr/share/man/man8
gzip -c ../man/drcm_server.1 > ${Build}/usr/share/man/man1/drcm_server.1.gz
gzip -c ../man/drcm_server.8 > ${Build}/usr/share/man/man8/drcm_server.8.gz
chmod 644  ${Build}/usr/share/man/man1/drcm_server.1.gz
chmod 644 ${Build}/usr/share/man/man8/drcm_server.8.gz
if [ "${OS}" == "rhel7" ]; then
    mkdir -p ${Build}/etc/systemd/system
    chmod 755 ${Build}/etc/systemd/system
    cp -fv ../systemd_init/drcm_server.service  ${Build}/etc/systemd/system/drcm_server.service
    chmod 644 ${Build}/etc/systemd/system/drcm_server.service
fi

cd ${Build}
if [ -f ${HOME}/rpmbuild/BUILD/drcm_server.tar.gz ]; then
    rm -f ${HOME}/rpmbuild/BUILD/drcm_server.tar.gz
fi
tar cfz ${HOME}/rpmbuild/BUILD/drcm_server.tar.gz *

cd ${BUILDDIR_tmp}/rpmbuild/SPECS
rpmbuild -ba --buildroot=${BUILDDIR_tmp}/rpmbuild/BUILDROOT drcm.spec

rm -f ${HOME}/rpmbuild/BUILD/drcm_server.tar.gz
rm -rf ${BUILDDIR_tmp}

