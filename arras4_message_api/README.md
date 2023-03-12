# arras4_message_api

## Messaging API for arras 4
 

arras4_message_api provides the message API itself, plus some core datatypes that are used almost everywhere in Arras:


- **ArrasTime** : microsecond resolution absolute time and time interval
- **UUID** : 16 byte guid, implemented using libuuid
- **Address** : 3 UUID values that together provide an address for an arras computation (being the source or destination of a message)
-- **Object** : a dynamically typed object

*Object* is actually a typedef for the *Value* class provided by the third party **jsoncpp** library

---

The following are (almost) pure virtual base classes to be subclassed by API providers:

- **Message** : in-memory representation of a message, consisting of content and metadata
- **MessageContent** : arbitrary content payload of a message
- **Metadata** : fields used by the messaging implementation (for example, message source and destination)
- **ObjectContent** : message content that is a serializable C++ object 
- **DataOutStream** : data serialization stream
- **DataInStream** : data deserialization stream

*ObjectContent* is generally subclassed by Arras users to define new custom message content types. The remaining classes are subclassed by Arras itself (in arras4_core_impl) to provide a concrete implementation of the messaging system.

---

- **ContentMacros** : helpers for implementing *ObjectContent*
- **ContentRegistry** : mapping of message type to corresponding *ObjectContent* class
- **MessageFormatError** 

are concrete implementations that form part of the API.



