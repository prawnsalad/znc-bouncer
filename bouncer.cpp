#include <sstream>
#include <fstream>
#include <znc/znc.h>
#include <znc/main.h>
#include <znc/IRCNetwork.h>
#include <znc/Client.h>
#include <znc/User.h>
#include <znc/Server.h>
#include <znc/Chan.h>
#include <znc/Query.h>
#include <znc/Modules.h>

#include "bouncer.h"

const char *Bouncer::command = "BOUNCER";
const char *Bouncer::version = "2";

bool Bouncer::OnLoad(const CString& sArgs, CString& sMessage) {
	(void) sArgs;
	(void) sMessage;

	return true;
}

void Bouncer::OnClientLogin(void) {
	CClient *client = this->GetClient();
	if (!client) return;

	this->sendSupportedModes(client);
}

void Bouncer::OnIRCConnected(void) {
	CIRCNetwork *net = this->GetNetwork();
	if (!net) return;

	const std::vector<CClient *> clients = net->GetClients();
	for (CClient *client : clients) {
		if (!client) continue;

		std::ostringstream os;
		os << Bouncer::command
		   << " state "
		   << net->GetName()
		   << " connected";

		client->PutClient(os.str());
	}
}

CModule::EModRet Bouncer::OnUserRaw(CString &sLine) {
	// Tokenizing without checking here is safe, since
	// CString::Token returns an empty string if it can't
	// index to the requested token.
	CString cmd = sLine.Token(0);
	CString subcmd = sLine.Token(1).MakeLower();
	CString params = sLine.Token(2, true);

	std::vector<CString> reply;

	if (cmd == Bouncer::command) {
		if (subcmd == "connect")            this->subcmd_connect(reply, params);
		else if (subcmd == "disconnect")    this->subcmd_disconnect(reply, params);
		else if (subcmd == "listnetworks")  this->subcmd_listnetworks(reply, params);
		else if (subcmd == "listbuffers")   this->subcmd_listbuffers(reply, params);
		else if (subcmd == "changebuffer")  this->subcmd_changebuffer(reply, params);
		else if (subcmd == "addnetwork")    this->subcmd_addnetwork(reply, params);
		else if (subcmd == "changenetwork") this->subcmd_changenetwork(reply, params);
		else if (subcmd == "delnetwork")    this->subcmd_delnetwork(reply, params);

		if (reply.size())
		{
			this->reply(this->GetClient(), reply);
			return CModule::EModRet::HALTCORE;
		}
	}

	return CModule::EModRet::CONTINUE;
}

void Bouncer::subcmd_connect(std::vector<CString> &replies, const CString &params) {
	CString network_name = params.Token(0);
	CUser *user = this->GetUser();
	CIRCNetwork *net = 0;

	if (network_name.empty()) {
		replies.push_back("connect * ERR_INVALIDARGS");
		return;
	}

	if (user)
		net = user->FindNetwork(network_name);

	if (!net) {
		replies.push_back("connect * ERR_NETNOTFOUND");
		return;
	}

	net->SetIRCConnectEnabled(true);
	CZNC::Get().WriteConfig();

	std::ostringstream os;
	os << "state "
	   << net->GetName()
	   << " connecting";

	replies.push_back(os.str());
}

void Bouncer::subcmd_disconnect(std::vector<CString> &replies, const CString &params) {
	CString network_name = params.Token(0);
	CUser *user = this->GetUser();
	CIRCNetwork *net = 0;

	if (network_name.empty()) {
		replies.push_back("disconnect * ERR_INVALIDARGS");
		return;
	}

	if (user)
		net = user->FindNetwork(network_name);

	if (!net) {
		replies.push_back("disconnect * ERR_NETNOTFOUND");
		return;
	}

	net->SetIRCConnectEnabled(false);
	CZNC::Get().WriteConfig();

	std::ostringstream os;
	os << "state "
	   << net->GetName()
	   << " disconnected";

	replies.push_back(os.str());
}

void Bouncer::subcmd_listnetworks(std::vector<CString> &replies, const CString &params) {
	(void) params;
	CUser *user = this->GetUser();
	std::vector<CIRCNetwork*> nets;

	if (user)
		nets = user->GetNetworks();

	for (const CIRCNetwork *net : nets) {
		if (!net) continue;

		CServer *server = net->GetCurrentServer();
		if (!server) continue;

		CString state = (net->GetIRCConnectEnabled()) ? "connected" : "disconnected";

		std::ostringstream os;
		os << "listnetworks "
		   << "network=" << net->GetName()    << ";"
		   << "host="    << server->GetName() << ";"
		   << "port="    << server->GetPort() << ";"
		   << "nick="    << net->GetNick()    << ";"
		   << "user="    << net->GetIdent()   << ";"
		   << "state="   << state             << ";";

		if (server->IsSSL())
			os << ";tls=1";

		replies.push_back(os.str());
	}

	replies.push_back("listnetworks RPL_OK");
}

