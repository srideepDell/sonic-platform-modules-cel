#!/bin/bash

# Add vlan
vconfig add eth0 4088
ifconfig eth0.4088 inet6 add "fe80::2:1/64" up