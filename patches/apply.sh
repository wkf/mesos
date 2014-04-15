#!/usr/bin/env bash

echo "******* applying 20179 *******"
patch -p1 < patches/20179_rb20179.patch
echo "******* applying 20221 *******"
patch -p1 < patches/20221_state.patch
echo "******* applying 19795 *******"
patch -p1 < patches/19795_executor_info09.patch
echo "******* applying 18403 *******"
patch -p1 < patches/18403_task_info.patch
# echo "******* applying 19901 *******"
# patch -p1 < patches/19901_termination.patch
echo "******* applying 20141 *******"
patch -p1 < patches/20141_plain_containerizer_proto.patch
echo "******* applying 17567 *******"
patch -p1 < patches/17567_external_containerizer.patch
