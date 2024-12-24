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

std::string getLocalIP() {
	char hostname[256];
	if (gethostname(hostname, sizeof(hostname)) != 0) {
		return "127.0.0.1"; // 如果无法获取，返回默认地址
	}

	struct addrinfo hints = {}, * info;
	hints.ai_family = AF_INET; // IPv4
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(hostname, NULL, &hints, &info) != 0) {
		return "127.0.0.1"; // 如果失败，返回默认地址
	}

	std::string ipAddress = inet_ntoa(((struct sockaddr_in*)info->ai_addr)->sin_addr);
	freeaddrinfo(info); // 释放资源
	return ipAddress;
}

void createConfigFile(const std::string& directory) {
	// 设置 config.ini 文件路径
	std::string configFilePath = directory + "\\config.ini";

	std::ofstream configFile(configFilePath);
	if (configFile.is_open()) {
		std::string localIP = getLocalIP();
		configFile << "address=" << localIP << "\n";
		configFile << "port=15050\n";
		configFile << "root_dir=" << directory << "\\web_directory\n";
		configFile.close();
		std::cout << "Config file created/updated: " << configFilePath << std::endl;
	}
	else {
		std::cerr << "Unable to create config file at: " << configFilePath << std::endl;
	}
}

std::map<std::string, std::string> readConfigFile(const std::string& filePath) {
	std::map<std::string, std::string> config;
	std::ifstream configFile(filePath);

	// 检查文件是否打开成功
	if (!configFile.is_open()) {
		std::cerr << "Failed to open config file: " << filePath << std::endl;
		return config;
	}

	std::string line;
	while (std::getline(configFile, line)) {
		// 跳过空行
		if (line.empty()) continue;

		// 找到等号分隔符的位置
		size_t delimiterPos = line.find('=');
		if (delimiterPos != std::string::npos) {
			// 提取 key 和 value
			std::string key = line.substr(0, delimiterPos);
			std::string value = line.substr(delimiterPos + 1);

			// 移除 key 和 value 中可能存在的空格
			key.erase(key.find_last_not_of(" \t\n\r\f\v") + 1);
			value.erase(0, value.find_first_not_of(" \t\n\r\f\v"));

			// 保存到 map 中
			config[key] = value;
		}
		else {
			std::cerr << "Invalid line in config file: " << line << std::endl;
		}
	}

	configFile.close();
	return config;
}

void sendResponse(SOCKET sessionSocket, const std::string& filePath) {
		// 以二进制模式打开文件
	std::ifstream file(filePath, std::ios::binary);
	if (!file) {
		// 文件不存在，返回 404
		std::string response = "HTTP/1.1 404 Not Found\r\n\r\n";
		send(sessionSocket, response.c_str(), response.size(), 0);
		return;
	}

	// 确定内容类型
	std::string contentType;
	if (filePath.ends_with(".html")) {
		contentType = "text/html";
	}
	else if (filePath.ends_with(".css")) {
		contentType = "text/css";
	}
	else if (filePath.ends_with(".js")) {
		contentType = "application/javascript";
	}
	else if (filePath.ends_with(".png")) {
		contentType = "image/png";
	}
	else if (filePath.ends_with(".jpg") || filePath.ends_with(".jpeg")) {
		contentType = "image/jpeg";
	}
	else if (filePath.ends_with(".gif")) {
		contentType = "image/gif";
	}
	else {
		contentType = "application/octet-stream"; // 默认类型
	}

	// 获取文件大小
	file.seekg(0, std::ios::end);
	std::streamsize fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	// 创建 HTTP 响应头
	std::string response = "HTTP/1.1 200 OK\r\n";
	response += "Content-Type: " + contentType + "\r\n";
	response += "Content-Length: " + std::to_string(fileSize) + "\r\n";
	response += "Cache-Control: max-age=3600\r\n";
	response += "Connection: keep-alive\r\n";
	response += "Keep-Alive: timeout=5, max=100\r\n\r\n";

	// 发送响应头
	send(sessionSocket, response.c_str(), response.size(), 0);

	// 发送文件内容
	char buffer[4096];
	while (file.read(buffer, sizeof(buffer))) {
		send(sessionSocket, buffer, file.gcount(), 0);
	}
	// 发送剩余的部分
	if (file.gcount() > 0) {
		send(sessionSocket, buffer, file.gcount(), 0);
	}

	file.close();
	
}

