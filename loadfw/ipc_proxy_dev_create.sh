# This script creates the /dev/mid_ipc entry under /dev
#  Copyright (C) 2009 Intel Corp
#  Author :Sudha Krishnakumar, Intel Inc.
#  Copyright (C) 2009 Intel Corporation. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU Lesser General Public License,
# version 2.1, as published by the Free Software Foundation.
# This program is distributed in the hope it will be useful, but
# WITHOUT ANY WARRANTY;
# without even the implied warranty of MERCHANTABILITY or  
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
# Public License for more details. 
# You should have received a copy of the GNU Lesser General
# Public License along with this program;
# if not, write to the Free Software Foundation, Inc., 
# * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.


#!/bin/sh

major_no=0

#Check if user is root, if not exit.

user=`whoami`

if [ $user != root ]
then
echo "you need to be root, to run this script"
exit 1
fi

if [ -z $1 ] 
then
ipc_dev=mid_ipc
else
ipc_dev=$1
fi

# echo "ipc_dev=$ipc_dev"

major=`grep $ipc_dev /proc/devices | awk '{ print $1 }'`

# echo "major no for ipc_dev=$major"

if [ -z $major ] 
then
echo "$ipc_dev device entry not there in /proc/devices"
exit 2
fi

mknod /dev/$ipc_dev c $major 0

