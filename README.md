ffton
=====

Fast File Transfer over NAT.

What is it?
-----------

It can transfer files between Linux machines as scp does. The transfer speed is generally much faster than scp, although depending on machines and networks. ffton works much better than scp particularly when the bandwidth is wide and the network latency is high. TCP does not work well with wider bandwidth and higher latency, so ffton uses UDP instead of TCP. The special congestion control provided by libudt does all the trick, and ffton wraps it for user-friendliness. Although most outgoing UDP packets are often filtered by NAT firewall in supercomputer centers, ffton (libudt) can pass it by UDP hole punching if possible.

Prerequisites
-------------

- Python 2.4 or later
- [libudt](http://udt.sourceforge.net/ "libudt") ver 4.x
- C++ compiler such as g++

Installation
------------

ffton uses waf. Just extract the tarball (or clone from the repository), chdir to the directory, and configure::

    % ./waf configure

and then build::

    % ./waf build
	
and install::

    % ./waf install

That's it.

If you do not have root privilege, use --prefix to install it under the home directory::

    % ./waf configure --prefix=$HOME/local

If you installed libudt using ./configure --prefix=$HOME/local for the same reason, then you must also specify --with-udt::

    % ./waf configure --with-udt=$HOME/local --prefix=$HOME/local

Note that you have to install ffton also on the remote side. If you copy a file from host A to host B, then the both hosts must have ffton installed.

Set up
------

Every user of ffton must first run ffton with --init option::

    % ffton --init
	
The initialization step takes a couple of minutes, so please be patient. If ffton says that you are using 'Symmetric UDP Firewall', then you are likely using a firewall that cannot be passed by UDP hole punching so ffton would not work; otherwise you are all set! You need to do this step on the both sides.

How to use ffton
----------------

Use it as if ffton is a drop-in replacement of scp. For example, let's copy 'abc.txt' in the current directory to /tmp on the hogehoge.example.com::

    % ffton abc.txt myuser@hoge.example.com:/tmp

You can also do the reverse::

    % ffton myuser@hoge.example.com:/tmp/abc.txt .

It is fairly easy if you already have an experince with scp. Just a note for scp users, ffton does not accept wildcards on the remote side, so you cannot do the following although scp works completely fine with it::

    % ffton 'myuser@hoge.example.com:/tmp/*.txt' .

In addition, ffton does not have -r option, so you cannot do recursive copy::

    % ffron -r hugedatadir/ myuser@hoge.example.com:
	
because I haven't implemented it yet.
	
Limitation
----------

ffton does not encrypt files, so do not transfer secret files at the moment.
ffton does not accept wildcards because the author has not implemented it yet.
ffton cannot transfer small files fast although it can potentially do much better in the future.
ffron cannot resume file transfer although it should be able to.
ffton should be able to transfer files between two remote nodes, but the author has not implemented yet.
ffton should be able to handle double SSH (i.e., ssh to B from A, then ssh to C from B, and communicate between A and C) but not yet.

Tips and notes
--------------

ffton requires a STUN server to identify the external IP of your machine. It points to the public STUN server in The University of Tokyo by default, but if you do not prefer using it or when it is unavailable due to maintenance, modify the list 'public_stun_servers' in ffton.

If you have a root access on the both sides, just use [High Performance SSH/SCP](http://www.psc.edu/networking/projects/hpn-ssh/) and you would be happy with it. Another option might be [Aspera](http://www.asperasoft.com/) if you can pay money and there is no firewall between the both sides. Neither conditions were met in my case so I developed ffton.

License
-------

The modified BSD license.

Author
------

Masahiro Kasahara

