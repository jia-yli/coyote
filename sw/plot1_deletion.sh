#!/bin/bash
# fullness_lst=(0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0)
# gc_ratio_lst=(0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0)

fullness_lst=(0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0)
gc_ratio_lst=(0.0)
num_page_lst=(16384)
repeat=8

cd /home/jiayli/projects/coyote/sw/build

for fullness in "${fullness_lst[@]}"
do
  for gc_ratio in "${gc_ratio_lst[@]}"
  do
    for num_page in "${num_page_lst[@]}"
    do
      echo "delete ${num_page} pages with ${gc_ratio} GC when hash table is ${fullness} full"
      ./main -n ${num_page} -g ${gc_ratio} -f ${fullness} -r ${repeat} -v 0
      echo "delete 16 pages with ${gc_ratio} GC when hash table is ${fullness} full"
      ./main -n 16 -g ${gc_ratio} -f ${fullness} -r ${repeat} -v 0
    done
  done
done