# message_impl

This library provides a concrete in-memory representation of messages,
conforming to the Arras 4 message api.

**`MessageImpl`** is the subclass of `api::Message` providing the memory representation.

---

**`Arras4MessageWriter`** provides the ability to serialize a `MessageImpl` to a socket with a framing protocol. Encoding of primitive values is implemented by `InStream` in `StreamImpl.h/cc`. Buffering for the send process is provided by the `SinkBuffers` class.

**`Arras4MessageReader`** provides the corresponding deserialization from a framed socket. Similarly, `OutStream` provides primitive decoding and `SourceBuffer` provides buffering.

---

The socket/framing code used is in the arras_common library (`Peer/Framing` classes). This maintains wire compatibility with Arras 3, but imposes some limitations, particularly:

- Total message size is limited to ~2Gb by the framing protocol
- `Arras4MessageReader` is blocking and cannot be cleanly interrupted without disconnecting the socket.

---

**`OpaqueContent`** provides a implementation of `MessageContent` that can be used with any message. It has a special relationship with the reader/writer code that allows it to more efficiently transfer messages opaquely through a process (relative to fully deserialing/reserializing them)
