#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "winsock2.h"
#include <ws2tcpip.h>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <map>
#include <filesystem> // C++17 for file path handling

using namespace std;
#pragma comment(lib,"ws2_32.lib")

std::map<std::string, std::string> readConfigFile(const std::string& filename) {
	std::map<std::string, std::string> config;
	std::ifstream file(filename);
	std::string line;

	while (std::getline(file, line)) {
		std::istringstream iss(line);
		std::string key, value;

		if (std::getline(iss, key, '=') && std::getline(iss, value)) {
			config[key] = value;
		}
	}
	return config;
}

void main() {
	std::string configPath = "C:\\Users\\30210\\source\\repos\\server\\config.ini";
	std::map<std::string, std::string> config = readConfigFile(configPath);

	// ºÏ≤È≈‰÷√
	if (config.find("address") == config.end() || config.find("port") == config.end()) {
		std::cerr << "Missing required config parameters." << std::endl;
		return;
	}

	std::string address = config["address"];
	int port = std::stoi(config["port"]);


	WSADATA wsaData;
	string input;

	int nRc = WSAStartup(0x0202, &wsaData);

	if (nRc) {
		printf("Winsock  startup failed with error!\n");
	}

	if (wsaData.wVersion != 0x0202) {
		printf("Winsock version is not correct!\n");
	}

	printf("Winsock  startup Ok!\n");


	SOCKET clientSocket;
	sockaddr_in serverAddr, clientAddr;

	int addrLen;

	//create socket
	clientSocket = socket(AF_INET, SOCK_STREAM, 0);

	if (clientSocket != INVALID_SOCKET)
		printf("Socket create Ok!\n");

	//set client port and ip
	clientAddr.sin_family = AF_INET;
	clientAddr.sin_port = htons(10101);
	clientAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	//binding
	int rtn = bind(clientSocket, (LPSOCKADDR)&clientAddr, sizeof(clientAddr));
	if (rtn != SOCKET_ERROR)
		printf("Socket bind Ok!\n");

	//set server's ip and port
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

	rtn = connect(clientSocket, (LPSOCKADDR)&serverAddr, sizeof(serverAddr));
	if (rtn == SOCKET_ERROR)
		printf("Connect to server error!\n");

	printf("Connect to server ok!");
	std::cout << "Client is linking on " << address << ":" << port << std::endl;

	do {
		cout << "\nPlease input your message:";
		cin >> input;

		//send data to server
		rtn = send(clientSocket, input.c_str(), input.length(), 0);
		if (rtn == SOCKET_ERROR) {
			printf("Send to server failed\n");
			closesocket(clientSocket);
			WSACleanup();
			return;
		}
	} while (input != "quit");
	closesocket(clientSocket);
	WSACleanup();
}