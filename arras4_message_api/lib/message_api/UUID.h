// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_UUID__
#define __ARRAS4_UUID__

#include <string>
#include <array>
#include <iostream>
#include <algorithm>

namespace arras4 {
    namespace api {

// wraps a 16-byte guid.
// this is largely compatible with arras3 UUID,
// but direct access to byte data has been added, 
// and the const char* constructor has been removed

class UUID {

public:
            
    UUID() : mBytes{0} {}
    UUID(const std::array<unsigned char, 16> &bytes) : mBytes(bytes) {}
    UUID(const UUID& other) : mBytes(other.mBytes) {}
    UUID(const std::string& aStrRep) { parse(aStrRep); }

    UUID& operator=(const UUID& other);

    bool operator<(const UUID& other) const 
        { return mBytes < other.mBytes; }
    bool operator!=(const UUID& other) const
        { return mBytes != other.mBytes; }
    bool operator==(const UUID& other) const
        { return mBytes == other.mBytes; }
      
    void clear() { std::fill(mBytes.begin(), mBytes.end(), 0); }
    bool valid() const { return *this != null; }
    explicit operator bool() const
        { return valid(); }
    bool isNull() const { return !valid(); }

    void toString(std::string& aStr) const;
    std::string toString() const;
    void parse(const std::string& aStrRep);
    
    
    const std::array<unsigned char, 16>& bytes() const
        { return mBytes; }
    std::array<unsigned char,16>& bytes()
        { return mBytes; }                                        
    void setBytes(const std::array<unsigned char, 16> &bytes)
        { mBytes = bytes; }

    void regenerate(); // can't have same name as static function...

    static UUID generate();
    static const UUID null;

private:

    std::array<unsigned char,16> mBytes;

    friend std::ostream &operator<<(std::ostream &s, const UUID &uuid);
};

}
}

#endif 

