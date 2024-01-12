// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

/**
 * Utils to support running arras sessions on farm hosts
 */

#ifndef __ARRAS4_CLIENT_FARM_HOSTS_FOR_ARRAS_UTILS_H_
#define __ARRAS4_CLIENT_FARM_HOSTS_FOR_ARRAS_UTILS_H_

#include <http/HttpRequest.h>
#include <message_api/Object.h>

#include <string>

namespace arras4 {
namespace sdk {

class FarmHostsForArrasUtils
{
public:

    static const std::string STATUS_KEY;
    static const std::string NUM_KEY;

    // some jobs/hosts in the group are not running yet
    static const std::string PEND_STATUS;

    // waiting for status to become available
    static const std::string WAIT_STATUS;

    // all jobs/hosts in group are running arras node
    static const std::string RUN_STATUS; 

    /**
     * @param datacenter, input string: "gld", "las"
     *
     * @param environment, input string, "prod", "uns", "stb"
     */
    FarmHostsForArrasUtils(const std::string& datacenter,
                           const std::string& environment);

    /**
     * Query the local multi-session-request-handler for all
     * groups status detail
     *
     * Returns json object:
     * example: {"120160105": {"status": "PEND", "num": 2}}
     * or 
     *   { 
     *      "120160105": {"status": "PEND", "num": 2},  
     *      "120160170": {"status": "RUN", "num": 4},  
     *   }
     *   where "120160105" is groupId, 
     *   num: number or jobs(or hosts)
     *   status : 
     *      RUN, all jobs are running arras node 
     *      PEND, at least one job is not running arras node yet
     *      WAIT, status is not available yet
     */
    api::Object getDetail() const;

    /**
     * Post a request for farm hosts to run arras sessions.
     *
     * Returns int, farm groupId
     * 
     * @param production, input string, name of production
     *
     * @param user, input string, user id
     *
     * @param num_hosts, input int, number of farm hosts
     *
     * @param min_cores_per_hosts, input int, min cores for each host.
     *        Defaults to 32. Examples: 32, 64
     * @param max_cores_per_hosts, input string, max cores for each host.
     *        Defaults to "*". Examples: "64", "*", "32"
     * @param share, input string, name of farm share, 
     *        if empty string, using "<production>_hi"
     * @param steering, input string, defaults to "unspecified"
     *        example: "ir_hosts", "iv_hosts", "ih_hosts", "ik_hosts"
     * @param priority, input int, priority of the request,  defaults to 1.
     * @param minutes, input int, runlimit in minutes, defaults to 120.
     * @param mem, input string, requested memory(in GB)
     *        examples: "unspecified", "64", "2", "100"
     *        Defaults to "unspecified" means 2G x min_cores
     * @param submission_label, input string, appends string to job submission label.
     *        Must not contain the single quote(')
     *        Defaults to ""
     */    
    int postRequest(const std::string& production,
                    const std::string& user,
                    int num_hosts,
                    int min_cores_per_hosts = 32,
                    const std::string& max_cores_per_hosts = "*",
                    const std::string& share = "",
                    const std::string& steering = "unspecified",
                    int priority = 1,
                    int minutes = 120,  // for runlimit in minutes
                    const std::string& mem = "unspecified",
                    const std::string& submission_label = "") const;

    /**
     * Cancel/kill the group.
     * 
     * @param groupId, int, farm groupId
     */
    bool cancelGroup(int groupId) const;

    /**
     * Set run limit for the given group.
     *
     * @param groupId, int, farm groupId
     * @param hours, float, example: 1.5f, 3.0f
     */
    bool setRunLimit(int groupId, float hours) const;

    /**
     * Set env to 'prod', 'stb', or 'uns' for arras nodes
     *
     * @param env, input string, name of env, 
     */
    bool setEnv(const std::string& env) const;

    /**
     * Returns the local mult-session-handler base url
     */
    static std::string getBaseUrl(const std::string& datacenter);

    /**
     * Query silo url from config service.
     */
    static std::string getSiloUrl(const std::string& datacenter);

private:

    static const std::string GROUP_ID_KEY;
    static const std::string PRODUCTION_KEY;
    static const std::string USER_KEY;
    static const std::string NUM_HOSTS_KEY;
    static const std::string MIN_CORES_KEY;
    static const std::string MAX_CORES_KEY;
    static const std::string SHARE_KEY;
    static const std::string STEERING_KEY;
    static const std::string PRIORITY_KEY;
    static const std::string MINUTES_KEY;
    static const std::string MEM_KEY;
    static const std::string SUBMISSION_LABEL_KEY;

    static api::Object getObjectFromResponse(const network::HttpResponse& resp);
    static int getGroupIdFromResponse(const network::HttpResponse& resp);

    std::string mBaseUrl;

};


} // end namespace sdk
} // end namespace arras4

#endif /* __ARRAS4_CLIENT_FARM_HOSTS_FOR_ARRAS_UTILS_H_ */

