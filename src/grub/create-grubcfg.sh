#!/bin/sh

cat << EOF > $1
menuentry "Load kernel" {
    multiboot2 /boot/kernel-$2.img
}
EOF
