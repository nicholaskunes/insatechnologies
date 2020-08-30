#include "../GLOBAL.h"

SystemCore* SYSCORE;

SystemCore::SystemCore()
{
	this->sysThread = std::thread(SystemCore::System, this);
	this->tickSpeed = 25;
	this->sysThreadActive = FALSE;
}

VOID SystemCore::System(SystemCore* sysCORE)
{
	while (sysCORE->sysThreadActive) {
		Sleep(sysCORE->tickSpeed);

		// DO SYSTEM TASKS

	}
}
