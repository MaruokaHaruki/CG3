#include "Log.h"

void Log(const std::string& message) {
	OutputDebugStringA(message.c_str());
}