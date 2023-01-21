class WebServer
{
public:
	WebServer();
	void start();
	void close();
private:
	std::thread thread;
	httplib::SSLServer svr;
};

extern httplib::SSLClient cli_ua;
extern httplib::SSLClient cli_wfw;
extern WebServer web;