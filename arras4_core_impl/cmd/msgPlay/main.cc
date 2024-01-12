// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <boost/program_options.hpp>

#include <network/IPCSocketPeer.h>
#include <network/BasicFramingSink.h>
#include <network/BasicFramingSource.h>
#include <network/BufferedSource.h>
#include <message_impl/MessageReader.h>

#include <iostream>
#include <unistd.h>
#include <thread>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <fstream>

constexpr size_t READBUF_SIZE = 16*1024;
char READBUF[READBUF_SIZE];

namespace bpo = boost::program_options;

using namespace arras4::network;
using namespace arras4::impl;
using namespace arras4::api;

const int EXIT_ERROR = -1;
const std::string MSG_EXT(".msg");

ArrasTime DELAY(1,0);
const int MAX_DELAY = 30; // maximum delay in seconds

void parseCmdLine(int argc, char* argv[],
                  bpo::options_description& flags, 
                  bpo::variables_map& cmdOpts)
{
    flags.add_options()
        ("path", bpo::value<std::string>()->default_value("."), "Path to message directory")
        ("ipc", bpo::value<std::string>(), "Address of IPC socket to play into")
        ("timestamps","use filename timestamps to determine delay between messages")
        ("save", bpo::value<std::string>(),"Directory to save outgoing messages to")
        ;

    bpo::positional_options_description positionals;
    positionals.add("path", 1);
    
    bpo::store(bpo::command_line_parser(argc, argv).
               options(flags).
               positional(positionals).run(), cmdOpts);
    bpo::notify(cmdOpts);
}

Peer* createClientPeer(const std::string& addr)
{ 
    std::cout << "Connecting to " << addr << std::endl;
    IPCSocketPeer *peer = new IPCSocketPeer();
    peer->connect(addr);
    std::cout << "Connected" << std::endl;
    return peer;
}

bool playFile(BasicFramingSink& sink,
              const std::string& filepath)
{
    // get file size for framing
    struct stat64 stat_buf;
    int rc = stat64(filepath.c_str(), &stat_buf);
    if (rc != 0) {
        std::cerr << "failed to stat file " << filepath << std::endl;
        return false;
    }
    
    size_t size = stat_buf.st_size;
    sink.openFrame(size);

    std::ifstream ifs(filepath);
    if (!ifs) {
        std::cerr << "failed to open message file " << filepath << std::endl;
        return false;
    }
    while (ifs) {
        ifs.read(READBUF,READBUF_SIZE);
        if (ifs.bad()) break;
        sink.write(reinterpret_cast<unsigned char*>(READBUF),ifs.gcount());
    }
    if (ifs.bad()) {
        std::cerr << "failed to read from message file " << filepath << std::endl;
        return false;
    }
    sink.closeFrame();
    ifs.close();
    return true;
}   
    
bool getFilenames(const std::string& dir,
                  std::vector<std::string>& filenames)
{
    DIR *dp = opendir(dir.c_str());
    if (dp == nullptr) {
        std::cerr << "failed to read directory " << dir << std::endl;
        return false;
    }
  
    struct dirent *dirp;
    while ((dirp = readdir(dp)) != nullptr) {
        std::string fname(dirp->d_name);
        if ((fname.size() > 4) &&
            std::equal(MSG_EXT.rbegin(), MSG_EXT.rend(),fname.rbegin())) {
            filenames.push_back(fname);
        }
    }
    closedir(dp);
    if (filenames.size() == 0) {
        std::cerr << "no message files found in directory " << dir << std::endl;
        return false;
    }
    std::sort(filenames.begin(),filenames.end());
    return true;
}

void doDelay(const ArrasTime& t)
{
    int secs = t.seconds;
    if (secs > MAX_DELAY) 
        secs = MAX_DELAY;
    long ms = secs*1000L + (t.microseconds/1000);
    if (ms > 0) {
        std::cout << "(delay " << (((double)ms)/1000) << " seconds)" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

bool playMessages(BasicFramingSink& sink, 
                  const std::string& path,
                  bool useTimestamps)
{
    std::vector<std::string> filenames;
    bool ok = getFilenames(path,filenames);
    if (!ok)
        return false;
   
    ArrasTime curr;
    ArrasTime prev;
    ArrasTime delay;
    for (std::string filename : filenames) {
        if (useTimestamps) {
            ok = curr.fromFilename(filename);
            if (ok && (prev != ArrasTime::zero)) {
                delay = curr - prev;
            } else {
                delay = DELAY;
            }
            if (ok)
                prev = curr;
            else
                prev = prev + delay; 
        } else {
            delay = DELAY;
        }
        doDelay(delay);
        std::cout << "Playing " << filename << std::endl;
        std::string fullpath = path + "/" + filename;
        ok = playFile(sink,fullpath);
        if (!ok) {
            return false;
        }
    }
    return true;
}

void receiveMessages(MessageReader* reader)
{
    try {
        while (true) {
            Envelope env = reader->read(false);
            std::cout << "Received: " << env.describe() << std::endl;
        }
    } catch (PeerDisconnectException&) {
        std::cout << "disconnected" << std::endl;
    }
}

int main(int argc, char** argv)
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

    if (cmdOpts.count("ipc") == 0) {
        std::cerr << "must supply IPC address: --ipc <addr>" << std::endl;
    }

    bool useTimestamps = cmdOpts.count("timestamps") > 0;
    // use smart pointer for peer
    std::unique_ptr<Peer> peer(createClientPeer(cmdOpts["ipc"].as<std::string>()));
    BasicFramingSink sink(peer->sink());

    BasicFramingSource bfs(peer->source());
    BufferedSource source(bfs);
    MessageReader reader(source,"none none");
    if (cmdOpts.count("save") > 0) {
        std::string saveDir = cmdOpts["save"].as<std::string>();
        std::cout << "Saving received messages to " << saveDir << std::endl;
        reader.enableAutosave(saveDir);
    }
    std::thread incoming {receiveMessages,&reader};

    bool ok = playMessages(sink,cmdOpts["path"].as<std::string>(),useTimestamps);
    if (!ok) {
        return -1;
    }
    std::cout << "Finished playing messages" << std::endl;
    incoming.join();
    return 0; 
}

