#! /bin/sh
#
# Copyright by The HDF Group.
# All rights reserved.
#
# This file is part of HDF5.  The full HDF5 copyright notice, including
# terms governing use, modification, and redistribution, is contained in
# the COPYING file, which can be found at the root of the source code
# distribution tree, or in https://www.hdfgroup.org/licenses.
# If you do not have access to either file, you may request a copy from
# help@hdfgroup.org.

# This file is for use of h5cc created with the CMake process
# HDF5_HOME is expected to be set

srcdir=..
builddir=.
verbose=yes
nerrors=0

# HDF5 compile commands, assuming they are in your $PATH.
H5CC=$HDF5_HOME/bin/h5cc
LD_LIBRARY_PATH=$HDF5_HOME/lib
export LD_LIBRARY_PATH

if ! test -f $H5CC; then
    echo "Set paths for H5CC and LD_LIBRARY_PATH in test.sh"
    echo "Set environment variable HDF5_HOME to the hdf5 install dir"
    echo "h5cc was not found at $H5CC"
    exit $EXIT_FAILURE
fi

H5DUMP=`echo $H5CC | sed -e 's/\/[^/]*$/\/h5dump/'`;
H5_LIBVER=$($H5CC -showconfig | grep -i "HDF5 Version:" | sed 's/^.* //g' | sed 's/[-].*//g')
H5_APIVER=$($H5CC -showconfig | grep -i "Default API mapping:" | sed 's/^.* //g' | sed 's/v//g' | sed 's/1/1_/')

H5_MAJORVER=$(echo $H5_LIBVER | cut -f1 -d'.'  | sed -E 's/\./_/g')
H5_MINORVER=$(echo $H5_LIBVER | cut -f2 -d'.'  | sed -E 's/\./_/g')
H5_RELEASEVER=$(echo $H5_LIBVER | cut -f3 -d'.'  | sed -E 's/\./_/g')
H5_LIBVER_DIR=$H5_MAJORVER$H5_MINORVER

# Shell commands used in Makefiles
RM="rm -rf"
DIFF="diff -c"
CMP="cmp -s"
GREP='grep'
CP="cp -p"  # Use -p to preserve mode,ownership,timestamps
DIRNAME='dirname'
LS='ls'
AWK='awk'

# setup plugin path
ENVCMD="env HDF5_PLUGIN_PATH=$LD_LIBRARY_PATH/plugin"

TESTDIR=$builddir


case `echo "testing\c"; echo 1,2,3`,`echo -n testing; echo 1,2,3` in
  *c*,-n*) ECHO_N= ECHO_C='
' ;;
  *c*,*  ) ECHO_N=-n ECHO_C= ;;
  *)       ECHO_N= ECHO_C='\c' ;;
esac
ECHO_N="echo $ECHO_N"


exout() {
    $*
}

dumpout() {
    $H5DUMP $*
}

# compare current version, required version.
# returns if cur_ver < req_ver is true.
version_compare() {
  version_lt=0
  if [ ! "$(printf '%s\n' "$1" "$2" | sort -V | head -n1)" = "$2" ]; then
          version_lt=1
  fi
}


topics="alloc checksum chunk compact extern fillval gzip hyper \
rdwr shuffle szip unlimadd unlimgzip unlimmod"
topics18=""

version_compare "$H5_LIBVER" "1.8.0"
# check if HDF5 version is < 1.8.0
if [ "$version_lt" = 1 ]; then
    dir16="/16"
else
    dir16=""
    topics18="nbit sofloat soint transform"
fi

return_val=0

#Remove external data file from h5ex_d_extern
rm -f h5ex_d_extern.data

for topic in $topics
do
    $H5CC $srcdir/h5ex_d_$topic.c -o h5ex_d_$topic
done

for topic in $topics
do
    fname=h5ex_d_$topic
    $ECHO_N "Testing C/H5D/$fname...$ECHO_C"
    exout .$dir16/$fname >tmp.test
    status=$?
    if test $status -eq 1
    then
        echo "  Unsupported feature"
        status=0
    else
        cmp -s tmp.test $srcdir/tfiles/16/$fname.tst
        status=$?
        if test $status -ne 0
        then
            echo "  FAILED!"
        else
          dumpout $fname.h5 >tmp.test
          rm -f $fname.h5
          cmp -s tmp.test $srcdir/tfiles/16/$fname.ddl
          status=$?
          if test $status -ne 0
          then
              echo "  FAILED!"
          else
              echo "  Passed"
          fi
        fi
        return_val=`expr $status + $return_val`
    fi
done

#######Non-standard tests#######
USE_ALT=""
### Set default tfiles directory for tests
nbitdir="18"
version_compare "$H5_LIBVER" "1.8.23"
# check if HDF5 version is < 1.8.23
if [ "$version_lt" = 1 ]; then
    USE_ALT="22"
else
# check if HDF5 version is >= 1.10.0 and < 1.10.8
  version_compare "$H5_LIBVER" "1.10.0"
  if [ "$version_lt" = 0 ]; then
    version_compare "$H5_LIBVER" "1.10.8"
    if [ "$version_lt" = 1 ]; then
      USE_ALT="07"
      nbitdir="110"
    fi
  fi
fi

for topic in $topics18
do
    $H5CC $srcdir/h5ex_d_$topic.c -o h5ex_d_$topic
done

for topic in $topics18
do
    fname=h5ex_d_$topic
    $ECHO_N "Testing C/H5D/$fname...$ECHO_C"
    exout ./$fname >tmp.test
    status=$?
    if test $status -eq 1
    then
        echo "  Unsupported feature"
        status=0
    else
        if [[ $fname == "h5ex_d_nbit" ]]
        then
            tdir=$nbitdir
            if [[ $USE_ALT == "" ]]
            then
                ### set USE_ALT=07 if not set above
                USE_ALT="07"
            fi
        else
            tdir=18
            ### unset USE_ALT for the other topics
            USE_ALT=""
        fi
        cmp -s tmp.test $srcdir/tfiles/18/$fname.tst
        status=$?
        if test $status -ne 0
        then
            echo "  FAILED!"
        else
          if [[ $fname == "h5ex_d_transform" ]]
          then
              targ="-n"
          else
              targ=""
          fi
          dumpout $targ $fname.h5 >tmp.test
          rm -f $fname.h5
          cmp -s tmp.test $srcdir/tfiles/$tdir/$fname$USE_ALT.ddl
          status=$?
          if test $status -ne 0
          then
              echo "  FAILED!"
          else
              echo "  Passed"
          fi
        fi
        return_val=`expr $status + $return_val`
    fi
done


#Remove external data file from h5ex_d_extern
rm -f h5ex_d_extern.data
rm -f tmp.test
echo "$return_val tests failed in C/H5D/"
exit $return_val