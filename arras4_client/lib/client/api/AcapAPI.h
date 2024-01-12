// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_ACAP_API_H__
#define __ARRAS_ACAP_API_H__

#include <message_api/Object.h>

#include <string>
#include <vector>

namespace arras4 {
    namespace client {
        class AcapRequest
        {
        public:
            /// Builds an AcapRequest with a given service, user agent, and AMS location and AMS Project URI
            AcapRequest(const std::string& aService,
                        const std::string& aUserAgent,
                        const std::string& aPamSite,
                        const std::string& aPamProject);

            ~AcapRequest();

            /// Returns the URI GET headers for a Request
            std::string uri() const;

            /// Returns the name of the requested service
            const std::string& service() const { return mService; }

            /// Returns the user agent string
            const std::string& userAgent() const { return mUserAgent; }

            /// Returns the name of the AMS site
            const std::string& pamSite() const { return mPamSite; }

            /// Returns the URI of the AMS project
            const std::string& pamProject() const { return mPamProject; }

        private:
            std::string mService;
            std::string mUserAgent;
            std::string mPamSite;
            std::string mPamProject;
        };


        /// An AcapResponse wraps the response from the ACAP service with an API for accessing the list of
        /// available sessions that the client can request to instantiate.
        /// Also handles error conditions from the service
        class AcapResponse
        {
        public:
            /// ErrorCode enum for a variety of error conditions.  Allows client to respond appropriately
            enum ErrorCode {
                NO_ERROR_ACAP = 0,
                NO_AVAILABLE_SESSIONS,
                INVALID_CREDENTIALS,
                INVALID_REQUEST,
                OTHER_ERROR
            };

            /// Session definition
            typedef std::string Session;

            /// List of sessions
            typedef std::vector<std::string> SessionList;

            // Session ID is a unique identifier that is specified per-session
            typedef std::string SessionID;

            /// Default constructor creates an AcapResponse set with an Invalid ID and general error
            AcapResponse();
            ~AcapResponse();

            /// Creates an AcapResponse with a set error condition
            AcapResponse(const std::string& error, ErrorCode errorCode=INVALID_REQUEST);

            /// Creates an AcapResponse with a given list of Sessions and an associated SessionId
            AcapResponse(const SessionList& sessions, const SessionID& sessionID);

            /// Returns the list of sessions fetched from the service
            const SessionList& sessions() const { return mSessions; }

            /// Returns the unique session identifier that will be associated with any session that the client requests
            /// to be instantiated
            const SessionID& sessionId() const {return mSessionID; }

            /// Error message from the service request (if applicable)
            const std::string error() const { return mError; }

            /// Error code from the service request (if applicable)
            ErrorCode errorCode() const { return mErrorCode; }

            /// Number of available sessions.
            size_t numSessions() const { return mSessions.size(); }

            /// Returns true if an error occurred during the ACAP request
            bool hasError() const { return mErrorCode > NO_ERROR_ACAP; }

        private:
            SessionList mSessions;
            SessionID mSessionID;
            ErrorCode mErrorCode;
            std::string mError;
        };


        /**
         * Options used when creating a session instance in Arras,
         * intended to be passed into the SDK::selectSession() method.
         * All attributes are optional.
         *
         * For example if someone was working on a sequence it would
         * make sense to define the production & sequence attributes
         * leaving the shot attribute undefined. However it does not
         * make sense to define a sequence without defining the production,
         * nor would it make sense to define shot without also defining
         * the production & sequence.
         *
         * Similarly defining an asset implies that the production and
         * assetGroup have been defined.
         *
         * Example shot usage:
         * \code{.cpp}
         * auto so = arras::client::SessionOptions().\
         *     setProduction("leap").\
         *     setSequence("sq1800").\
         *     setShot("s5").\
         *     setDepartment("light").\
         *     setTeam("team1").\
         *     setId("session-123");
         * \endcode
         */
        class SessionOptions
        {
        public:
            SessionOptions() {};
            SessionOptions(const SessionOptions& obj);
            ~SessionOptions() {};

            const std::string& getProduction() const { return mProduction; }
            const std::string& getSequence() const { return mSequence; }
            const std::string& getShot() const { return mShot; }

            const std::string& getAssetGroup() const { return mAssetGroup; }
            const std::string& getAsset() const { return mAsset; }

            const std::string& getDepartment() const { return mDepartment; }
            const std::string& getTeam() const { return mTeam; }
            const std::string& getId() const { return mId; }

            api::ObjectConstRef getMetadata() const { return mMetadata; }

            /**
             * @param aProduction Name of the production being worked on.
             */
            SessionOptions& setProduction(const std::string& aProduction);

            /**
             * @param aSequence Name of the sequence being worked on.
             */
            SessionOptions& setSequence(const std::string& aSequence);

            /**
             * @param aShot Name of the shot being worked on.
             */
            SessionOptions& setShot(const std::string& aShot);

            /**
             * @param aAssetGroup Name of the group the asset being worked
             * on belongs to.
             */
            SessionOptions& setAssetGroup(const std::string& aAssetGroup);

            /**
             * @param aAsset Name of the asset being worked on.
             */
            SessionOptions& setAsset(const std::string& aAsset);

            /**
             * @param aDepartment Name of the department the artist is in.
             */
            SessionOptions& setDepartment(const std::string& aDepartment);

            /**
             * @param team Name of the team the artist is in, for departments
             * which only have one team this should be set to the same value as
             * the department.
             */
            SessionOptions& setTeam(const std::string& aTeam);

            /**
             * @param id string, for client to pass in an id or key string.
             * Will be used as the progress message id.
             */
            SessionOptions& setId(const std::string& anId);

            /**
             * @param aMetadata Set of metadata to be associated with
             * this session expressed as an Object which is JSON serializable.
             */
            SessionOptions& setMetadata(api::ObjectConstRef aMetadata);

            /**
             * @param aMetadataJSON JSON formatted string of metadata to be associated with
             * this session.
             */
            SessionOptions& setMetadata(const std::string& aMetadataJSON);

	     /**
             * @return a title for this session, based on sequence/shot/assetgroup/asset
	     * if all 4 of these are empty, returns "[No Title]"
             */
	    std::string getTitle() const;

            /**
             * @param aObject Object to append the options to
             */
            void appendToObject(api::Object& object) const;

        private:
            void jsonStringToObj(const std::string& aJSON, api::ObjectRef obj) const;

            std::string mProduction;
            std::string mSequence;
            std::string mShot;
            std::string mAssetGroup;
            std::string mAsset;
            std::string mDepartment;
            std::string mTeam;
	    std::string mId;  // client defined key/id string

            api::Object mMetadata;

        };

    } // namespace client
} // namespace arras

#endif // __ARRAS_CLIENT_H__