void handleRequest(SOCKET sessionSocket, const char* requestData, int dataLength, string root_dir) {
	// 缓冲区
	char buffer[4096];
	memset(buffer, 0, sizeof(buffer));
	memcpy(buffer, requestData, dataLength);
	buffer[dataLength] = '\0';

	// 解析请求
	std::string request(buffer);
	std::string requestLine = request.substr(0, request.find("\r\n"));

	if (requestLine.empty() || requestLine.find(" ") == std::string::npos) {
		std::cerr << "无效请求行: " << requestLine << std::endl;
		return;
	}

	size_t firstSpace = requestLine.find(" ");
	size_t secondSpace = requestLine.find(" ", firstSpace + 1);

	if (secondSpace == std::string::npos) {
		std::cerr << "无效请求行: " << requestLine << std::endl;
		return;
	}

	// 文件路径
	std::string filePath = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	filePath = root_dir + filePath;

	sendResponse(sessionSocket, filePath);

}


void main() {

	std::string currentDir = std::filesystem::current_path().string();

	//创建或更新 config.ini 文件
	createConfigFile(currentDir);
	WSADATA wsaData;
	fd_set rfds;				//用于检查socket是否有数据到来的的文件描述符，用于socket非阻塞模式下等待网络事件通知（有数据到来）
	fd_set wfds;				//用于检查socket是否可以发送的文件描述符，用于socket非阻塞模式下等待网络事件通知（可以发送数据）
	bool first_connetion = true;
	int nRc = WSAStartup(0x0202, &wsaData);
	// 检错部分
	if (nRc) {
		printf("Winsock  startup failed with error!\n");
	}
	if (wsaData.wVersion != 0x0202) {
		printf("Winsock version is not correct!\n");
	}
	printf("Winsock  startup Ok!\n");
	std::map<std::string, std::string> config = readConfigFile("config.ini");

	// 检查是否正确读取了需要的配置项
	if (config.find("address") == config.end() || config.find("port") == config.end() || config.find("root_dir") == config.end()) {
		std::cerr << "Missing required config parameters." << std::endl;
		return ;
	}

	std::string address = config["address"];
	int port = std::stoi(config["port"]);
	std::string root_dir = config["root_dir"];



	int addrLen;
	//create socket
	//SOCKET srvSocket = socket(AF_INET, SOCK_STREAM, 0);
	//if (srvSocket != INVALID_SOCKET) {

	//	printf("Socket create Ok!\n");
	//}
	//else {
	//	std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
	//	return;
	//}
	////set port and ip
	//sockaddr_in serverAddr;
	//serverAddr.sin_family = AF_INET;
	//serverAddr.sin_port = htons(port);
	//if (inet_pton(AF_INET, address.c_str(), &serverAddr.sin_addr) <= 0) {
	//	std::cerr << "Invalid address: " << address << std::endl;
	//	closesocket(srvSocket);
	//	WSACleanup();
	//	return;
	//}
	////binding
	//int rtn = bind(srvSocket, (LPSOCKADDR)&serverAddr, sizeof(serverAddr));
	//if (rtn != SOCKET_ERROR)
	//	printf("Socket bind Ok!\n");
	////listen
	//rtn = listen(srvSocket, 5);
	//if (rtn != SOCKET_ERROR)
	//	printf("Socket listen Ok!\n");
	//std::cout << "Server is listening on " << address << ":" << port << std::endl;
	//以上为客户端的固定设计部分




	SOCKET srvSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (srvSocket != INVALID_SOCKET) {
		cout << "socket create ok!" << endl;
	}else {
		std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
		return;
	}
	//set port and ip
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	if (inet_pton(AF_INET, address.c_str(), &serverAddr.sin_addr) <= 0) {
		std::cerr << "Invalid address: " << address << std::endl;
		closesocket(srvSocket);
		WSACleanup();
		return;
	}
	//binding
	int rtn = bind(srvSocket, (LPSOCKADDR)&serverAddr, sizeof(serverAddr));
	if (rtn != SOCKET_ERROR)
		printf("Socket bind Ok!\n");
	//listen
	rtn = listen(srvSocket, 5);
	if (rtn != SOCKET_ERROR)
		printf("Socket listen Ok!\n");
	std::cout << "Server is listening on " << address << ":" << port << std::endl;








	
	//一下为与客户端的相连设计
	sockaddr_in clientAddr;
	SOCKET sessionSocket = INVALID_SOCKET;
	clientAddr.sin_family = AF_INET;
	addrLen = sizeof(clientAddr);
	char recvBuf[4096];
	

	u_long blockMode = 1;//将srvSock设为非阻塞模式以监听客户连接请求

	if ((rtn = ioctlsocket(srvSocket, FIONBIO, &blockMode) == SOCKET_ERROR)) { //FIONBIO：允许或禁止套接口s的非阻塞模式。
		std::cerr << "ioctlsocket() failed with error code: " << WSAGetLastError() << std::endl;
		closesocket(sessionSocket);
		return;
	}
	cout << "ioctlsocket() for server socket ok!	Waiting for client connection and data\n";

	//清空read,write描述符，对rfds和wfds进行了初始化，必须用FD_ZERO先清空，下面才能FD_SET
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);

	//设置等待客户连接请求
	FD_SET(srvSocket, &rfds);

	while (true) {
		//清空read,write描述符
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);

		//设置等待客户连接请求
		FD_SET(srvSocket, &rfds);

		if (!first_connetion) {
			//设置等待会话SOKCET可接受数据或可发送数据
			FD_SET(sessionSocket, &rfds);
			FD_SET(sessionSocket, &wfds);
		}

		//开始等待
		int nTotal = select(0, &rfds, &wfds, NULL, NULL);

		if (nTotal == SOCKET_ERROR) {
			std::cerr << "select() failed with error: " << WSAGetLastError() << std::endl;
			break; // 终止循环
		}else if (nTotal == 0) {
			continue; // 没有事件发生，继续等待
		}

		//如果srvSock收到连接请求，接受客户连接请求
		if (FD_ISSET(srvSocket, &rfds)) {
			nTotal--;

			//产生会话SOCKET
			sessionSocket = accept(srvSocket, (LPSOCKADDR)&clientAddr, &addrLen);
			
			//把会话SOCKET设为非阻塞模式
			rtn = ioctlsocket(sessionSocket, FIONBIO, &blockMode);
			if ((rtn == SOCKET_ERROR)) { //FIONBIO：允许或禁止套接口s的非阻塞模式。
				std::cerr << "ioctlsocket() failed with error code: " << WSAGetLastError() << std::endl;
				closesocket(sessionSocket);
				continue; // 继续处理其他连接
			}
			else{
				cout << "ioctlsocket() for session socket ok! Waiting for client connection and data\n";
			}
			if (sessionSocket != INVALID_SOCKET) {
							printf("Socket listen one client request\n");
			}else {
				int error = WSAGetLastError();
				if (error != WSAEWOULDBLOCK) { // 处理除WSAEWOULDBLOCK外的其他错误
					std::cerr << "Failed to accept connection Error: " << error << std::endl;
				}
				continue; // 继续等待其他客户端连接
			}
			//设置等待会话SOKCET可接受数据或可发送数据
			FD_SET(sessionSocket, &rfds);
			FD_SET(sessionSocket, &wfds);
			std::cout << "Current session socket state: " << (sessionSocket != INVALID_SOCKET ? "Valid" : "Invalid") << std::endl;

			first_connetion = false;

			if (sessionSocket != INVALID_SOCKET) {
				if (FD_ISSET(sessionSocket, &rfds)) {
					memset(recvBuf, 0, sizeof(recvBuf));
					int rtn = recv(sessionSocket, recvBuf, sizeof(recvBuf), 0);

					if (rtn > 0) {
						std::cout << "Received " << rtn << " bytes from client: " << std::string(recvBuf, rtn) << std::endl;
						handleRequest(sessionSocket, recvBuf, rtn, root_dir);
					}
					else if (rtn == 0) {
						std::cout << "Client disconnected." << std::endl;
						cout << " rtn == 0 !!" << endl;
						closesocket(sessionSocket);
						sessionSocket = INVALID_SOCKET;
					}
					else {
						int err = WSAGetLastError();
						if (err != WSAEWOULDBLOCK) {
							std::cerr << "recv() failed with error: " << err << std::endl;
							closesocket(sessionSocket);
							sessionSocket = INVALID_SOCKET;
						}
					}
				}
			}
		}
	}
	closesocket(srvSocket);
	WSACleanup();

	return;
}
	
