# Copyright (C) 2013 University of Texas at Austin
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


# next steps:
# does lock block second task?
# add change back to not running at end of task
# write code in myproj to build hosts.txt
# run test on stampede

import fcntl, os, sys
import rsf.path

def ljust(string):
    'left-justified record'
    return string.ljust(40)

class Hosts(object):
    
    def __init__(self,hosts_file=None):
        self._len = 40 # 
        
        if not hosts_file:
            # try environment first
            hosts_file=os.environ.get('RSF_HOSTS')
        if not hosts_file:
            # let us create it
            path = rsf.path.getpath(os.getcwd())
            hosts_file = os.path.join(path,'hosts.txt')

            try:
                if (os.path.isfile(hosts_file)):
                    os.unlink(hosts_file)
                hosts_fd=open(self.hosts,'w')
            except:
                sys.stderr.write('Trouble opening hosts file "%s"\n' % hosts_file)
                sys.exit(1)
        
            cluster = os.environ.get('RSF_CLUSTER')
            if cluster:
                sys.stderr.write('RSF_CLUSTER is getting deprecated.\n')
            else:
                cpu = rsf.path.cpus()
                cluster = 'localhost %d' % cpu

            hosts = cluster.split()
            nodes = []
            for i in range(1,len(hosts),2):
                nh = int(hosts[i])
                nodes.extend([hosts[i-1]]*nh)
            
            hosts_fd.write("numnodes %4d\n" % len(self.nodes))
            hosts_fd.write(ljust('host')+'state\n') 
            for host in self.nodes:
                hosts_fd.write(ljust(host)+'notrunning\n')
            hosts_fd.close()
         
        try:
            self.hosts_fd=open(hosts_file,'r+')
        except:
            sys.stderr.write('Trouble opening hosts file "%s"\n' % hosts_file)
            sys.exit(1)

        #hosts.txt file contains:
        # line 1 is "numnodes" space and number nodes in i4 format with leading 0's
        # line 2 is headers "host" and "state"
        # line 3 through numnodes+2 is host names (40 char long blank padded) and
        #        either notrunning or running.  record is padded to 80 characters 
        # fields are fixed length so when state changes the file size does not change

        self.host = None

    def start(self):
        'lock hosts.txt, get notrunning node, mark it running, unlock hosts.txt'
        
        fcntl.lockf(self.hosts_fd,fcntl.LOCK_EX)

        # get notrunning node, mark it running
        hosts_txt=self.hosts_fd.readlines()
        self.numnodes=int(hosts_txt[0].split()[1])

        idle_node=-1
        for node in range(self.numnodes):
            if(hosts_txt[2+node].split()[1]=='notrunning'):
                idle_node=node
                break
            
        if -1==idle_node:
            sys.stderr.write('sfnode: Looking for an idle node in hosts.txt,\n'
                             'but all %d nodes are running.' % self.numnodes) 
            sys.exit(1)
    
        self.host=hosts_txt[2+idle_node].split()[0]
        hosts_txt[2+idle_node]=ljust(self.host)+'running\n'

        self.hosts_fd.seek(0)   
        for line in hosts_txt:
            self.hosts_fd.write(line)
        self.hosts_fd.flush()   # need to do this or file does not change

        fcntl.lockf(self.hosts_fd,fcntl.LOCK_UN)
        #print "hosts_fd.txt unlocked"

    def task(self,command):
        'command to run on the available host'
        
        if not self.host:
            sys.stderr.write('Need to run start first\n')
            sys.exit(3)

        if self.host=='localhost':
            task=command
        else:
            task="ssh "+self.host+' \"'+command+' \"'

        return task

    def action(self,target=None,source=None,env=None):
        'SCons action'
        command = env.get('command')
        task = self.task(command)
        print task
        return os.system(task)

    def stop(self):
        'lock hosts.txt, mark the node notrunning, unlock hosts.txt'

        if not self.host:
            sys.stderr.write('Need to run start first\n')
            sys.exit(3)
        
        fcntl.lockf(self.hosts_fd,fcntl.LOCK_EX)
        self.hosts_fd.seek(0)   
        hosts_txt=self.hosts_fd.readlines()
        hosts_txt[2+idle_node]=ljust(self.host)+'notrunning\n'

        self.hosts_fd.seek(0)   
        for line in hosts_txt:
            self.hosts_fd.write(line)
        self.hosts_fd.flush()   # this will actually mode buffer to output file

        fcntl.lockf(self.hosts_fd,fcntl.LOCK_UN)

    def __del__(self):
        self.hosts_fd.close()

if __name__ == "__main__":
    hosts = Hosts(sys.argv[1])

    hosts.start()
    task = hosts.task(' '.join(sys.argv[2:]))
    os.system('date')
    print task
    os.system(task)
    hosts.stop()

    sys.exit(retcode)
