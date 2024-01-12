// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_REGISTRATIONDATA_H__
#define __ARRAS_REGISTRATIONDATA_H__

#include <message_api/UUID.h>

namespace arras4 {
    namespace impl {

        enum RegistrationType {
            REGISTRATION_CLIENT,
            REGISTRATION_NODE,
            REGISTRATION_EXECUTOR,
            REGISTRATION_CONTROL,

            REGISTRATION_INVALID = 0xFFFFFFFE,

            REGISTRATION_SIZE = 0xFFFFFFFF
        };

        struct RegistrationData {
            typedef uint64_t MagicType;
            static const MagicType MAGIC = 0x0104020309060201L;
            uint64_t mMagic;
            unsigned short mMessagingAPIVersionMajor;
            unsigned short mMessagingAPIVersionMinor;
            unsigned short mMessagingAPIVersionPatch;
            unsigned short mReserved; // for alignment
            
            // changes above this point may break protocol version checking

            api::UUID mSessionId;
            api::UUID mNodeId;
            api::UUID mComputationId;
            int mType;

            RegistrationData(unsigned short major, unsigned short minor, unsigned short patch) :
                mMagic(MAGIC),
                mMessagingAPIVersionMajor(major),
                mMessagingAPIVersionMinor(minor),
                mMessagingAPIVersionPatch(patch),
                mReserved(0) {};

        };

}
}

#endif 

