// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "MessageWriter.h"

#include "OpaqueContent.h"
#include "Envelope.h"

#include <exceptions/InternalError.h>

#include <message_api/ObjectContent.h>
#include <message_api/ContentRegistry.h>
#include <message_api/MessageFormatError.h>

#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>

#include <network/Peer.h>
#include <network/DataSink.h>

using namespace arras4::network;

namespace arras4 {
    namespace impl {

MessageWriter::MessageWriter(AttachableBufferSink& sink,
                             const std::string& traceInfo)
    : mSink(sink),
      mOutStream(sink),
      mTraceInfo(traceInfo),
      mIsAutosaving(false)
{
}

void MessageWriter::enableAutosave(const std::string& directory)
{
    mAutosaveDirectory = directory;
    mIsAutosaving = true;
}

void MessageWriter::disableAutosave()
{
    mIsAutosaving = false;
}

MessageWriter::~MessageWriter()
{
}

// Implementation of a class to write messages to DataSink
//
// The sink must be buffered and framed, and the writer places a 
// complete message in each frame.
//
// The message content must be either an ObjectContent subclass or
// an OpaqueContent instance. ObjectContent is serialized into
// the DataSink using an OutStreamImpl wrapper. Buffering allows
// the data to be sent out efficiently even though the ObjectContent
// serialize function may be delivering it in small chunks. It also
// makes the BasicFraming protocol (which requires an upfront frame 
// size) possible. 
//
// OpaqueContent holds a buffer that wasn't actually deserialized
// by the MessageReader that read it. Since this opaque data is already in
// a memory buffer and doesn't require serialization, we use an optimized
// path in BufferedSink that accepts pre-buffered data. This avoids an
// additional copy operation.
void MessageWriter::write(const Envelope& env)
{  
   
    // start a new message frame. Autoframed allows us to leave out the frame size, 
    // since it can fill in the size itself when it sends on the frame
    mSink.openFrame();

    // a flag in the metadata indicates whether this message
    // should be traced
    bool trace = env.metadata() && env.metadata()->trace();

    // serialize the envelope into the frame, via
    // our DataOutStream wrapper
    env.serialize(mOutStream);
    mOutStream.flush();

    // find out what type of content we are dealing with...
    const api::MessageContent* content = env.content().get();   
    const OpaqueContent* opaqueContent = 
        dynamic_cast<const OpaqueContent*>(content);
        
    if (opaqueContent) {

        // if the content is opaque, it is already buffered and
        // we can send it directly to the peer without copying
        // it into the frame

        mSink.appendBuffer(opaqueContent->dataBuffer());       
    
    } else {
    
        const api::ObjectContent* objectContent =
            dynamic_cast<const api::ObjectContent*>(content);

        if (objectContent) {

            if (trace) {
                ARRAS_ATHENA_TRACE(0,log::Session(env.metadata()->from().session.toString()) <<
                                   "{trace:message} serializing " << env.metadata()->instanceId().toString() << " "
                                   << mTraceInfo << " 0"); // 0 is "size" for consistent syntax...
            }
            // ObjectContent subclasses are serializable, so serialize
            // the object into our sink buffers right after the metadata, 
            // again using out DataOutStream wrapper
            objectContent->serialize(mOutStream);
            mOutStream.flush(); 
        
        } else {

            throw api::MessageFormatError(
                "Unknown message content format : Object or Opaque content expected");
        }
    }
        
    // Message tracing : following closeFrame triggers send of the message
    if (trace) {
        ARRAS_ATHENA_TRACE(0,log::Session(env.metadata()->from().session.toString()) <<
                           "{trace:message} sending " << env.metadata()->instanceId().toString() << " "
                           << mTraceInfo << " " << std::hex <<  mSink.bytesWritten());
    }

    if (mIsAutosaving)
        doAutosave();

    mSink.closeFrame();       
}
 
void MessageWriter::doAutosave()
{
    std::string filepath = mAutosaveDirectory + "/" + 
        api::ArrasTime::now().filenameStr() + ".msg";
    bool ok = mSink.writeToFile(filepath);
    if (ok) {
        ARRAS_DEBUG("Wrote message to file " << filepath);
    } else {
        ARRAS_WARN(log::Id("messageSaveFailed") << "Failed to write file " << filepath);
    }
}

}
}
