#pragma once

class CDnaCore;
class CGnuNetworks;
class CGnuCore;

class GNUC_API CDnaNetwork  
{
public:
	CDnaNetwork();
	  ~CDnaNetwork();


	void InitClass(CDnaCore* dnaCore);

	CDnaCore*     m_dnaCore;
	CGnuNetworks* m_gnuNetwork;
	CGnuCore*     m_gnuCore;

	std::vector<int> GetNodeIDs(void);
	LONG GetNodeState(LONG NodeID);
	LONG ClientMode(void);
	LONG GetNodeMode(LONG NodeID);
	ULONG GetNodeIP(LONG NodeID);
	LONG GetNodePort(LONG NodeID);
	
	LONG GetNodeBytesUp(LONG NodeID);
	LONG GetNodeBytesDown(LONG NodeID);

	void ConnectNode(LPCTSTR Host, LONG Port);
	void RemoveNode(LONG NodeID);
	LONG GetNormalConnectedCount(void);
	CString GetNodeHandshake(LONG NodeID);
	DATE GetNodeConnectTime(LONG NodeID);
	ULONG GetLocalIP(void);
	LONG GetLocalPort(void);
	std::vector<int> GetLanNodeIDs(void);
	void LanModeOn(void);
	void LanModeOff(void);
	CString GetLanNodeName(LONG LanNodeID);
	LONG GetLanNodeLeaves(LONG LanNodeID);
	void GetNodePacketsPing(LONG NodeID, LONG* Good, LONG* Total);
	void GetNodePacketsPong(LONG NodeID, LONG* Good, LONG* Total);
	void GetNodePacketsQuery(LONG NodeID, LONG* Good, LONG* Total);
	void GetNodePacketsQueryHit(LONG NodeID, LONG* Good, LONG* Total);
	void GetNodePacketsPush(LONG NodeID, LONG* Good, LONG* Total);
	void GetNodePacketsTotal(LONG NodeID, LONG* Good, LONG* Total);
	LONG GetNodeBytesDropped(LONG NodeID);
	DOUBLE GetNodePacketsDown(LONG NodeID);
	DOUBLE GetNodePacketsUp(LONG NodeID);
	DOUBLE GetNodePacketsDropped(LONG NodeID);
	LONG GetLocalSpeed(void);
	void ForceUltrapeer(BOOL Enabled);
	void JoinLan(LPCTSTR LanName);
	std::vector<int> GetChildNodeIDs(void);
	void SendChallenge(LONG NodeID, LPCTSTR Challenge, LPCTSTR Answer);
	void AnswerChallenge(LONG NodeID, LPCTSTR Answer);
	CString GetNodeAgent(LONG NodeID);
	LONG GetChildConnectedCount(void);
	CString GetNodeStatus(LONG NodeID);
	LONG ClientMode2(LONG NetworkID);
	void ConnectNode2(LPCTSTR Host, LONG Port, LONG NetworkID);
	LONG GetNormalConnectedCount2(LONG NetworkID);
	void ForceUltrapeer2(BOOL Enabled, LONG NetworkID);
	LONG GetChildConnectedCount2(LONG NetworkID);
	void SetClientMode(LONG Mode);
};


