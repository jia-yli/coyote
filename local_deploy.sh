#!/bin/bash

# login to deploy server and exec 'hw_server &' before exec this script

##
## Args
##

# if [ "$1" == "-h" ]; then
#   echo "Usage: $0 <base_path> <bitstream_path_within_base> <driver_path_within_base> " >&2
#   exit 0
# fi

if ! [ -x "$(command -v vivado)" ]; then
	echo "Vivado does NOT exist in the system."
	exit 1
fi

BASE_PATH="/home/jiayli/projects/coyote"

# if [ -z "$2" ]; then
#     PROGRAM_FPGA=0
# else
    PROGRAM_FPGA=1
    BIT_PATH="hw/build/bitstreams/cyt_top"
    # BIT_PATH="../dedup/bitstreams/cyt_top"
# fi

# if [ -z "$3" ]; then
#     DRV_INSERT=1
# else
    DRV_INSERT=1
    DRV_PATH="driver"
# fi

##
## Server IDs (u55c)
##
SERVID=$(hostname -s | tail -c 3)

BOARDSN=(XFL1QOQ1ATTYA XFL1O5FZSJEIA XFL1QGKZZ0HVA XFL11JYUKD4IA XFL1EN2C02C0A XFL1NMVTYXR4A XFL1WI3AMW4IA XFL1ELZXN2EGA XFL1W5OWZCXXA XFL1H2WA3T53A)

##
## Program FPGA
##

alveo_program()
{
	BOARDSN=$1
	DEVICENAME=$2
	BITPATH=$3
	vivado -nolog -nojournal -mode batch -source ./local_program.tcl -tclargs $BOARDSN $DEVICENAME $BITPATH
}

if [ $PROGRAM_FPGA -eq 1 ]; then
	echo "*** Programming FPGA... (path: $BIT_PATH)"
    echo " ** "
  boardidx=$(expr $SERVID - 1)
  alveo_program ${BOARDSN[boardidx]} xcu280_u55c_0 $BASE_PATH/$BIT_PATH &
	wait
	echo "*** FPGA programmed"
    echo " ** "
fi

##
## Driver insertion
##

if [ $DRV_INSERT -eq 1 ]; then
	# #NOTE: put -x '-tt' (pseudo terminal) here for sudo command
	echo "*** Removing the driver ..."
    echo " ** "
	  sudo rmmod coyote_drv
  echo "*** Rescan PCIe ..."	
    echo " ** "
	  sudo /opt/cli/program/pci_hot_plug "$(hostname -s)"
	# read -p "Hot-reset done. Press enter to load the driver or Ctrl-C to exit."
	# echo "*** Compiling the driver ..."
  #   echo " ** "
	#   make -C $BASE_PATH/$DRV_PATH
	echo "*** Loading the driver ..."
    echo " ** "
    # sudo insmod $BASE_PATH/$DRV_PATH/coyote_drv.ko
    # sudo /opt/cli/program/fpga_chmod 0
    sgutil program vivado -d $BASE_PATH/$DRV_PATH/coyote_drv.ko
  echo "*** Chmod FPGA ..."	
    echo " ** "
    sgutil set write -i 0
	echo "*** Driver loaded"
    echo " ** "
fi
