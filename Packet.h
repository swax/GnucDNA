#pragma once

#pragma pack (push, 1)

union IP;
struct Node;


// Definitions
union IP										// Size 4 	
{
	struct { u_char s_b1,s_b2,s_b3,s_b4; };		// S_un_b
	struct { u_short s_w1,s_w2; };				// S_un_w
	struct {BYTE a, b, c, d;};					// old_IP
	u_long S_addr;

	inline int operator > (union IP& first)
	{	
		bool result = false;

		if(a > first.a)
			result = true;
		else if(a == first.a)
		{
			if(b > first.b)
				result = true;
			else if(b == first.b)
			{
				if(c > first.c)
					result = true;
				else if(c == first.c)
				{
					if(d > first.d)
						result = true;
				}
			}
		}
	
		return result;
	};
};

struct Node
{
	int     Network;
	CString Host;
	UINT    Port;
	CTime   LastSeen;

	// Create a node based on a standard "Host:port" string
	Node();
	Node(CString HostPort);
	Node(CString nHost, UINT nPort, int nNetwork=NETWORK_GNUTELLA, CTime tLastSeen=0);

	void Clear() { Host = ""; Port = 0; };

	// Allow Node = "host:port" assignment
	Node& operator=(CString &rhs);
	CString GetString();
	bool operator == (Node& first)
	{
		if (Host == first.Host && Port == first.Port)
			return true;
		return false;
	};
};


#pragma pack (pop)


