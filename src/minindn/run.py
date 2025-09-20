# -*- Mode:python; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
#
# Copyright (C) 2015-2020, The University of Memphis,
#                          Arizona Board of Regents,
#                          Regents of the University of California.
#
# This file is part of Mini-NDN.
# See AUTHORS.md for a complete list of Mini-NDN authors and contributors.
#
# Mini-NDN is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Mini-NDN is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Mini-NDN, e.g., in COPYING.md file.
# If not, see <http://www.gnu.org/licenses/>.
import subprocess
import random
import os

from time import sleep
import numpy as np

from mininet.log import setLogLevel, info

from minindn.minindn import Minindn
from minindn.util import MiniNDNCLI
from minindn.apps.app_manager import AppManager
from minindn.apps.nfd import Nfd
from minindn.apps.nlsr import Nlsr

#from helpers import run_command_on_mininet_host


if __name__ == '__main__':
    setLogLevel('info')

    Minindn.cleanUp()
    Minindn.verifyDependencies()

    ndn = Minindn()

    ndn.start()

    info('Starting NFD on nodes\n')
    nfds = AppManager(ndn, ndn.net.hosts, Nfd)
    info('Starting NLSR on nodes\n')
    nlsrs = AppManager(ndn, ndn.net.hosts, Nlsr)

    #sleep(300)
    
    
    #sleep(30)


    os.system('sudo rm /run/nfd/nfd.sock')
    os.system('ln /run/nfd/a.sock /run/nfd/nfd.sock')
    
    number_of_services = len(ndn.net.hosts) 
    counter = 0
    for host in ndn.net.hosts:
        host_home = f"/home/fotis/Github/SeEDS/src/minindn/tmp/{host.name}"

        # make per-host dir first
        host.cmdPrint(f'mkdir -p "{host_home}"')

        # run daemon; pass HOME through sudo; redirect to absolute path
        host.cmdPrint(
            'cd /home/fotis/Github/SeEDS/build && '
            f'sudo env HOME="{host_home}" ./snds_daemon_bin '
            '--log-level=debug '
            f'--num-services={number_of_services} '
            '--snds-prefix=SNDS '
            '--state-prefix=state '
            '--is-alive-prefix=is_alive '
            f'--snds-id={counter} '
            '--id-scope-prefix=SNDS_ID_scope_ '
            '--registry-file-postfix=_registry '
            '--producer.broker-ip=0.0.0.0 '
            '--producer.broker-port=10000 '
            '--producer.registry-type=CAR '
            '--producer.enable-resilience-ipc=true '
            '--producer.resilience-ipc-host=127.0.0.1 '
            '--producer.resilience-ipc-port=47832 '
            f'> "{host_home}/log.txt" 2>&1 &'
        )

        # tcpdump as root, redirect to absolute path
        host.cmdPrint(
            f'sudo sh -c \'tcpdump -i any -n -tttt > "{host_home}/packets.txt" 2>&1 &\''
        )

        counter += 1
        sleep(2)


    
    MiniNDNCLI(ndn.net)

    ndn.stop()

    #ndn.net.stop()