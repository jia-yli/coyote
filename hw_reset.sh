#!/bin/bash
set -o xtrace
echo "*** Loading bitstream and driver..."
  echo " ** "
  sgutil program vivado --bitstream /mnt/scratch/jiayli/project_8FSM_noBF/coyote/hw/build/bitstreams/cyt_top.bit --driver /home/jiayli/projects/coyote/driver/coyote_drv.ko
echo "*** Chmod FPGA ..."	
  echo " ** "
  sgutil set write -i 0
echo "*** Done"
  echo " ** "
