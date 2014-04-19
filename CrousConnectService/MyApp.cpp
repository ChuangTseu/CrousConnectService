#include <winsock2.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <Winbase.h>
#include <Wininet.h>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <ctime>
#include <sstream>
#include <iostream>
#include <memory>

#include "AppData.h"

#include "MyApp.h"



#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Wininet.lib")

bool printAdaptatersInfosIpv4();
bool sendPostRequest(const char* serverIp, unsigned short serverPort, const char* postData);
bool getCrousGateway(std::string& gatewayStr);


#define WORKING_BUFFER_SIZE 15000
#define MAX_TRIES 3

#define MALLOC(x) malloc(x)
#define FREE(x) free(x)


MyApp::MyApp(CSampleService& underlyingService) : m_underlyingService(underlyingService)
{
	time(&m_begin);
	m_isCrousConnected = false;

	char serviceExePath[512];
	GetModuleFileNameA(nullptr, serviceExePath, 512);

	std::string serviceExePathStr(serviceExePath);

	//Logging with fucking Wide Char stuff
	WCHAR wBob[512];
	WCHAR logWstr[512];
	MultiByteToWideChar(0, 0, serviceExePath, 512, wBob, 512);
	m_underlyingService.WriteEventLogEntry(logWstr, EVENTLOG_INFORMATION_TYPE);

	std::string::size_type pos = serviceExePathStr.find_last_of("\\/");
	serviceExePathStr = serviceExePathStr.substr(0, pos);
	serviceExePathStr.append("\\CrousConnectService_login.txt");	

	std::cout << "serviceExePathStr : " << serviceExePathStr << "\n\n";

	std::ifstream loginFile(serviceExePathStr, std::ifstream::in);

	char line[512];

	m_postData = "auth_user=";
	loginFile.getline(line, 512);
	m_postData.append(line);
	m_postData.append("&auth_pass=");
	loginFile.getline(line, 512);
	m_postData.append(line);
	m_postData.append("&redirurl=&accept=Continue");
}


MyApp::~MyApp()
{
}

bool MyApp::run()
{
	time_t now;
	double timeElapsed;

	time(&now);
	timeElapsed = difftime(now, m_begin);

	std::string crousGateway;

	const double timeBeforeRefresh = 60*5;

	if (getCrousGateway(crousGateway))
	{
		printf("Connected to crous gateway '%s'\n", crousGateway.c_str());

		//Refresh connection when newly connected, changing gateway or each 5 minutes
		if (!m_isCrousConnected || crousGateway != m_crousGateway || timeElapsed > timeBeforeRefresh)
		{
			printf("\tRefreshing connection\n\t(newly connected %d / changing gateway %d / timeElapsed %d)\n",
				!m_isCrousConnected, crousGateway != m_crousGateway, timeElapsed > timeBeforeRefresh);

			m_crousGateway = crousGateway;
			time(&m_begin);

			//Better fix, need to check cheking POST success works
			printf("sendPostRequest, Try %d\n", 0);
			for (int i = 0; !sendPostRequest(m_crousGateway.c_str(), 8000, m_postData.c_str()) && i < 50; ++i)
			{
				printf("sendPostRequest, Try %d\n", i + 1);
				::Sleep(100);
			}
		}

		m_isCrousConnected = true;		
	}	
	else
	{
		m_isCrousConnected = false;
	}

	return true;
}


template<typename T>
void printCastAddresses(T* cast)
{
	for (int i = 0; cast != nullptr; i++) {
		printf("%s\n", inet_ntoa(((sockaddr_in*)(cast->Address.lpSockaddr))->sin_addr));

		cast = cast->Next;
	}
}

bool sendPostRequest(const char* serverIp, unsigned short serverPort, const char* postData)
{
	bool ret = false;
	HINTERNET Initialize, Connection, Post;
	DWORD dwBytes;

	char ch;
	Initialize = InternetOpenA("HTTPPOST", INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
	
	Connection = InternetConnectA(Initialize, serverIp, serverPort,
		nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);

	Post = HttpOpenRequestA(Connection,
		"POST",
		"/",
		"HTTP/1.1",
		nullptr, nullptr,
		INTERNET_FLAG_RELOAD | INTERNET_FLAG_EXISTING_CONNECT, 0); 	

	if (HttpSendRequestA(Post, "Content-Type: application/x-www-form-urlencoded", -1, (LPVOID)postData, strlen(postData)))
	{
		std::ostringstream httpAnswer;

		while (InternetReadFile(Post, &ch, 1, &dwBytes))
		{
			if (dwBytes != 1) break;
			httpAnswer << ch;
		}

		//HTML webpage returned when successfully logged to Crous contains this "Redirecting..."
		if (httpAnswer.str().find("Redirecting..."))
		{
			ret = true;
		}
	}

	InternetCloseHandle(Post);
	InternetCloseHandle(Connection);
	InternetCloseHandle(Initialize);

	return ret;
}

PIP_ADAPTER_ADDRESSES getAdaptatersInfosIpv4()
{
	DWORD dwSize = 0;
	DWORD dwRetVal = 0;

	unsigned int i = 0;

	// Set the flags to pass to GetAdaptersAddresses
	ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_INCLUDE_GATEWAYS;

	// default to IPv4 address family
	ULONG family = AF_INET;

	LPVOID lpMsgBuf = nullptr;

	PIP_ADAPTER_ADDRESSES pAddresses = nullptr;
	ULONG outBufLen = 0;
	ULONG Iterations = 0;

	// Allocate a 15 KB buffer to start with.
	outBufLen = WORKING_BUFFER_SIZE;

	do {

		pAddresses = (IP_ADAPTER_ADDRESSES *)MALLOC(outBufLen);
		if (pAddresses == nullptr) {
			printf
				("Memory allocation failed for IP_ADAPTER_ADDRESSES struct\n");

			return nullptr;
		}

		dwRetVal =
			GetAdaptersAddresses(family, flags, nullptr, pAddresses, &outBufLen);

		if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
			FREE(pAddresses);
			pAddresses = nullptr;
		}
		else {
			break;
		}

		Iterations++;

	} while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_TRIES));

	if (dwRetVal == NO_ERROR) {
		return pAddresses;
	}
	else {
		printf("Call to GetAdaptersAddresses failed with error: %d\n",
			dwRetVal);
		if (dwRetVal == ERROR_NO_DATA)
			printf("\tNo addresses were found for the requested parameters\n");
		else {

			if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr, dwRetVal, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				// Default language
				(LPTSTR)& lpMsgBuf, 0, nullptr)) {
				printf("\tError: %s", lpMsgBuf);
				LocalFree(lpMsgBuf);
				if (pAddresses)
					FREE(pAddresses);
			}
		}
	}

	return nullptr;
}

