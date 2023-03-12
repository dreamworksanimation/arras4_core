# arras4_computation_api

## Computation API for Arras 4


The **Computation** class defines the interface between a user-written computation and Arras. It contains two kinds of functions:

- pure virtual functions that a user-written subclass must override. For example: "onMessage", which allows the computation to receive incoming messages.

- functions that call pure virtual functions, providing Arras services to the user-written computation. For example: "send", which allows the computation to send an outgoing message.

**ComputationEnvironment** is the interface class for the second kind of function. It is subclassed by Arras itself to implement the computation's host environment. See *arras4_core_impl/computation_impl* for the implementation.

**Logger** is an interface to Arras' logging system.
*version.h* provides the current computation api version as a constant string.