void Bouncer::subcmd_listbuffers(std::vector<CString> &replies, const CString &params) {
	CString network_name = params.Token(0);
	CUser *user = this->GetUser();
	CIRCNetwork *net = 0;

	if (network_name.empty()) {
		replies.push_back("listbuffers * ERR_INVALIDARGS");
		return;
	}

	if (user)
		net = user->FindNetwork(network_name);

	if (!net) {
		replies.push_back("listbuffers * ERR_NETNOTFOUND");
		return;
	}

	CString userHash = user->GetUserName().MD5();
	CString mpath = this->GetModPath() + "/";

	const std::vector<CQuery*> &queries = net->GetQueries();
	for (const CQuery *query : queries) {
		if (!query) continue;
		if (!query->GetBufferCount()) continue;

		std::ostringstream os;
		os << "listbuffers "
		   << "network=" << net->GetName() << ";"
		   << "buffer="  << query->GetName();

		CString file = mpath + query->GetName().MD5() + "-" + userHash + ".txt";
		std::ifstream timestampfile(file);

		if (timestampfile.good()) {
			char buf[256];
			timestampfile.getline(buf, sizeof(buf));

			os << ";seen=" << buf;
		}

		replies.push_back(os.str());
	}

	const std::vector<CChan*> &chans = net->GetChans();
	for (const CChan *chan : chans) {
		if (!chan) continue;
		if (!chan->GetBufferCount()) continue;

		std::ostringstream os;
		os << "listbuffers " << net->GetName() << " "
		   << "network=" << net->GetName() << ";"
		   << "buffer="  << chan->GetName();

		if (!chan->IsDisabled())
			os << ";joined=1";

		CString file = mpath + chan->GetName().MD5() + "-" + userHash + ".txt";
		std::ifstream timestampfile(file);

		if (timestampfile.good()) {
			char buf[256];
			timestampfile.getline(buf, sizeof(buf));

			os << ";seen=" << buf;
		}

		CString topic = chan->GetTopic();
		topic.Replace(" ", "\\s");
		// The python plugin did something with UTF-8 here. Not sure if needed?

		os << ";topic=" << topic;

		replies.push_back(os.str());
	}

	replies.push_back("listbuffers " + network_name + " RPL_OK");
}

void Bouncer::subcmd_changebuffer(std::vector<CString> &replies, const CString &params) {
	CUser *user = this->GetUser();

	CString network_name = params.Token(0);
	CString buffer_name = params.Token(0);
	CString options = params.Token(1, true);

	CIRCNetwork *net = 0;

	if (user)
		net = user->FindNetwork(network_name);

	if (!net) {
		replies.push_back("changebuffer " + network_name + " * ERR_NETNOTFOUND");
		return;
	}

	CQuery *query = net->FindQuery(buffer_name);
	CChan *chan = net->FindChan(buffer_name);

	if (!chan || !query) {
		replies.push_back("changebuffer " + network_name + " " + buffer_name + " ERR_NETNOTFOUND");
		return;
	}

	CString key = options.Token(0, false, "=");
	CString val = options.Token(1, true, "=");

	unsigned y, m, d, H, M, S;
	int res = sscanf(val.c_str(), "%4u-%2u-%2uT%2u:%2u:%2uZ", &y, &m, &d, &H, &M, &S);
	if (key != "seen" || res != 6) {
		replies.push_back("changebuffer " + network_name + " " + buffer_name + " ERR_INVALIDARGS");
		return;
	}

	// Write timestamp to user file
	CString file = this->GetModPath() + "/" + buffer_name.MD5() + "-" + user->GetUserName().MD5() + ".txt";
	std::fstream stampout(file, std::fstream::out | std::fstream::trunc);
	stampout << y << "-" << m << "-" << d << "T" << H << ":" << M << ":" << S << "Z";

	replies.push_back("changebuffer " + network_name + " " + buffer_name + " RPL_OK");
}

