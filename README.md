# terminalconnect
An application that allows two users at two different terminals to "chat". 

Coded in C, with pthreads, UDP, with a client-server model.

Making use of a shared List ADTs between 4 kernel-level pthreads.

Each thread has a seperate function. 

The first p-thread awaits for input from the keyboard (user).

The second p-thread awaits for data from the network.

THe third p-thread is responsible for sending datagrams to the other (client).

The fourth p-thread is solely responsible for displaying information to the screen.

# usage:

s-talk [my port number] [remote machine name] [remote port number]

# example:

Client 1: s-talk 5000 10.0.0.2 5002
Client 2: s-talk 5002 10.0.0.1 5000


