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

void SocketStreamer::ThreadHandler(void *args)
{
	SetThreadName("orka:sockstream");

	CStdString logMsg;
	SocketStreamer* ssc = (SocketStreamer*) args; 

	CStdString params = ssc->m_logMsg;
	FLOG_INFO(getLog(), "Succesfully created thread (%s)", params);

	CStdString ipPort = params;
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

bool SocketStreamer::Parse(CStdString target) 
{
	CStdString ip;
	ChopToken(ip,":",target);

	if (!ACE_OS::inet_aton((PCSTR)ip, &m_addr.ip)) {
		m_logMsg.Format("Invalid host:%s", ip);
		return false;
	}
	m_logMsg += CStdString(" host:") + ip;

	m_addr.port = strtol(target,NULL,0);
	if (m_addr.port == 0) {
		m_logMsg.Format("Invalid port:%s", target);
		return false;
	}
	m_logMsg += CStdString(" port:") + target;

	return true;
}

bool SocketStreamer::Connect()
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
	return Handshake();
}

void SocketStreamer::Close() {
	m_peer.close();
}

size_t SocketStreamer::Recv() {
	CStdString logMsg;
	m_bytesRead = m_peer.recv(m_buf, sizeof(m_buf));

	if (m_bytesRead>0) {
		ProcessData();
		FLOG_INFO(getLog(),"DATA : %d",m_bytesRead);
	}
	return m_bytesRead;
}

void SocketStreamer::ProcessData() {
	// default do nothing
}

bool SocketStreamer::Handshake() {
	return true; // default no handshake
}

void SocketStreamer::Initialize(SocketStreamerFactory *factory)
{
	CStdString logMsg;
	SocketStreamerFactory ssf;

	if (factory == NULL) {
		factory = &ssf;
	}

	for (std::list<CStdString>::iterator it = CONFIG.m_socketStreamerTargets.begin(); it != CONFIG.m_socketStreamerTargets.end(); it++)
	{
		CStdString target=*it;

		CStdString protocol;
		ChopToken(protocol,"://",target);
		protocol.ToLower();

		if (factory->Accepts(protocol)) {

			SocketStreamer *ss = factory->Create();
			ss->m_logMsg.Format("protocol:%s",protocol);

			if (!ss->Parse(target) || !ss->Spawn()) {
				FLOG_ERROR(getLog(),"%s - %s", *it, ss->m_logMsg);
				delete ss;
			}
		}
	}
}

bool SocketStreamer::Spawn() 
{
	if (!ACE_Thread_Manager::instance()->spawn(ACE_THR_FUNC(ThreadHandler), (void*)this)) {
		m_logMsg = "Failed to start thread";
		return false;
	}
	return true;
}

/* void SocketStreamer::Initialize(CSt) */
/* { */
/* 	CStdString logMsg; */

/* 	for (std::list<CStdString>::iterator it = CONFIG.m_socketStreamerTargets.begin(); it != CONFIG.m_socketStreamerTargets.end(); it++) */
/* 	{ */
/* 		CStdString target=*it; */

/* 		try */ 
/* 		{ */
/* 			SocketStreamerConnection * ssc = SocketStreamerConnection::Create(target); */
/* 			if (!ACE_Thread_Manager::instance()->spawn(ACE_THR_FUNC(ThreadHandler), (void*)ssc)) { */
/* 				FLOG_WARN(getLog(),"Failed to start thread on %s", target); */
/* 			} */
/* 		} */
/* 		catch(CStdString s) */ 
/* 		{ */
/* 		/1* FLOG_ERROR(getLog(),"Invalid  port:%d in target:%s -- check SocketStreamerTargets in config.xml", port); *1/ */
/* 			// log the error */
/* 		} */
/* 	} */
/* } */



/* CStdString SocketStreamerConnection::ToString() { */
/* 	return CStdString(""); */
/* } */

/* void SocketStreamerConnection::ParseTarget(CStdString target) */ 
/* { */
/* 	CStdString ip; */

/* 	ChopToken(ip,":",target); */

/* 	if (!ACE_OS::inet_aton((PCSTR)ip, &m_addr.ip)) { */
/* 		throw "Invalid host:" + ip ; */
/* 	} */

/* 	m_addr.port = strtol(target,NULL,0); */
/* 	if (m_addr.port == 0) { */
/* 		throw "Invalid  port:0"; */
/* 	} */
/* } */

