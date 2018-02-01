#include <sstream>
#include <znc/main.h>
#include <znc/Client.h>
#include <znc/Modules.h>

class Bouncer : public CModule {
public:
	static const char *command, *version;

	MODCONSTRUCTOR(Bouncer) {};

	bool OnLoad(const CString& sArgs, CString& sMessage) override;
	void OnClientLogin(void) override;
	void OnIRCConnected(void) override;
	CModule::EModRet OnUserRaw(CString &sLine) override;

	void subcmd_connect(std::vector<CString> &replies, const CString &params);
	void subcmd_disconnect(std::vector<CString> &replies, const CString &params);
	void subcmd_listnetworks(std::vector<CString> &replies, const CString &params);
	void subcmd_listbuffers(std::vector<CString> &replies, const CString &params);
	void subcmd_addnetwork(std::vector<CString> &replies, const CString &params);
	void subcmd_changenetwork(std::vector<CString> &replies, const CString &params);
	void subcmd_delnetwork(std::vector<CString> &replies, const CString &params);

	void reply(CClient *client, const std::vector<CString> &reply);
	void sendSupportedModes(CClient *client);
};

