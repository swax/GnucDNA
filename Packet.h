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

	inline bool operator==(const union IP& first) const
	{
		return S_addr == first.S_addr;
	}

	inline int operator < (const union IP& first) const
	{
		if (*this > first || *this == first)
			return false;
		return true;
	}

	inline int operator > (const union IP& first) const
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

struct IPv4
{
	IP Host;
	UINT Port;

	IPv4()
	{
		Host.S_addr = 0;
		Port = 0;
	};
	
	IPv4(IP host, UINT port)
	{
		Host = host;
		Port = port;
	};

	bool operator==(const IPv4 &addr) const
	{
		return Host == addr.Host && Port == addr.Port;
	}

	bool operator!=(const IPv4 &addr) const	
	{ 
		return !(*this == addr); 
	}

	bool operator<(const IPv4 &addr) const
	{
		return ((Host < addr.Host) || ((Host == addr.Host) && (Port < addr.Port)));
	}
};

struct Node
{
	int     Network;
	CString Host;
	UINT    Port;
	CTime   LastSeen;
	bool    DNA;
	bool    TriedUdp;

	// Create a node based on a standard "Host:port" string
	Node();
	Node(CString HostPort);
	Node(CString nHost, UINT nPort, int nNetwork=NETWORK_GNUTELLA, CTime tLastSeen=0, bool DNA=false);

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


