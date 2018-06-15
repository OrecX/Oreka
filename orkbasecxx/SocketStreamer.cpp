/*
 * Oreka -- A media capture and retrieval platform
 *
 * Copyright (C) 2005, orecx LLC
 *
 * http://www.orecx.com
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License.
 * Please refer to http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "SocketStreamer.h"

static LoggerPtr getLog() {
	static LoggerPtr s_log = Logger::getLogger("socketstreamer");
	return s_log;
}

class SocketStreamerConnection {

	public:
		CStdString m_target;
		TcpAddress m_addr;
		ACE_SOCK_Stream m_peer;
		char m_buf[1024];
		size_t m_bytesRead;

		static bool DoesAcceptProtocol(CStdString protocol) {
			return false; // only let derived classes accept protocols
		}

		SocketStreamerConnection(struct in_addr & hostAddr, int port) {
			m_addr.port = port;
			memcpy(&m_addr.ip, &hostAddr, sizeof(struct in_addr));
		}

		bool Connect()
		{
			char szIp[16];
			ACE_INET_Addr server;
			ACE_SOCK_Connector connector;

			memset(m_buf, 0, sizeof(m_buf));

			memset(&szIp, 0, sizeof(szIp));
			ACE_OS::inet_ntop(AF_INET, (void*)&m_addr.ip, szIp, sizeof(szIp));

			server.set(m_addr.port, szIp);
			if(connector.connect(m_peer, server) == -1) {
				return false;
			}
			return Init();
		}

		void Close() {
			m_peer.close();
		}

		size_t Recv() {
			size_t bytesRead = m_peer.recv(m_buf, sizeof(m_buf));

			if (bytesRead>0) {
				ProcessData();
			}
			return bytesRead;
		}

	protected:
		virtual void ProcessData() {
		}
		virtual bool Init() {
			return true; // default no handshake
		}
};

class Mitel3000Connection : public SocketStreamerConnection {
	public:
		Mitel3000Connection(struct in_addr & hostAddr, int port) :  SocketStreamerConnection(hostAddr, port) {
		}

		static bool DoesAcceptProtocol(CStdString protocol) {
			return 0 == strcmp(protocol, "mitel3000");
		}
};

class Mitel5000Connection : public SocketStreamerConnection {

	public:
		Mitel5000Connection(struct in_addr & hostAddr, int port) :  SocketStreamerConnection(hostAddr, port) {
		}

		static bool DoesAcceptProtocol(CStdString protocol) {
			return 0 == strcmp(protocol, "mitel5000");
		}

	protected:
		virtual bool  Init() {
			char *buf;
			const unsigned char s[] = {0x03,0x00,0x00,0x00,0x84,0x00,0x00}; 	
			int len;
			m_peer.send_n(s, 7);	 
			return true;
		}

		virtual void ProcessData() {
			CStdString logMsg;

			CStdString s("");

			for (int i=0;i<m_bytesRead;i++) {
				if (isprint(m_buf[i])) {
					s += m_buf[i];
				}
			}
			FLOG_INFO(getLog(),"DATA : %s",s);
		}
};

SocketStreamerConnection* CreateSocketStreamer(CStdString target) {

	CStdString tcpAddress=target;
	CStdString logMsg;
	CStdString host;
	int port = 0;
	struct in_addr hostAddr;

	CStdString protocol = "mitel3000"; // use this if nothing is specified
	size_t pos = tcpAddress.find("://");
	if (pos != std::string::npos) {
		protocol = tcpAddress.substr(0,pos);
		tcpAddress = tcpAddress.substr(pos+sizeof("://")-1);
	}
	protocol.ToLower();

	memset(&hostAddr, 0, sizeof(hostAddr));
	host = GetHostFromAddressPair(tcpAddress);
	port = GetPortFromAddressPair(tcpAddress);

	if (!host.size() || port == 0) {
		FLOG_ERROR(getLog(),"Invalid host:%s or port:%d -- check SocketStreamerTargets in config.xml", host, port);
		return NULL;
	}

	if (!ACE_OS::inet_aton((PCSTR)host, &hostAddr)) {
		FLOG_ERROR(getLog(),"Invalid host:%s -- check SocketStreamerTargets in config.xml", host);
		return NULL;
	}

	SocketStreamerConnection * ssc = NULL;

	if (!ssc && Mitel3000Connection::DoesAcceptProtocol(protocol)) {
		ssc = new Mitel3000Connection(hostAddr,port);
	}

	if (!ssc && Mitel5000Connection::DoesAcceptProtocol(protocol)) {
		ssc = new Mitel5000Connection(hostAddr,port);
	}

	if (ssc) {
		ssc->m_target = target;
		return ssc;
	}

	FLOG_ERROR(getLog(),"Unsupported protocol: %s -- check SocketStreamerTargets in config.xml", protocol);
	return NULL;
}

void SocketStreamer::Initialize()
{
	std::list<CStdString>::iterator it;
	CStdString logMsg;

	for (it = CONFIG.m_socketStreamerTargets.begin(); it != CONFIG.m_socketStreamerTargets.end(); it++)
	{
		CStdString target=*it;

		SocketStreamerConnection * ssc = CreateSocketStreamer(target);
		if (ssc && !ACE_Thread_Manager::instance()->spawn(ACE_THR_FUNC(ThreadHandler), (void*)ssc))
		{
			delete ssc;
			FLOG_WARN(getLog(),"Failed to start thread on %s", target);
		}
	}
}

#define NANOSLEEP(sec,nsec) { struct timespec ts; ts.tv_sec = sec; ts.tv_nsec = nsec; ACE_OS::nanosleep(&ts, NULL);}
/* #define RUN_EVERY(interval,lastRun,code) if(time(NULL)-lastRun>interval){code;lastRun=time(NULL);}; */

void SocketStreamer::ThreadHandler(void *args)
{
	SetThreadName("orka:sockstream");

	SocketStreamerConnection * ssc = (SocketStreamerConnection *) args; 
	CStdString logMsg;
	CStdString ipPort = ssc->m_target;
	bool connected = false;
	time_t lastLogTime = 0;
	int bytesRead = 0;
	unsigned long int bytesSoFar = 0;

	while(1) {
		if (!connected) {
			lastLogTime = 0;
			while (!ssc->Connect()) {
				if (time(NULL) - lastLogTime > 60 ) {
					FLOG_WARN(getLog(), "Couldn't connect to: %s error: %s", ipPort, CStdString(strerror(errno)));
					lastLogTime = time(NULL);
				}
				NANOSLEEP(2,0);
			}
			connected=true;
			lastLogTime = 0;
		}

		bytesRead = ssc->Recv();
		if(bytesRead <= 0)
		{
			CStdString errorString(bytesRead==0?"Remote host closed connection":strerror(errno));
			FLOG_WARN(getLog(), "Connection to: %s closed. error :%s ", ipPort, errorString);
			ssc->Close();
			connected = false;
			continue;
		}

		bytesSoFar += bytesRead;
		if (time(NULL) - lastLogTime > 60 ) {
			FLOG_INFO(getLog(),"Read %d from %s so far", FormatDataSize(bytesSoFar), ipPort);
			lastLogTime = time(NULL);
		}
		NANOSLEEP(0,1);
	}
}

