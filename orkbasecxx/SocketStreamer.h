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
#ifndef __SOCKETSTREAMER_H__
#define __SOCKETSTREAMER_H__ 1

#include "OrkBase.h"
#include <log4cxx/logger.h>
#include "ConfigManager.h"
#include "LogManager.h"
#include "ace/Thread_Manager.h"
#include "Utils.h"
#include "ace/INET_Addr.h"
#include "ace/SOCK_Connector.h"
#include "ace/SOCK_Stream.h"
#include "ace/OS_NS_unistd.h"

class SocketStreamerFactory;

class DLL_IMPORT_EXPORT_ORKBASE SocketStreamer {
public:
	static void Initialize(SocketStreamerFactory *factory=NULL);

protected:
	SocketStreamer() {}
	bool Connect();
	void Close();
	size_t Recv();
	virtual void ProcessData();
	virtual bool Handshake();
	virtual bool Parse(CStdString target);

	CStdString m_protocol;
	CStdString m_logMsg;

	TcpAddress m_addr;
	ACE_SOCK_Stream m_peer;
	char m_buf[1024];
	size_t m_bytesRead;
	static void ThreadHandler(void *args);

	friend class SocketStreamerFactory;

private:
	bool Spawn();
};

class SocketStreamerFactory
{
protected:
	SocketStreamerFactory() {}
    virtual SocketStreamer* Create() { return new SocketStreamer(); } 
	virtual bool Accepts(CStdString protocolName) {
		return (protocolName == "");
	}

	friend class SocketStreamer;
};

#endif
/* class SocketStreamerConnectionFactory; */

/* class SocketStreamerConnection { */
/* 	public: */
/* 		static SocketStreamerConnection* Create(CStdString target); */
/* 		void ParseTarget(CStdString target); */

/* 		CStdString m_target; */
/* 		TcpAddress m_addr; */
/* 		ACE_SOCK_Stream m_peer; */
/* 		char m_buf[1024]; */
/* 		size_t m_bytesRead; */

/* 		/1* SocketStreamerConnection(struct in_addr & hostAddr, int port); *1/ */

/* 		bool Connect(); */
/* 		void Close(); */
/* 		size_t Recv(); */

/* 		virtual CStdString ToString(); */

/* 		static void RegisterType(CStdString typeName, SocketStreamerConnectionFactory *factory); */

/* 	protected: */
/* 		virtual void ProcessData(); */
/* 		virtual bool Init(); */

/* 		static std::map<CStdString, SocketStreamerConnectionFactory*> s_factoryMap; */

/* 		SocketStreamerConnection(CStdString target); */
/* 		SocketStreamerConnection() {} */
/* 	private: */
/* }; */

/* class SocketStreamerConnectionFactory */
/* { */
/* public: */
/*     virtual SocketStreamerConnection *create(CStdString target) = 0; */
/* }; */

/* #define REGISTER_SOCKET_STREAMER_CONNECTION_TYPE(klass) \ */
/*     class klass##Factory : public SocketStreamerConnectionFactory { \ */
/*     public: \ */
/*         klass##Factory() \ */
/*         { \ */
/*             SocketStreamerConnection::RegisterType(#klass, this); \ */
/*         } \ */
/*         virtual SocketStreamerConnection *create(CStdString t) { \ */
/*             return new klass(t); \ */
/*         } \ */
/*     }; \ */
/*     static klass##Factory global_##klass##Factory; */