void Bouncer::subcmd_addnetwork(std::vector<CString> &replies, const CString &params) {
	(void) replies; (void) params;
	CUser *user = this->GetUser();
	if (!user) {
		replies.push_back("addnetwork * ERR_UNKNOWN :Internal ZNC Error, User Not Found");
		return;
	}

	CString network_name = "";
	CString network_host = "";
	CString network_nick = user->GetNick();
	CString network_user = user->GetIdent();
	CString network_port = "";

	unsigned short port = 0;
	bool ssl = false;

	VCString parts;
	params.Split(";", parts);
	for (CString part : parts) {
		CString key = part.Token(0, false, "=");
		CString val = part.Token(1, false, "=");

		if (key == "" || val == "") {
			replies.push_back("addnetwork * ERR_INVALIDFORMAT");
			return;
		}

		if (key == "network") network_name = val;
		if (key == "host")    network_host = val;
		if (key == "port")    network_port = val;
		if (key == "nick")    network_nick = val;
		if (key == "user")    network_user = val;
		if (key == "tls")     ssl = (val == "1");
	}

	if (network_name == "") {
		replies.push_back("addnetwork * ERR_NONETNAMEGIVEN");
		return;
	}

	if (network_port != "") {
		errno = 0;
		long port_req = strtol(network_port.c_str(), 0, 10);
		if (errno || port_req < 1 || port_req > USHRT_MAX) {
			replies.push_back("addnetwork " + network_name + " ERR_PORTINVALID");
			return;
		}

		port = (unsigned short) port_req;
	}

	CIRCNetwork *net = user->FindNetwork(network_name);
	if (net) {
		replies.push_back("addnetwork " + network_name + " ERR_NAMEINUSE");
		return;
	}

	CIRCNetwork *net_add = new CIRCNetwork(user, network_name);
	net_add->SetNick(network_nick);
	net_add->SetAltNick(network_nick + "_");
	net_add->SetIdent(network_user);
	user->AddNetwork(net_add);

	// The python version set a property called `thisown` on `net_add`, but
	// I can't find this property in the CIRCNetwork doxygen docs.
	// Attempting to set it results in a compilation error.

	if (port && network_host != "") {
		net_add->AddServer(network_host, port, "", ssl);
	}

	CZNC::Get().WriteConfig();
	replies.push_back("addnetwork " + network_name + " RPL_OK");
}

void Bouncer::subcmd_changenetwork(std::vector<CString> &replies, const CString &params) {
	CUser *user = this->GetUser();

	CString network_name = params.Token(0);
	CString options = params.Token(1, true);

	CIRCNetwork *net = 0;

	if (user)
		net = user->FindNetwork(network_name);

	if (!net) {
		replies.push_back("changenetwork " + network_name + " ERR_NETNOTFOUND");
		return;
	}

	CServer *server = net->GetCurrentServer();

	CString network_host = server->GetName();
	CString network_port = CString(server->GetPort());
	CString network_nick = "";
	CString network_user = "";

	unsigned short port = 0;
	bool ssl = server->IsSSL();

	VCString parts;
	params.Split(";", parts);
	for (CString part : parts) {
		CString key = part.Token(0, false, "=");
		CString val = part.Token(1, false, "=");

		if (key == "" || val == "") {
			replies.push_back("changenetwork " + network_name + " ERR_INVALIDFORMAT");
			return;
		}

		if (key == "host")    network_host = val;
		if (key == "port")    network_port = val;
		if (key == "nick")    network_nick = val;
		if (key == "user")    network_user = val;
		if (key == "tls")     ssl = (val == "1");
	}

	errno = 0;
	long port_req = strtol(network_port.c_str(), 0, 10);
	if (errno || port_req < 1 || port_req > USHRT_MAX) {
		replies.push_back("changenetwork " + network_name + " ERR_PORTINVALID");
		return;
	}

	port = (unsigned short) port_req;

	net->DelServers();
	net->AddServer(network_host, port, "", ssl);

	if (network_nick != "")
		net->SetNick(network_nick);

	if (network_user != "")
		net->SetIdent(network_user);

	CZNC::Get().WriteConfig();
	replies.push_back("changenetwork " + network_name + " RPL_OK");
}

void Bouncer::subcmd_delnetwork(std::vector<CString> &replies, const CString &params) {
	CUser *user = this->GetUser();

	CString network_name = params.Token(0);
	CIRCNetwork *net = 0;

	if (user)
		net = user->FindNetwork(network_name);

	if (!net) {
		replies.push_back("changenetwork " + network_name + " ERR_NETNOTFOUND");
		return;
	}

	user->DeleteNetwork(net->GetName());

	CZNC::Get().WriteConfig();
	replies.push_back("changenetwork " + network_name + " RPL_OK");
}

void Bouncer::reply(CClient *client, const std::vector<CString> &reply) {
	if (!client) return;

	for (CString message : reply) {
		std::ostringstream os;
		os << Bouncer::command
		   << " "
		   << message;

		client->PutClient(os.str());
	}
}

void Bouncer::sendSupportedModes(CClient *client) {
	std::ostringstream os;
	os << ":irc.host 005 "
	   << client->GetNick()
	   << ' '
	   << Bouncer::command
	   << '='
	   << Bouncer::version
	   << " :are supported by this server";

	client->PutClient(os.str());
}

GLOBALMODULEDEFS(Bouncer, "Provides KiwiIRC BOUNCER support")

