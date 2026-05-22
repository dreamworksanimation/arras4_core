# arras4_core - part of the [MoonRay](https://github.com/OpenMoonRay/openmoonray) project
Policies concerning [Governance](https://github.com/OpenMoonRay/openmoonray/blob/main/GOVERNANCE.md), [Code of Conduct](https://github.com/OpenMoonRay/openmoonray/blob/main/CODE_OF_CONDUCT.md), and [Contribution](https://github.com/OpenMoonRay/openmoonray/blob/main/CONTRIBUTING.md) are available in the overarching MoonRay project, defined in the [`OpenMoonRay/openmoonray` GitHub repository superproject](https://github.com/OpenMoonRay/openmoonray).

This repository contains the C++ libraries implementing Arras. The libraries are required by the example
application arras_render, and by the MoonRay Hydra plugin HdMoonray.

- **arras4_message_api** The API used to manipulate messages and create new message types.
- **arras4_computation_api** The API used to implement new computations.
- **arras4_client** The APIs used to implement Arras clients.
- **arras4_log** The Arras logging system.
- **arras4_network** Implementation of INET and IPC sockets, and associated buffering. HTTP client and server support.
- **arras4_core_impl** Implementation of Arras messaging and computation host using sockets.
- **arras4_test** Test messages, computations and clients for Arras.

