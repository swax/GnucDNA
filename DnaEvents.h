#pragma once

class CDnaCore;

class GNUC_API CDnaEvents
{
public:
	CDnaEvents(CDnaCore* dnaCore);
	~CDnaEvents(void);

	// Network
	virtual void NetworkChange(int NodeID);
	virtual void NetworkPacketIncoming(int NetworkID, bool TCP, uint32 IP, int Port, byte* packet, int size, bool Local, int ErrorCode);
	virtual void NetworkPacketOutgoing(int NetworkID, bool TCP, uint32 IP, int Port, byte* packet, int size, bool Local);
	virtual void NetworkAuthenticate(int NodeID);
	virtual void NetworkChallenge(int NodeID, LPCTSTR Challenge);

	// Search
	virtual void SearchUpdate(LONG SearchID, LONG ResultID);
	virtual void SearchResult(LONG SearchID, LONG ResultID);
	virtual void SearchRefresh(LONG SearchID);
	virtual void SearchBrowseUpdate(LONG SearchID, LONG State, LONG Progress);
	virtual void SearchProgress(LONG SearchID);
	virtual void SearchPaused(LONG SearchID);

	// Share
	virtual void ShareUpdate(LONG FileID);
	virtual void ShareReload();

	// Downloads
	virtual void DownloadUpdate(long DownloadID);
	virtual void DownloadChallenge(long DownloadID, long SourceID, CString Challenge);

	// Uploads
	virtual void UploadUpdate(long UploadID);
	virtual void UploadAuthenticate(long UploadID);

	// Chat
	virtual void ChatRecvDirectMessage(LPCTSTR Address, LPCTSTR Message);

	// Update
	virtual void UpdateFound(LPCTSTR Version);
	virtual void UpdateFailed(LPCTSTR Reason);
	virtual void UpdateVersionCurrent();
	virtual void UpdateComplete();


	CDnaCore* m_dnaCore;
};

