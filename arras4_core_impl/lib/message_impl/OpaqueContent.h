// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_OPAQUE_CONTENTH__
#define __ARRAS4_OPAQUE_CONTENTH__


#include <network/network_types.h>
#include <network/Buffer.h>

#include <message_api/MessageContent.h>
#include <message_api/UUID.h>



namespace arras4 {
    namespace impl {

// OpaqueContent is a content representation used
// to transfer message data when you don't need to read it.
// It holds a shared pointer to a Buffer object, which may be
// longer than the content data, but has the start and end
// values set appropriately.
class OpaqueContent : public api::MessageContent 
{
public:
    OpaqueContent(const api::ClassID& classId,
                  const unsigned& version,
                  const network::BufferConstPtr& data) :
        mClassId(classId), 
        mVersion(version),
        mData(data)
    {}
    ~OpaqueContent() {}

    const arras4::api::ClassID& classId() const { return mClassId; }
    unsigned classVersion() const { return mVersion; }
    const std::string& defaultRoutingName() const { 
        static std::string name("OpaqueContent-INVALID_NAME"); return name; }
    arras4::api::MessageContent::Format format() const {
        return arras4::api::MessageContent::Opaque; }

    const network::BufferConstPtr& dataBuffer() const { return mData; }

    typedef std::shared_ptr<OpaqueContent> Ptr;
    typedef std::shared_ptr<const OpaqueContent> ConstPtr;

private:
    api::ClassID mClassId;
    unsigned mVersion;
    network::BufferConstPtr mData;
    
};
        
}
}
#endif
