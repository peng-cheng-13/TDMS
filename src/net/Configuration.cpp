/***********************************************************************
* 
* 
* Tsinghua Univ, 2016
*
***********************************************************************/

#include "Configuration.hpp"
#include "debug.hpp"

using namespace std;

Configuration::Configuration() {
	ServerCount = 0;
	read_xml("/BIGDATA/nsccgz_pcheng_1/src/octopus/conf.xml", pt);
	ptree child = pt.get_child("address");
	for(BOOST_AUTO(pos,child.begin()); pos != child.end(); ++pos) 
    {  
        id2ip[(uint16_t)(pos->second.get<int>("id"))] = pos->second.get<string>("ip");
        ip2id[pos->second.get<string>("ip")] = pos->second.get<int>("id");
        ServerCount += 1;
	printf("Debug-Configuration.cpp: read conf and add ip once, ip is %s, id is %d\n", pos->second.get<string>("ip").c_str(), pos->second.get<int>("id"));
    }
}

Configuration::~Configuration() {
	Debug::notifyInfo("Configuration is closed successfully.");
}

string Configuration::getIPbyID(uint16_t id) {
	return id2ip[id];
}

uint16_t Configuration::getIDbyIP(string ip) {
	return ip2id[ip];
}

unordered_map<uint16_t, string> Configuration::getInstance() {
	return id2ip;
}

int Configuration::getServerCount() {
	return ServerCount;
}
