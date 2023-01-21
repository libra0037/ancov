#include "pch.h"
#include "usrdb.h"
#ifndef _WIN32
#include <fcntl.h>
#include <sys/mman.h>
#endif

Status USER::status(time_t today)const
{
	int last = result & 65535, failures = result >> 16;
	if(nxtclk >= today + 86400)
	{
		if((skip * 86400ll) == today)return Skipped;
		else if(last == 0 || last == 32)return Done;
		return Abandoned;
	}
	return failures ? Retrying : Waiting;
}

void Users::open(const char *file)
{
#ifndef _WIN32
	int fd = ::open(file, O_RDWR);
	ulist = (USER*)mmap(0, sizeof(USER) * ARRAY_LENGTH, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	::close(fd);
#else
	hfile = CreateFileA(file, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	hmap = CreateFileMappingA(hfile, 0, PAGE_READWRITE, 0, sizeof(USER) * ARRAY_LENGTH, 0);
	ulist = (USER*)MapViewOfFile(hmap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(USER) * ARRAY_LENGTH);
#endif
	sgtree = new int[ARRAY_LENGTH];
	for(int idx = ARRAY_LENGTH - 1; idx; idx--)
	{
		if(ulist[idx].id)
		{
			sgtree[idx] = idx;
			from_cookie[ulist[idx].cookie] = idx;
			from_id[ulist[idx].id] = ulist[idx].cookie;
		}
		else
		{
			sgtree[idx] = -1;
			idle.push(idx);
		}
		if((idx << 1) < ARRAY_LENGTH)
		{
			if(sgtree[idx] == -1)sgtree[idx] = sgtree[idx << 1];
			else if(sgtree[idx << 1] != -1
					&& ulist[sgtree[idx << 1]].nxtclk < ulist[sgtree[idx]].nxtclk)
				sgtree[idx] = sgtree[idx << 1];
		}
		if((idx << 1 | 1) < ARRAY_LENGTH)
		{
			if(sgtree[idx] == -1)sgtree[idx] = sgtree[idx << 1 | 1];
			else if(sgtree[idx << 1 | 1] != -1
					&& ulist[sgtree[idx << 1 | 1]].nxtclk < ulist[sgtree[idx]].nxtclk)
				sgtree[idx] = sgtree[idx << 1 | 1];
		}
	}
	min_nxtclk.store(sgtree[1] != -1 ? ulist[sgtree[1]].nxtclk : INT64_MAX);
}

void Users::close()
{
#ifndef _WIN32
	munmap(ulist, sizeof(USER) * ARRAY_LENGTH);
#else
	UnmapViewOfFile(ulist);
	CloseHandle(hmap);
	CloseHandle(hfile);
#endif
	delete sgtree;
	idle = std::stack<int>();
	from_cookie.clear();
	from_id.clear();
}

void Users::maintain_tree(int idx)
{
	while(idx)
	{
		sgtree[idx] = ulist[idx].id ? idx : -1;
		if((idx << 1) < ARRAY_LENGTH)
		{
			if(sgtree[idx] == -1)sgtree[idx] = sgtree[idx << 1];
			else if(sgtree[idx << 1] != -1
					&& ulist[sgtree[idx << 1]].nxtclk < ulist[sgtree[idx]].nxtclk)
				sgtree[idx] = sgtree[idx << 1];
		}
		if((idx << 1 | 1) < ARRAY_LENGTH)
		{
			if(sgtree[idx] == -1)sgtree[idx] = sgtree[idx << 1 | 1];
			else if(sgtree[idx << 1 | 1] != -1
					&& ulist[sgtree[idx << 1 | 1]].nxtclk < ulist[sgtree[idx]].nxtclk)
				sgtree[idx] = sgtree[idx << 1 | 1];
		}
		idx >>= 1;
	}
	min_nxtclk.store(sgtree[1] != -1 ? ulist[sgtree[1]].nxtclk : INT64_MAX);
}

bool Users::insert(const USER &u)
{
	std::lock_guard<std::mutex> lock(mutex);
	if(idle.empty())return false;
	int idx = idle.top(); idle.pop();
	ulist[idx] = u;
	maintain_tree(idx);
	from_cookie[u.cookie] = idx;
	from_id[u.id] = u.cookie;
	return true;
}

bool Users::remove(const std::string &cookie)
{
	std::lock_guard<std::mutex> lock(mutex);
	if(!from_cookie.count(cookie))return false;
	int idx = from_cookie[cookie];
	from_cookie.erase(from_cookie.find(cookie));
	from_id.erase(from_id.find(ulist[idx].id));
	ulist[idx].id = 0;
	maintain_tree(idx);
	idle.push(idx);
	return true;
}

void Users::update(const USER &u)
{
	std::lock_guard<std::mutex> lock(mutex);
	int idx = from_cookie[u.cookie];
	ulist[idx] = u;
	maintain_tree(idx);
}

USER Users::select(const std::string &cookie)
{
	std::lock_guard<std::mutex> lock(mutex);
	if(!from_cookie.count(cookie))return USER({0});
	return ulist[from_cookie[cookie]];
}

bool Users::exist(int64_t id, std::string &cookie)
{
	std::lock_guard<std::mutex> lock(mutex);
	if(!from_id.count(id))return false;
	cookie = from_id[id];
	return true;
}

USER Users::select_active(time_t cn_t)
{
	if(min_nxtclk.load() > cn_t)return USER({0});
	std::lock_guard<std::mutex> lock(mutex);
	return ulist[sgtree[1]];
}

bool Users::skip_or_undo(const std::string &cookie)
{
	std::lock_guard<std::mutex> lock(mutex);
	if(!from_cookie.count(cookie))return false;
	int idx = from_cookie[cookie];
	if(ulist[idx].skip)ulist[idx].skip = 0, ulist[idx].nxtclk -= 86400;
	else ulist[idx].skip = ulist[idx].nxtclk / 86400, ulist[idx].nxtclk += 86400;
	maintain_tree(idx);
	return true;
}

void Users::iterate_all(std::function<void(USER&)> func)
{
	std::lock_guard<std::mutex> lock(mutex);
	for(auto it : from_cookie)
	{
		time_t nxtclk_old = ulist[it.second].nxtclk;
		func(ulist[it.second]);
		if(ulist[it.second].nxtclk != nxtclk_old)
			maintain_tree(it.second);
	}
}

Users usrdb;