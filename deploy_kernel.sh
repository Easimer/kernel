#!/bin/bash
REMOTE=root@192.168.0.139
scp src/kernel-0.2.img $REMOTE:/boot/
ssh $REMOTE "strip -x /boot/kernel-0.2.img; mount /kerneldev/mnt0/"
scp sdk/program.exe $REMOTE:/kerneldev/mnt0/COMMAND.EXE
ssh $REMOTE "reboot"
