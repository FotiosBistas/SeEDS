#!/usr/bin/env python3
import argparse
import sys
import os
from time import sleep

from mininet.log import setLogLevel, info
from minindn.minindn import Minindn
from minindn.util import MiniNDNCLI
from minindn.apps.app_manager import AppManager
from minindn.apps.nfd import Nfd
from minindn.apps.nlsr import Nlsr

# Persistent MongoDB data root (per host under this dir)
PERSIST_BASE = "/home/fotis/Github/SeEDS/src/minindn/persist/mongo"

HUB_NAME = 'd'  # center node is now 'd' instead of 'c'

if __name__ == '__main__':
    # Hand only the topology to Mini-NDN; keep the rest as our script args
    if len(sys.argv) > 1:
        topo_and_minindn_args = [sys.argv[0], sys.argv[1]]  # script + topology
        my_args = sys.argv[2:]
    else:
        topo_and_minindn_args = sys.argv
        my_args = []

    sys.argv = topo_and_minindn_args

    parser = argparse.ArgumentParser(description="Extra SeEDS options")
    parser.add_argument('--mongo-user', default=None)
    parser.add_argument('--mongo-pass', default=None)
    parser.add_argument('--mongo-db', default=None)
    parser.add_argument('--mongo-uri', default=None)
    parser.add_argument('--log-level', default='debug')
    parser.add_argument('--node-timeout', default='timeout 30s',
                        help='Shell timeout prefix for failing nodes, e.g. "timeout 120s" or "timeout 5m"')
    parser.add_argument('--ipc-is-alive-ms', default='50', help='Frequency of is_alive interest for the resilience poller')
    parser.add_argument('--ipc-meta-ms', default='50', help='Frequency of meta interest for the resilience poller')
    parser.add_argument('--ipc-miss-threshold', default='1', help='Consider node failed after these consecutive misses')
    parser.add_argument('--poller-delay', type=int, default=5,
                        help='Seconds to sleep before starting poller on SECONDARY nodes')
    parser.add_argument('--publish-delay', type=int, default=2,
                        help='Seconds to sleep before publishing HTTP requests on each node')
    args = parser.parse_args(my_args)

    setLogLevel('info')

    Minindn.cleanUp()
    Minindn.verifyDependencies()

    ndn = Minindn()
    ndn.start()

    info('Starting NFD on nodes\n')
    _nfds = AppManager(ndn, ndn.net.hosts, Nfd)
    info('Starting NLSR on nodes\n')
    _nlsrs = AppManager(ndn, ndn.net.hosts, Nlsr)

    # Use HUB_NAME as the hub: point default nfd.sock to host HUB_NAME
    os.system('sudo rm -f /run/nfd/nfd.sock')
    os.system(f'sudo ln -sf /run/nfd/{HUB_NAME}.sock /run/nfd/nfd.sock')

    # Hub stays up; use it as the seed-from broker
    hub_host = next(h for h in ndn.net.hosts if h.name == HUB_NAME)
    center_ip   = hub_host.IP()
    center_port = 10000
    number_of_services = len(ndn.net.hosts)

    # Choose 50% of non-hub nodes to "fail"
    spokes = sorted([h for h in ndn.net.hosts if h.name != HUB_NAME], key=lambda h: h.name)
    fail_count = len(spokes) // 2
    failing_hosts  = spokes[:fail_count]
    staying_hosts  = spokes[fail_count:]   # secondaries

    failing_names = [h.name for h in failing_hosts]
    staying_names = [h.name for h in staying_hosts]

    timeout_prefix = args.node_timeout

    info(f'Failure plan (excluding hub {HUB_NAME}):\n')
    info(f'  will FAIL (PRIMARY): {failing_names}\n')
    info(f'  will STAY  (SECONDARY): {staying_names}\n')
    info(f'  node timeout command: "{timeout_prefix}"\n')
    info(f'  poller start delay: {args.poller_delay}s\n')
    info(f'  publish delay: {args.publish_delay}s\n')

    # Build a deterministic PRIMARY<->SECONDARY pairing (zip shortest)
    pairs = list(zip(failing_hosts, staying_hosts))
    if len(staying_hosts) != len(failing_hosts):
        info(f'  [note] Unpaired nodes exist (count difference={abs(len(staying_hosts)-len(failing_hosts))}); unpaired secondaries will not run a poller.\n')

    # Quick maps for lookups
    name_to_index = {h.name: i for i, h in enumerate(ndn.net.hosts)}
    primary_for_secondary = {sec.name: pri for (pri, sec) in pairs}

    # --- Per-host setup: Mongo (persistent) + Broker + Daemon + tcpdump (+ Poller + HTTP publish)
    for idx, host in enumerate(ndn.net.hosts):
        host_home = f"/home/fotis/Github/SeEDS/src/minindn/tmp/{host.name}"   # volatile logs, etc.
        mongo_dir = f"{PERSIST_BASE}/{host.name}"                              # PERSISTENT data
        node_ip   = host.IP()                                                  # unique Mininet IP
        host.cmdPrint(f'mkdir -p "{host_home}" "{mongo_dir}"')

        # Role & failure plan
        if host.name == HUB_NAME:
            will_fail  = False
            is_primary = False  # hub excluded from poller, remains up
        else:
            will_fail  = host.name in failing_names
            is_primary = True if will_fail else False  # failing spokes are PRIMARY; staying spokes are SECONDARY

        ipc_port = 47832 + idx

        info(f"[role] {host.name}: is_primary={is_primary} will_fail={will_fail}\n")
        info(f"[ipc ] {host.name}: ipc_host={node_ip} ipc_port={ipc_port}\n")

        # Try a clean shutdown of this node's Mongo
        host.cmd(
            'sudo bash -lc '
            f'\'mongod --dbpath "{mongo_dir}" --shutdown >/dev/null 2>&1 || true; '
            f'rm -f "{mongo_dir}/mongod.pid" >/dev/null 2>&1 || true\''
        )

        # Start mongod bound to the node's mininet IP
        ret = host.cmd(
            'sudo bash -lc \''
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

        if "child process failed" in ret or "ERROR:" in ret:
            host.cmdPrint(f'sudo tail -n 200 "{mongo_dir}/mongod.log" || true')
            print(f"[FATAL] mongod failed to start in host {host.name}")
            MiniNDNCLI(ndn.net); ndn.stop(); sys.exit(1)

        host.cmdPrint(f'tail -n 10 "{mongo_dir}/mongod.log" || true')

        # Health check
        ready = host.cmd(
            'sudo bash -lc \'for i in {1..60}; do '
            f'mongosh --quiet --host {node_ip} --port 27017 --eval "db.adminCommand({{ping:1}}).ok" 2>/dev/null | grep -q "^1$" && echo READY && exit 0; '
            'sleep 0.5; '
            'done; echo FAIL; exit 1\''
        ).strip()

        if ready != "READY":
            host.cmdPrint(f'sudo tail -n 200 "{mongo_dir}/mongod.log" || true')
            print(f"[FATAL] mongod not healthy in host {host.name}")
            MiniNDNCLI(ndn.net); ndn.stop(); sys.exit(1)

        # Start broker on this host (bind to node IP, port 10000)
        broker_uri = f'mongodb://{node_ip}:27017/?serverSelectionTimeoutMS=60000'
        host.cmdPrint(
            'cd /home/fotis/Github/SeEDS/build && '
            'sudo ./snds_broker_bin '
            f'--mongo.uri="{broker_uri}" '
            f'--bind-ip={node_ip} '
            '--bind-port=10000 '
            f'--log-level={args.log_level} '
            f'--log-file="{host_home}/Broker.log" '
            f'> "{host_home}/Broker.stdout" 2>&1 &'
        )

        # Build SNDS daemon command
        cmd_core = (
            f'./snds_daemon_bin '
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
            f'--producer.registry-type=CAR_{idx} '
            '--producer.enable-resilience-ipc=true '
            f'--producer.resilience-ipc-host={node_ip} '
            f'--producer.resilience-ipc-port={ipc_port} '
            f'--producer.is-primary={"true" if is_primary else "false"} '
            f'--producer.seed-from-broker-ip={center_ip} '
            f'--producer.seed-from-broker-port={center_port} '
        )

        # Launch SNDS daemon
        if will_fail:
            # Failing nodes run for limited time then exit (simulate failure)
            host.cmdPrint(
                'cd /home/fotis/Github/SeEDS/build && '
                f'( sudo {timeout_prefix} {cmd_core}; '
                'rc=$?; '
                'if [ "$rc" -eq 124 ]; then '
                f'  echo "[TIMEOUT] $(date \"+%Y-%m-%d %H:%M:%S.%3N\") ' + host.name + ': daemon exceeded timeout"; '
                'fi; '
                'exit $rc '
                f') > "{host_home}/log.txt" 2>&1 &'
            )
        else:
            # Staying nodes keep running
            host.cmdPrint(
                'cd /home/fotis/Github/SeEDS/build && '
                f'sudo {cmd_core} > "{host_home}/log.txt" 2>&1 &'
            )

        # For each SECONDARY node (excluding hub), start resilience_poller_bin (after delay)
        if (not will_fail) and (host.name != HUB_NAME):
            primary = primary_for_secondary.get(host.name)
            if primary is None:
                info(f"[poller] {host.name}: no paired PRIMARY, skipping poller.\n")
            else:
                primary_idx = name_to_index[primary.name]
                peer_alive  = f'is_alive_{primary_idx}'
                peer_state  = f'state_{primary_idx}'
                info(f"[poller] {host.name}: will send/listen on {peer_alive},{peer_state} after {args.poller_delay}s delay")

                poller_cmd = (
                    'bash -lc \''
                    f'sleep {args.poller_delay}; '
                    'cd /home/fotis/Github/SeEDS/build && '
                    'sudo ./resilience_poller_bin '
                    '--prefix=SNDS '
                    f'--peer-alive={peer_alive} '
                    f'--peer-state={peer_state} '
                    f'--alive-ms={args.ipc_is_alive_ms} '
                    f'--meta-ms={args.ipc_meta_ms} '
                    f'--miss-threshold={args.ipc_miss_threshold} '
                    f'--ipc-host={node_ip} '
                    f'--ipc-port={ipc_port} '
                    f'--log-level={args.log_level} '
                    f'> "{host_home}/Poller.log" 2>&1 '
                    '&\''
                )
                host.cmdPrint(poller_cmd)


        # Per-host packet capture
        host.cmdPrint(
            f'sudo sh -c \'tcpdump -i any -n -tttt > "{host_home}/packets.txt" 2>&1 &\''
        )

        sleep(2)

    # --- Per-host setup: Mongo (persistent) + Broker + Daemon + tcpdump (+ Poller + HTTP publish)
    for idx, host in enumerate(ndn.net.hosts):
        host_home = f"/home/fotis/Github/SeEDS/src/minindn/tmp/{host.name}"   # volatile logs, etc.
        mongo_dir = f"{PERSIST_BASE}/{host.name}"                              # PERSISTENT data
        node_ip   = host.IP()                                                  # unique Mininet IP
        host.cmdPrint(f'mkdir -p "{host_home}" "{mongo_dir}"')
        # --- After starting daemon (and poller if any), publish HTTP requests on each node ---
        # We wait max(poller_delay, publish_delay) to ensure services are up before publishing.
        pub_wait = max(args.poller_delay, args.publish_delay)
        publish_cmd = (
            'bash -lc \''
            f'sleep {pub_wait}; '
            'URL="http://127.0.0.1:8080"; '
            # First request: id=car{idx}
            f'echo "[PUBLISH] $(date "+%Y-%m-%d %H:%M:%S.%3N") {host.name}: POST id=car{idx}" >> "{host_home}/Requests.log"; '
            f'curl -v -i -X POST "$URL" -d "id=car{idx}" >> "{host_home}/Requests.log" 2>&1; '
            'echo "" >> "{host_home}/Requests.log"; '
            # Second request: id=car{10+idx}&type=CAR_{idx}
            f'echo "[PUBLISH] $(date "+%Y-%m-%d %H:%M:%S.%3N") {host.name}: POST id=car{10+idx}&type=CAR_{idx}" >> "{host_home}/Requests.log"; '
            f'curl -v -i -X POST "$URL" -d "id=car{10+idx}&type=CAR_{idx}" >> "{host_home}/Requests.log" 2>&1; '
            'echo "" >> "{host_home}/Requests.log"; '
            '\''
        )
        host.cmdPrint(publish_cmd)

          # --- Subscription: subscribe only to the "next" index (wrap-around), never to self ---
        next_idx = (idx + 1) % number_of_services
        if next_idx == idx:
            info(f"[subscribe] {host.name}: only one node present; skipping self-subscribe.\n")
        else:
            subscribe_cmd = (
                'bash -lc \''
                'URL="http://127.0.0.1:8080"; '
                f'SUBURL="http://{node_ip}:10000/notify"; '  # <-- change path if needed
                f'echo "[SUBSCRIBE] $(date "+%Y-%m-%d %H:%M:%S.%3N") {host.name}: GET type=CAR_{next_idx} isSub=true suburl=$SUBURL" >> "{host_home}/Subscription.log"; '
                'curl -v -i -G "$URL" '
                f' --data-urlencode "type=CAR_{next_idx}" '
                '  --data-urlencode "isSub=true" '
                '  --data-urlencode "suburl=$SUBURL" '
                f'  >> "{host_home}/Subscription.log" 2>&1; '
                'echo "" >> "{host_home}/Subscription.log"; '
                '\''
            )
            host.cmdPrint(subscribe_cmd)

    # Interactive CLI
    MiniNDNCLI(ndn.net)

    # --- Graceful shutdown ---
    for host in ndn.net.hosts:
        host.cmd('sudo pkill -f "/snds_broker_bin" >/dev/null 2>&1 || true')

    sleep(1)

    for host in ndn.net.hosts:
        mongo_dir = f"{PERSIST_BASE}/{host.name}"
        host.cmdPrint(
            'sudo mongod '
            f'--dbpath "{mongo_dir}" '
            '--shutdown '
            '>/dev/null 2>&1 || true'
        )
        host.cmd(f'sudo rm -f "{mongo_dir}/mongod.pid" >/dev/null 2>&1 || true')

    ndn.stop()
