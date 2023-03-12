# arras4_core_impl

Implementations of the message and computation APIs

This package provides the following libraries

- **chunking** implements the ability to split very large messages into smaller chunks. This is required to support messages more than ~2Gb in size.
- **computation_impl** implements an environment that can host a computation, handling startup and message send/receive via *message_impl* and *routing*.
- **core_messages** provides a set of messages used internally by the implementation.
- **exceptions** defines some exception class that the implementation throws.
- **execute** is a library to start and manage processes, used in the implementation of local Arras (in the client) and by arras4_node
- **message_impl** implements the Arras 4 message api, providing classes that can read/write messages from/to sockets and memory buffers.
- **routing** implements routing of messages, providing an *Addresser* class that can set the correct destinations for an outgoing message based on a computation map.
- **shared_impl** supports threaded queue and dispatch of messages, required by several different components.

**execComp** is the main host program for computation classes. It instantiates a Computation subclass provided in a DSO and runs it in an environment provided by *computation_impl*. Message queues are provided by *shared_impl*, and outgoing message routing by *routing*. The hosted computation communicates with the outside world through an IPC socket : typically this is connected to either arras4_node (for distributed Arras) or directly to the client (local mode).

**runComp** is a variant of *execComp* that runs the computation without the node control protocol (i.e. the 'go','stop','engine_ready' messages used to coordinate session startup), and so is mostly only useful for testing or running single computations.

**msgInfo** and **msgPlay** are diagnostic tools used with recorded messages.

---

There are simple test implementations of a message and a computation. To run a test example, use two terminals. In the first run
```
rez-env arras4_core_impl
runComp test_config.json /tmp/a4test
```
then, in the other:
```
rez-env  arras4_core_impl
rcTestClient /tmp/a4test
```
*'test_config.json' is in the repo /test/rcTestClient/test_config.json*

*'/tmp/a4test' is a arbitrary IPC socket address to connect them*

This test sends 10 test messages to the test computation, which in turn echos them back to the client with ' -- reply' appended.

