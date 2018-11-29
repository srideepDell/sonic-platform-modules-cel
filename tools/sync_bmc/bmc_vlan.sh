#!/bin/bash

# Add vlan
ip link add link eth0 name eth0.4088 type vlan id 4088
ifconfig eth0.4088 inet6 add "fe80::2:1/64" up
ip addr add 240.1.1.2/30 dev eth0.4088