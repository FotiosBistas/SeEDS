import sys
import configparser
import logging
import os

from typing import List

from mininet.topo import Topo
from mininet.log import MininetLogger
from mininet.node import Host

# Define a new logging format
standard_logging = '%(asctime)s - %(name)s - %(levelname)s - %(message)s'

_logger = MininetLogger(os.path.basename(__file__))
#TODO hardcoded log level
_logger.setLogLevel('debug')
for handler in _logger.handlers:
    handler.setFormatter(logging.Formatter(standard_logging))

class TopologyWrapper(Topo):
    def __init__(
        self,
        extra_host_options: dict=None,
        extra_link_options: dict=None,
        extra_switch_options: dict=None,
    ):
        super().__init__(
            hopts=extra_host_options, 
            lopts=extra_link_options, 
            sopts=extra_switch_options,
        )
        # This gets filled in add_host and will contain a structure:
        #'host_name'= {
        #    'host_name': hosts_name,
        #    'host_ip': host_ip,
        #}
        self.hosts_dictionary={}

        # This gets filled in add_switch and will contain a structure:
        # 'switch_name': 'switch_name'
        self.switches_dictionary={}

        # Get filled in add_mininet_hosts when minindn is created
        # 'host.name': 'mininethost object'
        self.mininet_hosts = {}

        # Gets filled in add_port
        self.all_ports_dictionary = {}
        
        # Map edge nodes to IP addresses
        self.edge_nodes = {}

    def run_command_on_mininet_host(self, host_name:str, command:str, verbose: bool=True) -> str:
        _logger.info(f"Running command: {command} on host: {host_name}\n")
        host = self.mininet_hosts.get(host_name, None)

        if host is None:
            _logger.error(f"Host {host_name} is not found in mininet_hosts\n")
            return "Command was not run, because host is not in topology"

        result = host.cmdPrint(command)
        _logger.debug(f"Result after running: {command} on host: {host_name}\nResult: {result}\n")

        return result