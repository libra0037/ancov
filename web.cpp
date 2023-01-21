#include "pch.h"
#include "usrdb.h"
#include "web.h"
#define COOKIE_LENGTH 26

using string = std::string;
const char *status_str[5] = {"Waiting", "Retrying", "<font color=lawngreen>Done</font>", "<font color=red>Abandoned</font>", "Skipped"};

#ifdef _WIN32
tm* gmtime_r(const time_t *timer, tm *buf)
{
	gmtime_s(buf, timer);
	return buf;
}
#endif

void join_web(httplib::Response &res, const char *id, const char *msg)
{
	static const string fmt([]
	{
		std::ifstream ifs("join.html.fmt");
		std::istreambuf_iterator<char> begin(ifs), end;
		return string(begin, end);
	}());
	auto u_res = cli_ua.Get("/login?service=https%3A%2F%2Fwfw.scu.edu.cn%2Fa_scu%2Fapi%2Fsso%2Fcas-index%3Fredirect%3Dhttps%253A%252F%252Fwfw.scu.edu.cn%252Fncov%252Fwap%252Fdefault%252Findex");
	if(!u_res)return res.set_content("error 10: " + to_string(u_res.error()), "text/plain");
	if(u_res->status != 200)return res.set_content("error 11", "text/plain");
	string line, exe, cid, *p = nullptr;
	std::istringstream iss(u_res->body);
	while(!iss.eof())
	{
		size_t k;
		getline(iss, line);
		if((k = line.find("\"execution\" value=\"")) != string::npos) { k += 19; p = &exe; }
		else if((k = line.find("    id: '")) != string::npos) { k += 9; p = &cid; }
		if(k != string::npos)while(line[k] != '\'' && line[k] != '"')*p += line[k++];
	}
	if(exe.empty() || cid.empty())return res.set_content("error 12", "text/plain");
	char *buf = new char[fmt.length() + exe.length() + 256];
	sprintf(buf, fmt.c_str(), exe.c_str(), id, cid.c_str(), msg);
	res.set_header("Set-Cookie", u_res->get_header_value("Set-Cookie"));
	res.set_content(buf, "text/html");
	delete buf;
}

void welcome_to_join(const string &loc, int64_t id, httplib::Response &res)
{
	auto w_res = cli_wfw.Get(loc);
	if(!w_res)return res.set_content("error 20: " + to_string(w_res.error()), "text/plain");
	if(w_res->status != 302 || w_res->get_header_value("Location") != "https://wfw.scu.edu.cn/ncov/wap/default/index")return res.set_content("error 21", "text/plain");
	string cookie = w_res->get_header_value("Set-Cookie");
	size_t k = cookie.find("eai-sess=");
	if(k == string::npos)return res.set_content("error 22", "text/plain");
	if(!usrdb.exist(id, cookie))
	{
		cookie = cookie.substr(k + 9, COOKIE_LENGTH);
		USER u = {0};
		strcpy(u.cookie, cookie.c_str());
		u.id = id;
		if(!usrdb.insert(u))return res.set_content("no more space", "text/plain");
	}
	tm t;
	time_t e = time(0) + 8640000;
	char expire[32];
	strftime(expire, 32, "%a, %d %b %Y %H:%M:%S GMT", gmtime_r(&e, &t));
	res.set_header("Set-Cookie", "chocolate=" + cookie + "; expires=" + expire + "; Max-Age=8640000; path=/clockin; secure; HttpOnly");
	res.set_redirect("/clockin");
}

void join_token(const httplib::Request &req, httplib::Response &res)
{
	string id = req.get_param_value("i");
	if(id.length() == 13) // avoid phone number
	{
		httplib::Headers head{{"Cookie", req.get_header_value("Cookie")}};
		auto p_res = cli_ua.Post("/token", head, "{\"username\":\"" + id + "\"}", "application/json; charset=UTF-8");
		if(p_res->status == 200 && p_res->body.find("success") != string::npos)
			return void(res.status = 204);
	}
	join_web(res, id.substr(0, 13).c_str(), "&#x53D1;&#x9001;&#x5931;&#x8D25;");
}

void join_login(const httplib::Request &req, httplib::Response &res)
{
	string id = req.get_param_value("i");
	if(id.length() == 13) // avoid phone number
	{
		httplib::Headers head{{"Cookie", req.get_header_value("Cookie")}};
		string exe = req.get_param_value("e");
		int char_eq = 0;
		while(exe.back() == '=')exe.pop_back(), char_eq++;
		while(char_eq--)exe += "%3D";
		string form = "username=" + id + "&"
			"password=" + req.get_param_value("s") + "&"
			"captcha=" + req.get_param_value("c") + "&"
			"submit=%E7%99%BB%E5%BD%95&"
			"type=username_smstoken&"
			"execution=" + exe + "&"
			"_eventId=submit";
		auto p_res = cli_ua.Post("/login", head, form, "application/x-www-form-urlencoded");
		if(p_res->status == 302)
		{
			string p_loc = p_res->get_header_value("Location");
			if(p_loc.substr(0, 22) == "https://wfw.scu.edu.cn")
				return welcome_to_join(p_loc.substr(22), std::stoll(id), res);
		}
	}
	join_web(res, id.substr(0, 13).c_str(), "&#x9A8C;&#x8BC1;&#x5931;&#x8D25;");
}

