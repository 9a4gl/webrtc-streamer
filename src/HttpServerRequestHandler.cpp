/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** HttpServerHandler.cpp
**
** -------------------------------------------------------------------------*/

#include <iostream>

#include "HttpServerRequestHandler.h"

/* ---------------------------------------------------------------------------
**  Civet HTTP callback
** -------------------------------------------------------------------------*/
class RequestHandler : public CivetHandler
{
  public:
	bool handle(CivetServer *server, struct mg_connection *conn)
	{
		bool ret = false;
		const struct mg_request_info *req_info = mg_get_request_info(conn);

		HttpServerRequestHandler* httpServer = (HttpServerRequestHandler*)server;

		httpFunction fct = httpServer->getFunction(req_info->request_uri);
		if (fct != NULL)
		{
			Json::Value  jmessage;

			// read input
			long long tlen = req_info->content_length;
			if (tlen > 0)
			{
				std::string body;
				long long nlen = 0;
				char buf[1024];
				while (nlen < tlen) {
					long long rlen = tlen - nlen;
					if (rlen > sizeof(buf)) {
						rlen = sizeof(buf);
					}
					rlen = mg_read(conn, buf, (size_t)rlen);
					if (rlen <= 0) {
						break;
					}
					body.append(buf, rlen);

					nlen += rlen;
				}
				std::cout << "body:" << body << std::endl;

				// parse in
				Json::Reader reader;
				if (!reader.parse(body, jmessage))
				{
					LOG(WARNING) << "Received unknown message:" << body;
				}
			}

			// invoke API implementation
			Json::Value out(fct(req_info, jmessage));

			// fill out
			if (out.isNull() == false)
			{
				std::string answer(Json::StyledWriter().write(out));
				std::cout << "answer:" << answer << std::endl;

				mg_printf(conn,"HTTP/1.1 200 OK\r\n");
				mg_printf(conn,"Access-Control-Allow-Origin: *\r\n");
				mg_printf(conn,"Content-Type: application/json\r\n");
				mg_printf(conn,"Content-Length: %zd\r\n", answer.size());
				mg_printf(conn,"Connection: close\r\n");
				mg_printf(conn,"\r\n");
				mg_printf(conn,answer.c_str());

				ret = true;
			}
		}

		return ret;
	}
	bool handleGet(CivetServer *server, struct mg_connection *conn)
	{
		return handle(server, conn);
	}
	bool handlePost(CivetServer *server, struct mg_connection *conn)
	{
		return handle(server, conn);
	}
};


int log_message(const struct mg_connection *conn, const char *message) 
{
	const struct mg_context *ctx = mg_get_context(conn);
	struct tuser_data *ud = (struct tuser_data *)mg_get_user_data(ctx);

	fprintf(stderr, "%s\n", message);

	return 0;
}

static struct CivetCallbacks _callbacks;
const struct CivetCallbacks * getCivetCallbacks() 
{
	memset(&_callbacks, 0, sizeof(_callbacks));
	_callbacks.log_message = &log_message;
	return &_callbacks;
}

/* ---------------------------------------------------------------------------
**  Constructor
** -------------------------------------------------------------------------*/
HttpServerRequestHandler::HttpServerRequestHandler(PeerConnectionManager* webRtcServer, const std::vector<std::string>& options)
	: CivetServer(options, getCivetCallbacks()), m_webRtcServer(webRtcServer)
{
	// http api callbacks
	m_func["/getMediaList"]          = [this](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
		return m_webRtcServer->getMediaList();
	};
	
	m_func["/getVideoDeviceList"]    = [this](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
		return m_webRtcServer->getVideoDeviceList();
	};

	m_func["/getAudioDeviceList"]    = [this](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
		return m_webRtcServer->getAudioDeviceList();
	};

	m_func["/getIceServers"]         = [this](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
		return m_webRtcServer->getIceServers(req_info->remote_addr);
	};

	m_func["/call"]                  = [this](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
		std::string peerid;
		std::string url;
		std::string audiourl;
		std::string options;
		if (req_info->query_string) {
            CivetServer::getParam(req_info->query_string, "peerid", peerid);
            CivetServer::getParam(req_info->query_string, "url", url);
            CivetServer::getParam(req_info->query_string, "audiourl", audiourl);
            CivetServer::getParam(req_info->query_string, "options", options);
        }
		return m_webRtcServer->call(peerid, url, audiourl, options, in);
	};

	m_func["/hangup"]                = [this](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
		std::string peerid;
		if (req_info->query_string) {
            CivetServer::getParam(req_info->query_string, "peerid", peerid);
        }
		return m_webRtcServer->hangUp(peerid);
	};

	m_func["/createOffer"]           = [this](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
		std::string peerid;
		std::string url;
		std::string audiourl;
		std::string options;
		if (req_info->query_string) {
            CivetServer::getParam(req_info->query_string, "peerid", peerid);
            CivetServer::getParam(req_info->query_string, "url", url);
            CivetServer::getParam(req_info->query_string, "audiourl", audiourl);
            CivetServer::getParam(req_info->query_string, "options", options);
        }
		return m_webRtcServer->createOffer(peerid, url, audiourl, options);
	};
	m_func["/setAnswer"]             = [this](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
		std::string peerid;
		if (req_info->query_string) {
            CivetServer::getParam(req_info->query_string, "peerid", peerid);
        }
		m_webRtcServer->setAnswer(peerid, in);
		Json::Value answer(1);
		return answer;
	};

	m_func["/getIceCandidate"]       = [this](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
		std::string peerid;
		if (req_info->query_string) {
            CivetServer::getParam(req_info->query_string, "peerid", peerid);
        }
		return m_webRtcServer->getIceCandidateList(peerid);
	};

	m_func["/addIceCandidate"]       = [this](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
		std::string peerid;
		if (req_info->query_string) {
            CivetServer::getParam(req_info->query_string, "peerid", peerid);
        }
		return m_webRtcServer->addIceCandidate(peerid, in);
	};

	m_func["/getPeerConnectionList"] = [this](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
		return m_webRtcServer->getPeerConnectionList();
	};

	m_func["/getStreamList"] = [this](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
		return m_webRtcServer->getStreamList();
	};

	m_func["/help"]                  = [this](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
		Json::Value answer;
		for (auto it : m_func) {
			answer.append(it.first);
		}
		return answer;
	};

	m_func["/version"]                  = [this](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
		Json::Value answer(VERSION);
		return answer;
	};

	m_func["/log"]                      = [this](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
		std::string loglevel;
		if (req_info->query_string) {
			CivetServer::getParam(req_info->query_string, "level", loglevel);
			if (!loglevel.empty()) {
				rtc::LogMessage::LogToDebug((rtc::LoggingSeverity)atoi(loglevel.c_str()));
			}
		}
		Json::Value answer(rtc::LogMessage::GetLogToDebug());
		return answer;
	};

	// register handlers
	for (auto it : m_func) {
		this->addHandler(it.first, new RequestHandler());
	}
}

httpFunction HttpServerRequestHandler::getFunction(const std::string& uri)
{
	httpFunction fct = NULL;
	std::map<std::string,httpFunction>::iterator it = m_func.find(uri);
	if (it != m_func.end())
	{
		fct = it->second;
	}

	return fct;
}

