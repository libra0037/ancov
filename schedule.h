class Schedule
{
public:
	void start();
	void stop();
	static void clockin(USER &u);
private:
	std::atomic_bool running;
	std::thread thread;
};

extern Schedule schedule;