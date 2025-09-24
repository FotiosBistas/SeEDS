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

# Persistent MongoDB data root (per host under this dir)
PERSIST_BASE = "/home/fotis/Github/SeEDS/src/minindn/persist/mongo"

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

    # Point default nfd.sock to host 'a' (existing behavior)
    os.system('sudo rm -f /run/nfd/nfd.sock')
    os.system('ln /run/nfd/a.sock /run/nfd/nfd.sock')

    # Determine PRIMARY broker endpoint (we use host 0 / 'a' as PRIMARY)
    primary_host = ndn.net.hosts[0]
    primary_ip   = primary_host.IP()
    primary_port = 10000  # broker bind-port we use everywhere
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

    # print or use broker_cmd_parts as needed (debug)
    print(broker_cmd_parts)

    # --- Per-host setup: Mongo (persistent) + Broker + Daemon + tcpdump
    for idx, host in enumerate(ndn.net.hosts):
        host_home = f"/home/fotis/Github/SeEDS/src/minindn/tmp/{host.name}"   # volatile logs, etc.
        mongo_dir = f"{PERSIST_BASE}/{host.name}"                              # PERSISTENT data
        node_ip   = host.IP()                                                  # unique Mininet IP
        host.cmdPrint(f'mkdir -p "{host_home}" "{mongo_dir}"')

        #is_last = (idx == number_of_services - 1)  # make last host SECONDARY
        is_second = (idx == 1)  # make the second host SECONDARY
        is_primary = not is_second
        registry_key = f"CAR_{idx}"

        # --- SAFE per-host Mongo startup (no pkill) ---

        # Try a clean shutdown of this node's Mongo (harmless if not running)
        host.cmd(
            'bash -lc '
            f'\'mongod --dbpath "{mongo_dir}" --shutdown >/dev/null 2>&1 || true; '
            f'rm -f "{mongo_dir}/mongod.pid" >/dev/null 2>&1 || true\''
        )

        # Start mongod bound to the node IP only (no 127.0.0.1)
        ret = host.cmd(
            'bash -lc \''
            'mongod '
            '--port 27017 '
            f'--bind_ip {node_ip} '
            f'--dbpath "{mongo_dir}" '
            f'--logpath "{mongo_dir}/mongod.log" '
            f'--pidfilepath "{mongo_dir}/mongod.pid" '
            '--wiredTigerCacheSizeGB 0.25 '
            '--fork'
            '\''
        )

        # If fork failed, show log and bail
        if "child process failed" in ret or "ERROR:" in ret:
            host.cmdPrint(f'tail -n 200 "{mongo_dir}/mongod.log" || true')
            print(f"[FATAL] mongod failed to start in host {host.name}")
            MiniNDNCLI(ndn.net); ndn.stop(); sys.exit(1)

        # Show last lines (helpful context)
        host.cmdPrint(f'tail -n 10 "{mongo_dir}/mongod.log" || true')

        # Strong readiness: mongosh ping via the NODE IP
        ready = host.cmd(
            'bash -lc \'for i in {1..60}; do '
            f'mongosh --quiet --host {node_ip} --port 27017 --eval "db.adminCommand({{ping:1}}).ok" 2>/dev/null | grep -q "^1$" && echo READY && exit 0; '
            'sleep 0.5; '
            'done; echo FAIL; exit 1\''
        ).strip()

        if ready != "READY":
            host.cmdPrint(f'tail -n 200 "{mongo_dir}/mongod.log" || true')
            print(f"[FATAL] mongod not healthy in host {host.name}")
            MiniNDNCLI(ndn.net); ndn.stop(); sys.exit(1)

        # Start a broker on the SAME host, talking to this node's Mongo via node IP
        broker_uri = f'mongodb://{node_ip}:27017/?serverSelectionTimeoutMS=60000'
        host.cmdPrint(
            'cd /home/fotis/Github/SeEDS/build && '
            './snds_broker_bin '
            f'--mongo.uri="{broker_uri}" '
            f'--bind-ip={node_ip} '
            '--bind-port=10000 '
            f'--log-level={args.log_level} '
            f'--log-file="{host_home}/Broker.log" '
            f'> "{host_home}/Broker.stdout" 2>&1 &'
        )

        # idx is your SNDS ID (0 for host a). Or use (host.name == 'a')
        only_a_timeout = 'timeout 120s' if idx == 0 else ''
        #only_a_timeout = '' if idx == 0 else ''

        # Prepare seed-from (PRIMARY) flags for Producer:
        # - PRIMARY itself can omit them (Producer defaults to its own broker),
        #   but setting explicitly is fine too.
        seed_from_ip   = primary_ip
        seed_from_port = primary_port

        # Start the SNDS daemon on every host, pointing to the per-node broker via node IP
        host.cmdPrint(
            'cd /home/fotis/Github/SeEDS/build && '
            f'env HOME="{host_home}" {only_a_timeout} ./snds_daemon_bin '
            f'--log-level={args.log_level} '
            f'--num-services={number_of_services} '
            '--snds-prefix=SNDS '
            '--state-prefix=state '
            '--is-alive-prefix=is_alive '
            f'--snds-id={idx} '
            '--id-scope-prefix=SNDS_ID_scope_ '
            '--registry-file-postfix=_registry '
            f'--producer.broker-ip={node_ip} '
            '--producer.broker-port=10000 '
            f'--producer.registry-type={registry_key} '
            '--producer.enable-resilience-ipc=true '
            f'--producer.resilience-ipc-host={node_ip} '
            f'--producer.resilience-ipc-port={47832 + idx} '
            f'--producer.is-primary={"true" if is_primary else "false"} '
            f'--producer.seed-from-broker-ip={seed_from_ip} '
            f'--producer.seed-from-broker-port={seed_from_port} '
            f'> "{host_home}/log.txt" 2>&1 &'
        )

        # Per-host packet capture (optional)
        host.cmdPrint(
            f'sudo sh -c \'tcpdump -i any -n -tttt > "{host_home}/packets.txt" 2>&1 &\''
        )

        # Stagger startups a bit
        sleep(2)

    # Interactive CLI; when you exit, we’ll gracefully shut down services
    MiniNDNCLI(ndn.net)

    # --- Graceful shutdown: brokers then mongod (persist data cleanly) ---
    for host in ndn.net.hosts:
        # Stop broker(s) (match the binary name)
        host.cmd('pkill -f "/snds_broker_bin" >/dev/null 2>&1 || true')

    # Give brokers a moment to flush
    sleep(1)

    for host in ndn.net.hosts:
        mongo_dir = f"{PERSIST_BASE}/{host.name}"
        # Clean shutdown of mongod for this host (safe if already stopped)
        host.cmdPrint(
            'mongod '
            f'--dbpath "{mongo_dir}" '
            '--shutdown '
            '>/dev/null 2>&1 || true'
        )
        # Remove stale pidfile if left behind
        host.cmd(f'rm -f "{mongo_dir}/mongod.pid" >/dev/null 2>&1 || true')

    ndn.stop()
