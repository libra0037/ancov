#define ARRAY_LENGTH 65536

enum Status
{
	Waiting = 0,
	Retrying = 1,
	Done = 2,
	Abandoned = 3,
	Skipped = 4,
};

struct USER
{
	char cookie[32]; // primary key, 'eai-sess', no changes
	int64_t id; // student id, no changes
	time_t lasclk; // GMT+8
	time_t nxtclk; // GMT+8
	int result; // [31:16] failures, [15:0] last state
	int skip; // the day skips
	Status status(time_t today)const;
};

class Users
{
public:
	void open(const char *file);
	void close();
	bool insert(const USER &u);
	bool remove(const std::string &cookie);
	void update(const USER &u);
	USER select(const std::string &cookie);
	bool exist(int64_t id, std::string &cookie);
	USER select_active(time_t cn_t);
	bool skip_or_undo(const std::string &cookie);
	void iterate_all(std::function<void(USER&)> func);
private:
	void maintain_tree(int idx);
	std::mutex mutex;
	USER *ulist;
	int *sgtree;
	std::atomic_llong min_nxtclk;
	std::unordered_map<std::string, int> from_cookie;
	std::unordered_map<int64_t, std::string> from_id;
	std::stack<int> idle;
#ifdef _WIN32
	HANDLE hfile, hmap;
#endif
};

extern Users usrdb;