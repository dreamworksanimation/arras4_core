# shared_impl

This library provides higher-level functions built on top of the core messaging implementation.

**`MessageQueue`** is a concurrent implementation of a FIFO queue. It is used in the implementation of `MessageDispatcher`.

**`MessageDispatcher`** provides a queue-buffered interface to the socket message support in message_impl. A client calls 'send' to send a message, which is queued until the socket is available to write it. The dispatcher calls the 'handleMessage' function to pass an incoming message to the client : this is called serially on a single thread, and messages are queued until it is their turn to be handled.

