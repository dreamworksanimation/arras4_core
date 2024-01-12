// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_COMPUTATION_DSOH__
#define __ARRAS4_COMPUTATION_DSOH__

#include <string>
#include <exceptions/InternalError.h>

namespace arras4 {
    namespace api {
        class Computation;
        class ComputationEnvironment;
    }
}

namespace arras4 {
    namespace impl {


class ComputationHandle
{
public:

    // throws ComputationLoadError
    ComputationHandle(const std::string& dsoName,
                      api::ComputationEnvironment* env);

    ~ComputationHandle();

    api::Computation* operator->() {
        return mComputation;
    }

private:
    void *mDso;
    api::Computation* mComputation;
};

}
}
#endif
