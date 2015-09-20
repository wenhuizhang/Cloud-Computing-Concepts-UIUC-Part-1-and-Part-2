Read the specification document thoroughly.

Create a high level design covering all scenarios / test cases before you start coding. 

How do I run only the CRUD tests ? 

$ make clean
$ make
$ ./Application ./testcases/create.conf
or 
$ ./Application ./testcases/delete.conf
or
$ ./Application ./testcases/read.conf
or
$ ./Application ./testcases/update.conf

How do I test if my code passes all the test cases ? 
Run the grader. Check the run procedure in KVStoreGrader.sh

 A key-value store supporting CRUD operations (Create, Read, Update, Delete).
 Load-balancing (via a consistent hashing ring to hash both servers and keys).
 Fault-tolerance up to two failures (by replicating each key three times to three successive nodes
		in the ring, starting from the first node at or to the clockwise of the hashed key).
 Quorum consistency level for both reads and writes (at least two replicas).
 Stabilization after failure (recreate three replicas after failure).
