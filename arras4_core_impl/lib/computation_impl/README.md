# computation_impl

This library provides an implementation of a computation host environment,
conforming to the Arras 4 computation api.

This particular implementation bridges a computation with a framed socket. Incoming messages on the socket are routed in to the computation's "onMessage" function. Messages that the computation sends are serialized out through the socket.

The computation has to be able to handle and send messages at its own pace. To ensure that the sockets are used efficiently, the environment uses a MessageDispatcher (from the shared_impl library). This buffers both incoming and outgoing messages in queues. If the computation falls behind in handling incoming messages, they will sit on the queue until the computation can catch up. Similarly, if it is sending out messages faster than they can be transmitted via the socket, they will accumlate on the outgoing queue. However, if either of these rate differences is sustained for too long, the queue will continue to grow and eventually cause very large latencies.

The environment is also responsible for loading the computation code in dso form, and calling the canonical function (_create_computation) to construct a computation instance. It does this using the ComputationHandle helper class.

