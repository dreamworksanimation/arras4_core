// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <boost/program_options.hpp>

#include <message_api/Object.h>
#include <message_impl/StreamImpl.h>
#include <message_impl/Envelope.h>
#include <message_impl/MetadataImpl.h>

#include <network/FileDataSource.h>
#include <network/BasicFramingSource.h>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <algorithm>

namespace bpo = boost::program_options;

using namespace arras4::api;
using namespace arras4::impl;
using namespace arras4::network;

const int EXIT_ERROR = -1;
const std::string MSG_EXT(".msg");

void parseCmdLine(int argc, char* argv[],
                  bpo::options_description& flags, 
                  bpo::variables_map& cmdOpts)
{
   flags.add_options()
       ("path", bpo::value<std::string>()->default_value("."), "Path to directory or message file")
       ("full","Show full info")
       ;

    bpo::positional_options_description positionals;
    positionals.add("path", 1);
    
    bpo::store(bpo::command_line_parser(argc, argv).
               options(flags).
               positional(positionals).run(), cmdOpts);
    bpo::notify(cmdOpts);
}

void readMessageHeader(const std::string& filepath,
                       ObjectRef obj)
{
    struct stat64 stat_buf;
    int rc = stat64(filepath.c_str(), &stat_buf);
    if (rc != 0) {
        std::cerr << "failed to stat file " << filepath << std::endl;
        return;
    }
    Json::Value size { static_cast<Json::Int64>(stat_buf.st_size) }; // Json::Int64 = long long

    FileDataSource fds(filepath); 
 
    InStreamImpl inStream(fds);
    ClassID classId;
    unsigned version;
    Envelope env;
    env.deserialize(inStream,classId,version);
    if (env.metadata())
        env.metadata()->toObject(obj);

    // add additional non-metadata info 
    obj["_classId"] = classId.toString();
    obj["_version"] = version;
    obj["_serialSize"] = size;
}

void showInfo(const std::string& filepath,bool full, const std::string& prefix="")
{
    Object obj;
    readMessageHeader(filepath,obj);
    if (full) {
        std::cout << prefix << " " << objectToStyledString(obj) << std::endl;
    } else {
        std::cout << prefix << " " << obj["instanceId"].asString() << " " << obj["routingName"].asString() << " "
                  << obj["_serialSize"].asInt64() << " bytes" << std::endl;
    }
}
      
void showDir(const std::string& dir,bool full)
{
    DIR *dp = opendir(dir.c_str());
    if (dp == nullptr) {
        std::cerr << "failed to read directory " << dir << std::endl;
        return;
    }
  
    struct dirent *dirp;
    std::vector<std::string> filenames;
    while ((dirp = readdir(dp)) != nullptr) {
        std::string fname(dirp->d_name);
        if ((fname.size() > 4) &&
            std::equal(MSG_EXT.rbegin(), MSG_EXT.rend(),fname.rbegin())) {
            filenames.push_back(fname);
        }
    }
    closedir(dp);
    if (filenames.empty()) {
        std::cout << "No message files found in directory " << dir << std::endl;
    } else {
        std::sort(filenames.begin(),filenames.end());
        for (const std::string& fn : filenames) {
            std::string fullpath = dir + "/" + fn;
            showInfo(fullpath,full,fn);
        }
    }
}

void show(const std::string& path, bool full)
{
    struct stat64 stat_buf;
    int rc = stat64(path.c_str(), &stat_buf);
    if (rc != 0) {
        std::cerr << "failed to stat path " << path << std::endl;
        return;
    }

    if (S_ISDIR(stat_buf.st_mode)) {
        showDir(path,full);
    } else {
        showInfo(path,full);
    }
}

int
main(int argc, char* argv[])
{
    // parse the command line arguments
    bpo::options_description flags;
    bpo::variables_map cmdOpts;
    try {
        parseCmdLine(argc, argv, flags, cmdOpts);
    } catch (std::exception& e) {
        std::cerr << "error parsing command line : " << e.what() << std::endl;
        return EXIT_ERROR; 
    } catch(...) {
        std::cerr << "error parsing command line : (unknown exception)" 
                  << std::endl;
        return EXIT_ERROR;
    }

    bool full = (cmdOpts.count("full") > 0);
    show(cmdOpts["path"].as<std::string>(),full);
    return 0;
}
   
    