bool printAdaptatersInfosIpv4()
{
	unsigned int i = 0;

	//std::unique_ptr<_IP_ADAPTER_ADDRESSES_LH> pAddresses = nullptr;

	PIP_ADAPTER_ADDRESSES pCurrAddresses = nullptr;
	PIP_ADAPTER_UNICAST_ADDRESS pUnicast = nullptr;
	PIP_ADAPTER_GATEWAY_ADDRESS pGateway = nullptr;
	IP_ADAPTER_DNS_SERVER_ADDRESS *pDnServer = nullptr;

	//pAddresses = getAdaptatersInfosIpv4();
	std::unique_ptr<IP_ADAPTER_ADDRESSES_LH> pAddresses(getAdaptatersInfosIpv4());

	if (pAddresses) {
		// If successful, output some information from the data we received
		pCurrAddresses = pAddresses.get();
		while (pCurrAddresses) {
			printf("\tDescription: %wS\n", pCurrAddresses->Description);
			printf("\tFriendly name: %wS\n", pCurrAddresses->FriendlyName);
			printf("\tIfIndex (IPv4 interface): %u\n", pCurrAddresses->IfIndex);
			//printf("\tAdapter name: %s\n", pCurrAddresses->AdapterName);

			pUnicast = pCurrAddresses->FirstUnicastAddress;
			if (pUnicast != nullptr) {

				for (i = 0; pUnicast != nullptr; i++, pUnicast = pUnicast->Next);
				printf("\tNumber of Unicast Addresses: %d\n", i);
				pUnicast = pCurrAddresses->FirstUnicastAddress;

				printCastAddresses(pUnicast);				
			}
			else
				printf("\tNo Unicast Addresses\n");

			pDnServer = pCurrAddresses->FirstDnsServerAddress;
			if (pDnServer) {
				for (i = 0; pDnServer != nullptr; i++, pDnServer = pDnServer->Next);
				printf("\tNumber of DNS Server Addresses: %d\n", i);
				pDnServer = pCurrAddresses->FirstDnsServerAddress;

				printCastAddresses(pDnServer);
			}
			else
				printf("\tNo DNS Server Addresses\n");

			pGateway = pCurrAddresses->FirstGatewayAddress;
			if (pGateway) {
				for (i = 0; pGateway != nullptr; i++, pGateway = pGateway->Next);
				printf("\tNumber of Gateway Addresses: %d\n", i);
				pGateway = pCurrAddresses->FirstGatewayAddress;

				printCastAddresses(pGateway);
			}
			else
				printf("\tNo Gateway Addresses\n");

			printf("\tIfType: %ld\n", pCurrAddresses->IfType);

			printf("\n\n");

			pCurrAddresses = pCurrAddresses->Next;
		}

		return true;
	}
	else
	{
		return false;
	}

}

template<typename V, typename T>
bool isInside(V& vec, T val)
{
	return std::find(vec.begin(), vec.end(), val) != vec.end();
}

bool getCrousGateway(std::string& gatewayStr)
{
	unsigned int i = 0;

	PIP_ADAPTER_ADDRESSES pCurrAddresses = nullptr;
	PIP_ADAPTER_UNICAST_ADDRESS pUnicast = nullptr;
	PIP_ADAPTER_GATEWAY_ADDRESS pGateway = nullptr;
	IP_ADAPTER_DNS_SERVER_ADDRESS *pDnServer = nullptr;

	std::unique_ptr<IP_ADAPTER_ADDRESSES_LH> pAddresses(getAdaptatersInfosIpv4());

	if (pAddresses) 
	{
		pCurrAddresses = pAddresses.get();
		while (pCurrAddresses) 
		{
			pGateway = pCurrAddresses->FirstGatewayAddress;
			if (pGateway) 
			{
				for (int i = 0; pGateway != nullptr; i++) 
				{
					char* gatewayIp = inet_ntoa(((sockaddr_in*)(pGateway->Address.lpSockaddr))->sin_addr);

					if (isInside(KnownGateways::Crous::Ips, gatewayIp))
					{
						gatewayStr = gatewayIp;
						return true;
					}

					pGateway = pGateway->Next;
				}
			}

			pCurrAddresses = pCurrAddresses->Next;
		}

	}

	return false;
}


