#pragma once

class SystemCore {
	std::thread sysThread;
	UINT tickSpeed;
	BOOL sysThreadActive;
public:
	SystemCore();

	static VOID System(SystemCore* sysCORE);
};