# arras4_core
This repository contains the C++ libraries implementing Arras. The libraries are required by the example
application arras_render, and by the MoonRay Hydra plugin HdMoonray.

This repository is part of the larger MoonRay/Arras codebase.  It is included as a submodule in the top-level
OpenMoonRay repository located here: [OpenMoonRay](https://github.com/dreamworksanimation/openmoonray)

- **arras4_message_api** The API used to manipulate messages and create new message types.
- **arras4_computation_api** The API used to implement new computations.
- **arras4_client** The APIs used to implement Arras clients.
- **arras4_log** The Arras logging system.
- **arras4_network** Implementation of INET and IPC sockets, and associated buffering. HTTP client and server support.
- **arras4_core_impl** Implementation of Arras messaging and computation host using sockets.
- **arras4_test** Test messages, computations and clients for Arras.

