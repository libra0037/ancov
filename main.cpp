#include "pch.h"
#include "usrdb.h"
#include "schedule.h"
#include "web.h"

int main()
{
#ifndef _WIN32
	cli_ua.set_ca_cert_path("", CA_CERT_DIR_PATH);
	cli_wfw.set_ca_cert_path("", CA_CERT_DIR_PATH);
#endif
	//cli_ua.set_keep_alive(true);
	//cli_wfw.set_keep_alive(true);
	usrdb.open("usrdb.dat");
	schedule.start();
	web.start();
	while(1)
	{
		std::string line;
		std::cout << "clockin-server> ";
		getline(std::cin, line);
		if(line == "list failed")
		{
			time_t cn_t = time(0) + 3600 * 8;
			time_t today = cn_t - cn_t % 86400;
			int cnt = 0;
			usrdb.iterate_all([&](USER &u)
			{
				Status st = u.status(today);
				if(st == Retrying || st == Abandoned)
				{
					std::cout << u.id << "\t" << u.cookie << std::endl;
					cnt++;
				}
			});
			std::cout << "cnt = " << cnt << std::endl;
		}
		if(line == "clockin admin")
		{
			std::string cookie;
			if(usrdb.exist(ADMINISTRATOR, cookie))
			{
				USER u = usrdb.select(cookie);
				Schedule::clockin(u);
				usrdb.update(u);
				std::cout << "state = " << (u.result & 65535) << std::endl;
			}
		}
		if(line == "clockin failed")
		{
			time_t cn_t = time(0) + 3600 * 8;
			time_t today = cn_t - cn_t % 86400;
			int cnt = 0;
			usrdb.iterate_all([&](USER &u)
			{
				Status st = u.status(today);
				if(st == Retrying || st == Abandoned)
				{
					Schedule::clockin(u);
					std::cout << u.id << ": " << (u.result & 65535) << std::endl;
					cnt++;
				}
			});
			std::cout << "cnt = " << cnt << std::endl;
		}
		if(line == "exit")break;
	}
	std::cout << "Close web server ..." << std::endl;
	web.close();
	std::cout << "Stop schedule ..." << std::endl;
	schedule.stop();
	std::cout << "Close user database ..." << std::endl;
	usrdb.close();
	cli_ua.stop();
	cli_wfw.stop();
	return 0;
}