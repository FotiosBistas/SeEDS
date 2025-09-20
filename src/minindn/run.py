import argparse
import sys
import os
import shlex
import urllib.parse

from time import sleep

from mininet.log import setLogLevel, info
from minindn.minindn import Minindn
from minindn.util import MiniNDNCLI
from minindn.apps.app_manager import AppManager
from minindn.apps.nfd import Nfd
from minindn.apps.nlsr import Nlsr


if __name__ == '__main__':
    # Mini-NDN will grab its own arguments normally.
    # We’ll treat everything *after the topology* as our own.
    if len(sys.argv) > 1:
        topo_and_minindn_args = [sys.argv[0], sys.argv[1]]  # script + topology
        my_args = sys.argv[2:]
    else:
        topo_and_minindn_args = sys.argv
        my_args = []

    sys.argv = topo_and_minindn_args  # hand only topology to Mini-NDN

    parser = argparse.ArgumentParser(description="Extra SeEDS options")
    parser.add_argument('--mongo-user', default=None)
    parser.add_argument('--mongo-pass', default=None)
    parser.add_argument('--mongo-db', default=None)
    parser.add_argument('--mongo-uri', default=None)
    parser.add_argument('--log-level', default='debug')
    args = parser.parse_args(my_args)

    setLogLevel('info')

    Minindn.cleanUp()
    Minindn.verifyDependencies()

    ndn = Minindn()
    ndn.start()

    info('Starting NFD on nodes\n')
    nfds = AppManager(ndn, ndn.net.hosts, Nfd)
    info('Starting NLSR on nodes\n')
    nlsrs = AppManager(ndn, ndn.net.hosts, Nlsr)

    os.system('sudo rm /run/nfd/nfd.sock')
    os.system('ln /run/nfd/a.sock /run/nfd/nfd.sock')

    number_of_services = len(ndn.net.hosts)
    counter = 0

    cclib = "/home/fotis/Github/SeEDS/build/mongo-c-driver-install/lib"
    cxxlib = "/home/fotis/Github/SeEDS/build/mongo-cxx-driver-install/lib"
    ndnlib = "/home/fotis/Github/SeEDS/build/ndn-cxx/install/lib"

    broker_cmd_parts = [
        './snds_broker_bin',
        f'--log-level={args.log_level}',
    ]
    if args.mongo_user:
        broker_cmd_parts.append(f'--mongo.user={args.mongo_user}')
    if args.mongo_pass:
        broker_cmd_parts.append(f'--mongo.pass={args.mongo_pass}')
    if args.mongo_db:
        broker_cmd_parts.append(f'--mongo.db={args.mongo_db}')
    if args.mongo_uri:
        user_enc = urllib.parse.quote(args.mongo_user or "", safe="")
        pass_enc = urllib.parse.quote(args.mongo_pass or "", safe="")
        uri_filled = (
            args.mongo_uri
            .replace("<user>", user_enc)
            .replace("<pass>", pass_enc)
        )
        # Shell-quote the whole URI so < > & ? = etc. are safe
        broker_cmd_parts.append(f'--mongo.uri={shlex.quote(uri_filled)}')

    print(broker_cmd_parts)
    for host in ndn.net.hosts:
        host_home = f"/home/fotis/Github/SeEDS/src/minindn/tmp/{host.name}"
        host.cmdPrint(f'mkdir -p "{host_home}"')

        host.cmdPrint(
            'cd /home/fotis/Github/SeEDS/build && '
            f'env HOME="{host_home}" ./snds_daemon_bin '
            f'--log-level={args.log_level} '
            f'--num-services={number_of_services} '
            '--snds-prefix=SNDS '
            '--state-prefix=state '
            '--is-alive-prefix=is_alive '
            f'--snds-id={counter} '
            '--id-scope-prefix=SNDS_ID_scope_ '
            '--registry-file-postfix=_registry '
            '--producer.broker-ip=127.0.0.1 '
            '--producer.broker-port=10000 '
            f'--producer.registry-type=CAR_{counter} '
            '--producer.enable-resilience-ipc=true '
            '--producer.resilience-ipc-host=127.0.0.1 '
            '--producer.resilience-ipc-port=47832 '
            f'> "{host_home}/log.txt" 2>&1 &'
        )

        host.cmdPrint(
            f'sudo sh -c \'tcpdump -i any -n -tttt > "{host_home}/packets.txt" 2>&1 &\''
        )

        counter += 1
        sleep(2)

    MiniNDNCLI(ndn.net)
    ndn.stop()
