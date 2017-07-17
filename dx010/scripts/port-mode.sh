#!/bin/bash
mode=$1

if [ "$mode" = "100G" ]; then
    cp /usr/share/sonic/device/x86_64-cel_seastone-r0/Seastone-DX010/minigraph.xml /etc/sonic/minigraph.xml
    echo "32x100G mode is used"
elif [ "$mode" = "10G-50G" ]; then
    cp /usr/share/sonic/device/x86_64-cel_seastone-r0/Seastone-DX010-10-50/minigraph.xml /etc/sonic/minigraph.xml
    echo "96x10G+16x50G mode is used"
elif [ "$mode" = "50G" ]; then
    cp /usr/share/sonic/device/x86_64-cel_seastone-r0/Seastone-DX010-50/minigraph.xml /etc/sonic/minigraph.xml
    echo "64x100G mode is used"
else
    echo "Please enter 100G, 10G-50G or 50G mode"
    exit
fi

docker stop syncd
docker stop swss
docker rm syncd
docker rm swss
echo " rebooting the box"
reboot
