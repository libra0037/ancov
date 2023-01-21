#include "pch.h"
#include "usrdb.h"
#include "schedule.h"
#include "web.h"
#include <openssl/sha.h>

using string = std::string;

int getinfo(const httplib::Headers &head, int64_t id, string &info)
{
	static const std::regex rgx1("var def = \\{([^]+?)\\};\\n[^/]+?number: '(\\d+)'[^/]+?info: \\$\\.extend\\(\\{([^]+?)\\}, def, \\{([^]+?)\\}\\),\\s+oldInfo:");
	static const std::regex rgx2("(\"(?:(?:\\\\\\\\)|(?:\\\\\")|[^\"])*\")|('(?:(?:\\\\\\\\)|(?:\\\\')|[^'])*')|(?:\\/\\/[^\\n]*)|(?:\\/\\*[^]*?\\*\\/)|\\s");
	static const std::regex rgx3("(?:\\{(?:(?:(?:\"[\\w()*.-]*\")|(?:'[\\w()*.-]*')|\\w+):(?:(?:\"[\\w()*.-]*\")|(?:'[\\w()*.-]*')|\\d+),)+,?\\}){3}");
	static const std::regex rgx4("(?:(?:\"([\\w()*.-]*)\")|(?:'([\\w()*.-]*)')|(\\w+)):(?:(?:\"([\\w()*.-]*)\")|(?:'([\\w()*.-]*)')|(\\d+))");
	static const std::regex rgx5("&([\\w()*.-]*)=[\\w()*.-]*((?:&[\\w()*.-]*=[\\w()*.-]*)*?)&\\1=([\\w()*.-]*)");
	auto res = cli_wfw.Get("/ncov/wap/default/index", head);
	if(!res)return 20;
	if(res->status != 200)return 21;
	string str = std::regex_replace(res->body, rgx1, "//$2\n{$3,}{$1,}{$4,}", std::regex_constants::format_no_copy);
	if(str.empty())return 22;
	if(std::stoll(str.substr(2, 13)) != id)return 23;
	str = std::regex_replace(str, rgx2, "$1$2");
	if(!std::regex_match(str, rgx3))return 24;
	str = std::regex_replace(str, rgx4, "&$1$2$3=$4$5$6", std::regex_constants::format_no_copy);
	for(string s; (s = std::regex_replace(str, rgx5, "&$1=$3$2")) != str; str = s);
	info = str.substr(1);
	return 0;
}

int postinfo(const httplib::Headers &head, const string &info)
{
	auto res = cli_wfw.Post("/ncov/wap/default/save", head, info, "application/x-www-form-urlencoded");
	if(!res)return 30;
	if(res->status != 200)return 31;
	string &str = res->body;
	if(str.find("\xE6\x93\x8D\xE4\xBD\x9C\xE6\x88\x90\xE5\x8A\x9F") != string::npos)return 0;
	else if(str.find("\xE4\xBB\x8A\xE5\xA4\xA9\xE5\xB7\xB2\xE7\xBB\x8F\xE5\xA1\xAB\xE6\x8A\xA5\xE4\xBA\x86") != string::npos)return 32;
	return 33;
}

int clockin(const char *cookie, int64_t id)
{
	uint64_t sha[4]; // any value is ok
	SHA256((const unsigned char*)cookie, strlen(cookie), (unsigned char*)sha);
	std::ostringstream oss;
	oss << "eai-sess=" << cookie << ";UUkey=" << std::hex << std::setfill('0') << std::setw(16) << sha[0] + sha[1] << std::setw(16) << sha[2] + sha[3];
	httplib::Headers head{{"Cookie", oss.str()}};
	string ifo;
	if(int r1 = getinfo(head, id, ifo))return r1;
	return postinfo(head, ifo);
}

void Schedule::clockin(USER &u)
{
	thread_local std::minstd_rand generator(std::random_device{}());
	std::uniform_int_distribution<int> uniform(300, 600);
	time_t cn_t = time(0) + 3600 * 8;
	time_t today = cn_t - cn_t % 86400;
	int failures = u.result >> 16;
	int state = ::clockin(u.cookie, u.id);
	u.lasclk = cn_t;
	u.nxtclk = today + 86400 + uniform(generator);
	if(state == 0 || state == 32)failures = 0;
	else if(++failures < 3)u.nxtclk = cn_t + failures * 60;
	u.result = failures << 16 | state;
	u.skip = 0;
}

void schedule_thread_func(const std::atomic_bool &running)
{
	time_t cn_t, today, lasday = 0;
	while(running.load())
	{
		cn_t = time(0) + 3600 * 8;
		today = cn_t - cn_t % 86400;
		if(today != lasday)
		{
			usrdb.iterate_all([today](USER &u)
			{
				u.result &= 65535;
				if(today > u.skip * 86400ll)u.skip = 0;
			});
			lasday = today;
		}
		USER u = usrdb.select_active(cn_t);
		if(u.id)
		{
			Schedule::clockin(u);
			usrdb.update(u);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

void Schedule::start()
{
	running.store(true);
	thread = std::thread(schedule_thread_func, std::cref(running));
}

void Schedule::stop()
{
	running.store(false);
	thread.join();
}

Schedule schedule;