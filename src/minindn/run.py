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

    # hand only topology to Mini-NDN
    sys.argv = topo_and_minindn_args

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

    # Point default nfd.sock to host 'a' (your existing behavior)
    os.system('sudo rm /run/nfd/nfd.sock')
    os.system('ln /run/nfd/a.sock /run/nfd/nfd.sock')

    # TODO CARE CARE CARE 
    # DO NOT CONSIDER SECONDARY SERVICES AS HOSTS 
    # NEED TO DISCUSS THIS
    number_of_services = len(ndn.net.hosts) 

    # (Optional) Broker command construction — kept for completeness
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
        broker_cmd_parts.append(f'--mongo.uri={shlex.quote(uri_filled)}')

    # print or use broker_cmd_parts as needed
    print(broker_cmd_parts)

    for idx, host in enumerate(ndn.net.hosts):
        host_home = f"/home/fotis/Github/SeEDS/src/minindn/tmp/{host.name}"
        host.cmdPrint(f'mkdir -p "{host_home}"')

        is_last = (idx == number_of_services - 1)  # make last host SECONDARY
        is_primary = not is_last
        registry_key = f"CAR_{idx}"

        # Start the resilience poller ONLY on the SECONDARY (last host),
        # pointing it at the PRIMARY's alive/state names (assumed id 0).
        if is_last:
            sleep(10)
            host.cmdPrint(
                'cd /home/fotis/Github/SeEDS/build && '
                f'env HOME="{host_home}" ./resilience_poller_bin '
                '--prefix=SNDS '
                '--peer-alive=is_alive_0 '
                '--peer-state=state_0 '
                '--alive-ms=14000 '
                '--meta-ms=2000 '
                '--ipc-host=127.0.0.1 '
                '--ipc-port=47832 '
                f'--log-level={args.log_level} '
                '&'
            )

        # Start the SNDS daemon on every host:
        # - PRIMARY on all except the last
        # - SECONDARY on the last (with --producer.is-primary=false)
        host.cmdPrint(
            'cd /home/fotis/Github/SeEDS/build && '
            f'env HOME="{host_home}" ./snds_daemon_bin '
            f'--log-level={args.log_level} '
            f'--num-services={number_of_services} '
            '--snds-prefix=SNDS '
            '--state-prefix=state '
            '--is-alive-prefix=is_alive '
            f'--snds-id={idx} '
            '--id-scope-prefix=SNDS_ID_scope_ '
            '--registry-file-postfix=_registry '
            '--producer.broker-ip=127.0.0.1 '
            '--producer.broker-port=10000 '
            f'--producer.registry-type={registry_key} '
            '--producer.enable-resilience-ipc=true '
            '--producer.resilience-ipc-host=127.0.0.1 '
            '--producer.resilience-ipc-port=47832 '
            f'--producer.is-primary={"true" if is_primary else "false"} '
            f'> "{host_home}/log.txt" 2>&1 &'
        )

        # Per-host packet capture
        host.cmdPrint(
            f'sudo sh -c \'tcpdump -i any -n -tttt > "{host_home}/packets.txt" 2>&1 &\''
        )

        # Stagger startups a bit
        sleep(2)

    MiniNDNCLI(ndn.net)
    ndn.stop()