/* /1* SocketStreamerConnection::SocketStreamerConnection(struct in_addr & hostAddr, int port) { *1/ */
/* /1* 	m_addr.port = port; *1/ */
/* /1* 	memcpy(&m_addr.ip, &hostAddr, sizeof(struct in_addr)); *1/ */
/* /1* } *1/ */

/* bool SocketStreamerConnection::Connect() */
/* { */
/* 	char szIp[16]; */
/* 	ACE_INET_Addr server; */
/* 	ACE_SOCK_Connector connector; */

/* 	memset(m_buf, 0, sizeof(m_buf)); */

/* 	memset(&szIp, 0, sizeof(szIp)); */
/* 	ACE_OS::inet_ntop(AF_INET, (void*)&m_addr.ip, szIp, sizeof(szIp)); */

/* 	server.set(m_addr.port, szIp); */
/* 	if(connector.connect(m_peer, server) == -1) { */
/* 		return false; */
/* 	} */
/* 	return Init(); */
/* } */

/* void SocketStreamerConnection::Close() { */
/* 	m_peer.close(); */
/* } */

/* size_t SocketStreamerConnection::Recv() { */
/* 	CStdString logMsg; */
/* 	m_bytesRead = m_peer.recv(m_buf, sizeof(m_buf)); */

/* 	if (m_bytesRead>0) { */
/* 		ProcessData(); */
/* 		FLOG_INFO(getLog(),"DATA : %d",m_bytesRead); */
/* 	} */
/* 	return m_bytesRead; */
/* } */

/* void SocketStreamerConnection::ProcessData() { */
/* } */

/* bool SocketStreamerConnection::Init() { */
/* 	return true; // default no handshake */
/* } */

/* SocketStreamerConnection* SocketStreamerConnection::Create(CStdString target) */ 
/* { */
/* 	CStdString protocol("mitel3000"); // use this if nothing is specified */
/* 	ChopToken(protocol,"://",target); */
/* 	protocol.ToLower(); */

/* 	if (s_factoryMap.find(protocol) == s_factoryMap.end()) { */
/* 		throw "Could not find protocol : " + protocol; */
/* 	} */
/* 	return s_factoryMap[protocol]->create(target); */
/* } */

/* void SocketStreamerConnection::RegisterType(CStdString typeName, SocketStreamerConnectionFactory *factory) { */
/* 	s_factoryMap[typeName] = factory; */
/* } */

/* std::map<CStdString,SocketStreamerConnectionFactory*> SocketStreamerConnection::s_factoryMap; */

/* // ------------------------ */

/* SocketStreamerConnection* CreateSocketStreamer(CStdString target) */
/* { */
/* 	CStdString logMsg; */
/* 	/1* struct in_addr hostAddr; *1/ */


/* 	/1* CStdString ip; *1/ */
/* 	/1* int port; *1/ */
/* 	/1* CStdString pass; *1/ */

/* 	/1* ParseSocketStreamerTarget(target,ip,port,protocol,pass); *1/ */

/* 	/1* if (!ACE_OS::inet_aton((PCSTR)ip, &hostAddr)) { *1/ */
/* 	/1* 	FLOG_ERROR(getLog(),"Invalid host:%s in target:%s -- check SocketStreamerTargets in config.xml", ip); *1/ */
/* 	/1* 	return NULL; *1/ */
/* 	/1* } *1/ */

/* 	/1* if (port == 0) { *1/ */
/* 	/1* 	FLOG_ERROR(getLog(),"Invalid  port:%d in target:%s -- check SocketStreamerTargets in config.xml", port); *1/ */
/* 	/1* 	return NULL; *1/ */
/* 	/1* } *1/ */

/* 	/1* SocketStreamerConnection * ssc = NULL; *1/ */

/* 	/1* if (!ssc && Mitel3000Connection::DoesAcceptProtocol(protocol)) { *1/ */
/* 	/1* 	ssc = new Mitel3000Connection(hostAddr,port); *1/ */
/* 	/1* } *1/ */

/* 	/1* if (!ssc && Mitel5000Connection::DoesAcceptProtocol(protocol)) { *1/ */
/* 	/1* 	ssc = new Mitel5000Connection(hostAddr,port,pass); *1/ */
/* 	/1* } *1/ */

/* 	/1* if (ssc) { *1/ */
/* 	/1* 	ssc->m_target = target; *1/ */
/* 	/1* 	return ssc; *1/ */
/* 	/1* } *1/ */

/* 	/1* FLOG_ERROR(getLog(),"Unsupported protocol: %s -- check SocketStreamerTargets in config.xml", protocol); *1/ */
/* 	return NULL; */
/* } */

