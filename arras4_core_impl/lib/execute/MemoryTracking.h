// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_MEMORYTRACKING_H__
#define __ARRAS_MEMORYTRACKING_H__

namespace arras4 {
namespace impl {

class MemoryTracking {
  public:
    MemoryTracking() :
        mAvailableMb(0),
        mReservedMb(0),
        mBorrowedMb(0) {};

    // the amount of memory isn't known at construction time so needs to be
    // set in a separate function but it will only be set once.
    void set(unsigned int available) {
        std::lock_guard<std::mutex> lock(mMemoryMutex);

        mAvailableMb = available;
        mReservedMb = 0;
        mBorrowedMb = 0;
    }

    // Note that a computation with a formal reservation is using the memory
    // If the return value is the requested amount then the reservation is in
    // place. If the return value is less that the amount of the reservation
    // then the reservation isn't in place and the return value is the amount
    // that was actually available. Because coordinator won't send a computation
    // that we can't handle a shortfall will always be due to borrowers.
    unsigned int reserve(unsigned int amount) {
        std::lock_guard<std::mutex> lock(mMemoryMutex);

        if ((mReservedMb + mBorrowedMb + amount) <= mAvailableMb) {
            mReservedMb += amount;
            return 0;
        } else {
            return amount - (mAvailableMb - mReservedMb - mBorrowedMb);
        }
    }

    // note that a computation with a formal reservation is done using memory
    void release(unsigned int amount) {
        std::lock_guard<std::mutex> lock(mMemoryMutex);

        mReservedMb -= amount;
    }

    // Note that a computation needs memory beyond its reservation
    // The return value is the amount that was actually borrowed.
    // Currently its all or nothing but it API allows for an intermediate
    // value to be returned.
    unsigned int borrow(unsigned int amount) {
        std::lock_guard<std::mutex> lock(mMemoryMutex);

        if ((mReservedMb + mBorrowedMb + amount) <= mAvailableMb) {
            mBorrowedMb += amount;
            return amount;
        } else {
            return 0;
        }
    }

    // note that a computation using memory beyond its reservation is
    // done with it
    void repay(unsigned int amount) {
        std::lock_guard<std::mutex> lock(mMemoryMutex);

        mBorrowedMb -= amount;
    }

  private:
    unsigned int mAvailableMb;  // the amount of memory to be used by computations
    unsigned int mReservedMb;   // the amount of memory reserved for running computations
    unsigned int mBorrowedMb;   // the amount of memory tentatively handed out
    mutable std::mutex mMemoryMutex;
};

} // end namespace node
} // end namespace arras4

#endif // __ARRAS_MEMORYTRACKING_H__

