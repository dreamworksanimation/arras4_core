// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "MessageReader.h"

#include "OpaqueContent.h"
#include "Envelope.h"

#include <exceptions/InternalError.h>
#include <message_api/ObjectContent.h>
#include <message_api/ContentRegistry.h>

#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>

#include <network/Peer.h>
#include <network/DataSource.h>

using namespace arras4::network;

namespace arras4 {
    namespace impl {

MessageReader::MessageReader(DetachableBufferSource& source,
                             const std::string& traceInfo)
    : mSource(source),
      mInStream(source),
      mTraceInfo(traceInfo),
      mIsAutosaving(false)
{
}

void MessageReader::enableAutosave(const std::string& directory)
{
    mAutosaveDirectory = directory;
    mIsAutosaving = true;
}

void MessageReader::disableAutosave()
{
    mIsAutosaving = false;
}

MessageReader::~MessageReader()
{
}

// Implementation of a class to read messages from a DataSource
//
// The source must be buffered and framed, and the reader assumes that 
// each frame represents one complete message. The reader then interprets
// the frame data as a message and returns it, with appropriately
// deserialized content data. Generally the content is delivered
// using a subclass of ObjectContent registered with the ContentRegistry.
//
// If there is no ObjectContent class registered that can deserialize the
// content data, the reader delivers it using the OpaqueContent class. 
// This simply holds on to the data in serialized form. The only things 
// a client can do with OpaqueContent is send it out to a MessageWriter 
// or delete it. Therefore this case is useful for transferring the
// message across to someone else, but not for actually acting on the 
// content.
//
// If useRegistry is 'false', the content is always delivered using
// OpaqueContent. This is useful for processes like 'node', whose only
// purpose is to route messages from one socket to another.
//
// To avoid copying opaque content data (which could be large), we actually 
// transfer ownership of the original buffer to the OpaqueContent class. 
// It is this optimization that leads to the requirement that the input
// source be buffered. See MessageWriter for the special handling that
// allows it to recover the opaque data buffer and send it efficiently.
//
// 
// If the data source times out before a message is available,
// returns an empty envelope
Envelope MessageReader::read(bool useRegistry)
{
    // get the next frame from our source. This will be an
    // entire message.
   
    size_t frameSize = mSource.nextFrame();
    if (frameSize == 0) {
        return Envelope(); // timeout
    }

    if (mIsAutosaving)
        doAutosave();

    // deserialize envelope from the source buffer,
    // via mInStream

    api::ClassID classId;
    unsigned version;
    Envelope env;
    env.deserialize(mInStream,classId,version);

    // a flag in the metadata indicates whether this message
    // should be traced
    bool trace = env.metadata() && env.metadata()->trace();

    if (trace) {
        ARRAS_ATHENA_TRACE(0,log::Session(env.metadata()->from().session.toString()) <<
                           "{trace:message} received " << env.metadata()->instanceId().toString() << " "
                            << mTraceInfo << " " << std::hex << frameSize);
    }

    // if 'useRegistry' is true, try to instantiate content
    // object using the content registry

    api::MessageContent* content = nullptr;
    if (useRegistry) {
        api::ObjectContent* objContent = api::ContentRegistry::singleton()->
            create(classId,version);
        if (objContent) {
            objContent->deserialize(mInStream,version);
            if (trace) {
                ARRAS_ATHENA_TRACE(0,log::Session(env.metadata()->from().session.toString()) <<
                                   "{trace:message} deserialized " << env.metadata()->instanceId().toString() << " "
                                   << mTraceInfo << " " << std::hex << frameSize);
            }
            content = objContent;
        }
    }

    // if we didn't/couldn't create a content object, fall back
    // to OpaqueContent. The calling code can't look inside
    // this, but it can pass it back to MessageWriter to send out again, or
    // simply delete it...

    if (!content) {

        // OpaqueContent grabs the entire buffer from mSourceBuffer, and
        // uses this as its content representation. We are taking more data
        // than necessary, because the buffer still holds the message metadata
        // at the beginning. However, the buffer will have its start offset
        // set to the start of the content data. Doing this avoids copying
        // the entire message content in the case we just want to transfer it
        // to another place.
        // mSourceBuffer will automatically reallocate on the next call 
        // to resetForFill()

        BufferConstPtr data = mSource.takeBuffer();
        content = new OpaqueContent(classId,version,data);
    }
    
    mSource.endFrame();
    env.setContent(content);
    return env;
}
 
// deserialize message after the fact
// static
bool MessageReader::deserializeContent(Envelope& env)
{
    OpaqueContent::ConstPtr oc = env.contentAs<OpaqueContent>();
    if (oc) {

        api::ObjectContent* objContent = api::ContentRegistry::singleton()->
            create(env.classId(),env.classVersion());
        if (objContent) {
            // deserialize from the OpaqueContent's buffer 
            const BufferConstPtr& ocBuf = oc->dataBuffer();
            // we're going create a new buffer to avoid modifying the read pointer
            // in the original one, but there is no "ConstBuffer" class
            unsigned char* data = const_cast<unsigned char*>(ocBuf->start());
            Buffer* source = new Buffer(data,
                                        ocBuf->remaining(),
                                        ocBuf->remaining());
            InStreamImpl inStream(*source);
            objContent->deserialize(inStream,env.classVersion());
            env.setContent(api::MessageContentConstPtr(objContent));
            return true;
        }
        return false;
    }
    return true; // was already deserialized
}

void MessageReader::doAutosave()
{
    std::string filepath = mAutosaveDirectory + "/" + 
        api::ArrasTime::now().filenameStr() + ".msg";
    bool ok = mSource.writeToFile(filepath);
    if (ok) {
        ARRAS_DEBUG("Wrote message to file " << filepath);
    } else {
        ARRAS_WARN(log::Id("messageSaveFailed") << "Failed to write file " << filepath);
    }
}


}
}
