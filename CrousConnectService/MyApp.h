#pragma once

#include "SampleService.h"

#include <ctime>
#include <string>

class MyApp
{
public:
	MyApp(CSampleService& underlyingService);
	~MyApp();

	bool run();

private:
	time_t m_begin;
	bool m_isCrousConnected;
	std::string m_crousGateway;
	std::string m_postData;
	CSampleService& m_underlyingService;
};