void opt_web(httplib::Response &res, const USER &u)
{
	static const string fmt([]
	{
		std::ifstream ifs("opt.html.fmt");
		std::istreambuf_iterator<char> begin(ifs), end;
		return string(begin, end);
	}());
	tm t;
	time_t cn_t = time(0) + 3600 * 8;
	time_t today = cn_t - cn_t % 86400;
	Status st = u.status(today);
	int last = u.result & 65535;
	const char *ls;
	if(last == 0)ls = "Succeeded";
	else if(last == 32)ls = "Checked";
	else ls = "<font color=red>Failed</font>";
	char las[32], nxt[32], *buf = new char[fmt.length() + 256];
	strftime(las, 32, "%Y-%m-%d %H:%M:%S", gmtime_r(&u.lasclk, &t));
	strftime(nxt, 32, "%Y-%m-%d %H:%M:%S", gmtime_r(&u.nxtclk, &t));
	sprintf(buf, fmt.c_str(), u.nxtclk - 3600 * 8, u.id, status_str[st], ls, las, nxt, u.skip ? "Undo" : "Skip");
	res.set_content(buf, "text/html");
	delete buf;
}

void overview_web(httplib::Response &res)
{
	static const string fmt("<!DOCTYPE html>\n<html>\n<head>\n<style>\ntable,th,td{\n\ttext-align:center;\n\tborder:solid thin black;\n}\n</style>\n<meta charset=\"UTF-8\">\n<title>Overview</title>\n</head>\n"
							"<body>\n%02d:%02d:%02d | Waiting: %d, Retrying: %d, <font color=lawngreen>Done</font>: %d, <font color=red>Abandoned</font>: %d, Skipped: %d<br>\n"
							"<table>\n<tr><th>ID</th><th>Status</th><th>Last State</th><th>Failures</th><th>Next Clock-in</th></tr>\n%s</table>\n</body>\n</html>");
	time_t cn_t = time(0) + 3600 * 8;
	time_t today = cn_t - cn_t % 86400;
	std::ostringstream oss;
	int count[5] = {0};
	usrdb.iterate_all([&](USER &u)
	{
		int status = u.status(today);
		count[status]++;
		int last = u.result & 65535, failures = u.result >> 16;
		oss << "<tr><td>" << u.id << "</td><td>" << status_str[status] << "</td><td>" << last << "</td><td>" << failures << "</td><td>" << u.nxtclk - cn_t << "s</td></tr>\n";
	});
	tm t;
	gmtime_r(&cn_t, &t);
	string table = oss.str();
	char *buf = new char[fmt.length() + table.length() + 256];
	sprintf(buf, fmt.c_str(), t.tm_hour, t.tm_min, t.tm_sec, count[0], count[1], count[2], count[3], count[4], table.c_str());
	res.set_content(buf, "text/html");
	delete buf;
}

void web_thread_func(httplib::SSLServer &svr)
{
	svr.Get("/", [](const httplib::Request &, httplib::Response &res)
	{
		res.set_content("Hello World", "text/plain");
	});
	svr.Get("/clockin", [](const httplib::Request &req, httplib::Response &res)
	{
		string cookie = req.get_header_value("Cookie");
		size_t k = cookie.find("chocolate=");
		if(k != string::npos)
		{
			USER u = usrdb.select(cookie.substr(k + 10, COOKIE_LENGTH));
			if(u.id)return opt_web(res, u);
		}
		res.set_redirect("/clockin/join");
	});
	svr.Get("/clockin/join", [](const httplib::Request &, httplib::Response &res)
	{
		join_web(res, "", "");
	});
	svr.Get("/clockin/captcha", [](const httplib::Request &req, httplib::Response &res)
	{
		httplib::Headers head{{"Cookie", req.get_header_value("Cookie")}};
		auto c_res = cli_ua.Get(req.target.substr(8), head);
		if(c_res->status == 200)res.set_content(c_res->body, "image/png");
		else res.status = 400;
	});
	svr.Get("/clockin/overview", [](const httplib::Request &req, httplib::Response &res)
	{
		string cookie = req.get_header_value("Cookie");
		size_t k = cookie.find("chocolate=");
		if(k != string::npos)
		{
			USER u = usrdb.select(cookie.substr(k + 10, COOKIE_LENGTH));
			if(u.id == ADMINISTRATOR)return overview_web(res);
		}
		res.status = 404;
	});
	svr.Post("/clockin", [](const httplib::Request &req, httplib::Response &res)
	{
		string cookie = req.get_header_value("Cookie");
		size_t k = cookie.find("chocolate=");
		if(k != string::npos)
		{
			cookie = cookie.substr(k + 10, COOKIE_LENGTH);
			string method = req.get_param_value("m");
			if((method == "0" && usrdb.skip_or_undo(cookie)) ||
				(method == "1" && usrdb.remove(cookie)))
				return res.set_redirect("/clockin");
		}
		res.status = 403;
	});
	svr.Post("/clockin/join", [](const httplib::Request &req, httplib::Response &res)
	{
		if(req.get_param_value("e") == "--token")return join_token(req, res);
		return join_login(req, res);
	});
	svr.listen("0.0.0.0", 443);
}

WebServer::WebServer(): svr(SVR_CERT_PATH, SVR_KEY_PATH)
{
	httplib::Headers def{
		{"Cache-Control", "no-store"},
		{"Strict-Transport-Security", "max-age=31536000; includeSubDomains; preload"},
		{"X-Content-Type-Options", "nosniff"},
		{"X-Frame-Options", "SAMEORIGIN"},
	};
	svr.set_default_headers(def);
}

void WebServer::start()
{
	thread = std::thread(web_thread_func, std::ref(svr));
}

void WebServer::close()
{
	svr.stop();
	thread.join();
}

httplib::SSLClient cli_ua("ua.scu.edu.cn");
httplib::SSLClient cli_wfw("wfw.scu.edu.cn");
WebServer web;