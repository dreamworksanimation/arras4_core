// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <sdk/sdk.h>
#include <client/api/SessionDefinition.h>

#include <message_api/Object.h>
#include <boost/program_options.hpp>

#include <string>

namespace bpo = boost::program_options;

using namespace arras4;

void
parseCmdLine(int argc, 
		 char* argv[],
		 bpo::options_description& flags, 
		 bpo::variables_map& cmdOpts) 
{
    flags.add_options()
	("help", "produce help message")
	("file", bpo::value<std::string>())
        ("session,s", bpo::value<std::string>())
	("json",bpo::value<std::string>())
        ("rez_packages", bpo::value<std::string>())
        ("pseudo-compiler", bpo::value<std::string>())
        ("packaging_system", bpo::value<std::string>())
	("rez_packages_prepend",bpo::value<std::string>())
;
    bpo::store(bpo::command_line_parser(argc, argv).
	       options(flags).run(), cmdOpts);
    bpo::notify(cmdOpts);
}

void processObj(api::ObjectRef obj)
{
    std::string err;
    std::string ctx = sdk::SDK::resolveRez(obj,err);
    if (ctx == "ok") {
	std::cout << "No modifications needed" << std::endl;
    } else if (ctx == "error") {
	std::cout << "Processing error: " << std::endl <<
	    "    " << err << std::endl;
    } else {
	std::cout << "Result: " << std::endl <<
	    ctx << std::endl;
    }
}

void processDef(client::SessionDefinition& def)
{
    std::string err;
    bool ok = sdk::SDK::resolveRez(def,err);
    if (ok) {
	std::cout << "Result" << std::endl <<
	    api::objectToStyledString(def.getObject())
		  << std::endl;
    } else {
	std::cout << "Processing error: " << std::endl <<
	    "    " << err << std::endl;
} 
}

int
main(int argc, char* argv[])
{

    bpo::options_description flags;
    bpo::variables_map cmdOpts;

    try {
        parseCmdLine(argc, argv, flags, cmdOpts);
    } catch(std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
        return 1;
    } catch(...) {
        std::cerr << "Exception of unknown type!" << std::endl;;
        return 1;
    }

    if (cmdOpts.count("help")) {
        std::cout << flags << std::endl;
        return 0;
    }

    if (cmdOpts.count("file")) {
	std::string file = cmdOpts["file"].as<std::string>();
	std::cout << "Definition from file: " << file << std::endl;
        client::SessionDefinition def;
	def.loadFromFile(cmdOpts["file"].as<std::string>());
	processDef(def);
    }
    else if (cmdOpts.count("session")) {
        std::string name = cmdOpts["session"].as<std::string>();
	std::cout << "Session name: " << name << std::endl;
        client::SessionDefinition def = client::SessionDefinition::load(name);
	processDef(def);
    }
    else if (cmdOpts.count("json")) {
	std::string json = cmdOpts["json"].as<std::string>();
	api::Object obj;
	api::stringToObject(json,obj);
	processObj(obj);
    }
    else if (cmdOpts.count("rez_packages")) {
	api::Object obj;
	obj["rez_packages"] = cmdOpts["rez_packages"].as<std::string>();
	if (cmdOpts.count("pseudo-compiler")) 
	    obj["pseudo-compiler"] = cmdOpts["pseudo-compiler"].as<std::string>();
	if (cmdOpts.count("packaging_system"))
	    obj["packaging_system"] = cmdOpts["packaging_system"].as<std::string>();
	if (cmdOpts.count("rez_packages_prepend"))
	    obj["rez_packages_prepend"] = cmdOpts["rez_packages_prepend"].as<std::string>();
	processObj(obj);
    }
    else {
	std::cerr << "One of '--file','--session','--json' or '--rez_packages' is required" << std::endl;
	return -1;
    }
    return 0;
}
