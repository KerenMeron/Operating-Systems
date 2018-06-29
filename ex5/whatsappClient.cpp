

#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <map>
#include <vector>
#include <iostream>
#include <zconf.h>
#include <sys/param.h>
#include <netdb.h>
#include <cstring>
#include <stdio.h>
#include <arpa/inet.h>


#define MAX_WAITING_SOCKETS 10
#define MAX_INPUT_MSG 1000
#define NO_CONNECTION 2
#define EXIT_CLIENT 33
#define BAD_EXIT 44
#define NO_CMD_TYPE -1

char * readIntoBuffer;
typedef struct sockaddr_in whatsAppSocket;
fd_set selectSet;
fd_set cinSet;


/** messages */
const char* gotConnection = "Connection received.\n";
const char* successfulConnection = "Connected Successfully.\n";
const char* takenName = "Client name is already in use.\n";
const char* unregister = "Unregister me.\n";
const char* gotMessage = "Got message.\n";
const std::string createGroupCmdForDict = "create_group";
const std::string sendCmdForDict = "send";
const std::string whoCmdForDict = "who";
const std::string exitCmdForDict = "exit";
const std::string serverTerminating = "Terminate.\n";


const std::map<const std::string, const int> commandDict = {std::make_pair(createGroupCmdForDict, 0),
															std::make_pair(sendCmdForDict, 1),
															std::make_pair(whoCmdForDict, 2),
															std::make_pair(exitCmdForDict, 3)};

enum cmdEnum{createGroupCmd, sendCmd, whoCmd, exitCmd};


class WhatsAppClient {
private:
	whatsAppSocket *localSocket;
	std::string name;
	int serverPort;
	int socketFd;
	char *ipAddress;



	/**
	 * Parse message from user and send to client accordingly.
	 */
	int parseMessage(const char* msg)
	{
		// get command type
		std::string command;
		int i = 0;
		while (msg[i] != ' ' and msg[i] != '\n')
		{
			command += msg[i];
			++i;
		}

		// get message and count number of words
		bool valid = true;
		int spaces = 0;
		std::string message = "";
		while (msg[i] != '\n')
		{
			++i;
			// traverse entire first word even if error was found, for the error message
			if (valid and spaces == 0 and msg[i] != ' ' and msg[i] != ',' and (msg[i] < 'a' or
					msg[i] > 'z')
				and (msg[i] < 'A' or msg[i] > 'Z') and (msg[i] < '0' or msg[i] > '9'))
			{
				// invalid char found
				valid = false;
			}
			if (spaces > 0 and not valid)
			{
				break;
			}
			message += msg[i];
			if (msg[i] == ' ')
			{
				spaces += 1;
			}
		}
		// if there was a failure, it was in the first word after 'command type', and only that
		// word will be in the message variable.
		if (not valid)
		{
			if (command == "create_group")
			{
				message.pop_back();
				std::cout << "ERROR: failed to create group " << message << "." << std::endl;
			}
			else if (command == "send")
			{
				std::cout << "ERROR: failed to send." << std::endl;
			}
			else if (command == "who")
			{
				std::cout << "ERROR: failed to receive list of connected clients." << std::endl;
			}
			else
			{
				std::cout << "ERROR: Invalid input." << std::endl;
			}
			return 0;
		}


		// ==== command was valid so far, send over to server ====

		std::string emptyMessage("");
		auto commandNum = commandDict.find(command);
		if (commandNum == commandDict.end()) {
			std::cout << "ERROR: Invalid input." << std::endl;
			return 0;
		}
		// handles the different commands. If there is an error, will print ERROR
		std::string nameOfUser;
		std::string groupName;
		switch (commandNum->second)
		{
			case cmdEnum::createGroupCmd:
				if (spaces == 1)
				{
					return sendMsgToServer(cmdEnum::createGroupCmd, message.c_str(), true);
				}
				// extract group name
				groupName = message.substr(0, message.find(' '));
				std::cout << "ERROR: failed to create group " << groupName << "." << std::endl;
				return 0;
			case cmdEnum::sendCmd:
				nameOfUser = message.substr(0, message.find(' '));
				if (nameOfUser == name)
				{
					break;
				}
				return sendMsgToServer(cmdEnum::sendCmd, message.c_str(), true);
			case cmdEnum::whoCmd:
				if (!strcmp(message.c_str(), ""))
				{
					return sendMsgToServer(cmdEnum::whoCmd, message.c_str(), true);
				}
				std::cout << "ERROR: failed to receive list of connected clients." << std::endl;
				return 0;
			case cmdEnum::exitCmd:
				if (!strcmp(message.c_str(), ""))
				{
					if (sendMsgToServer(cmdEnum::exitCmd, message.c_str(), false) < 0)  { return -1;}
					return EXIT_CLIENT;
				}
				break;
			default:
				std::cout << "ERROR: Invalid input." << std::endl;
				return 0;
		}
		std::cout << "ERROR: Invalid input." << std::endl;
		return 0;
	}

	/**
	 * Send message with command type to server. Receive response from server and print out.
	 */
	int sendMsgToServer(int cmdType, const char* msg,  bool getConfirmation=false)
	{
		std::string message(msg);
		std::string fullMsg = message + "\n";
		// when NO_CMD_TYPE, there is no cmdType required by protocol
		if (cmdType != NO_CMD_TYPE)
		{
			fullMsg = std::to_string(cmdType) + " " + message + "\n";
		}
		char fullMsgChar[MAX_INPUT_MSG];
		std::fill(fullMsgChar, fullMsgChar + MAX_INPUT_MSG, '\0');
		memcpy(fullMsgChar, fullMsg.c_str(), strlen(fullMsg.c_str()));

		int retVal = 0;
		do {
			errno = 0;
			retVal += send(socketFd, fullMsgChar+retVal, MAX_INPUT_MSG - retVal, 0);
			if (retVal == MAX_INPUT_MSG) {
				break;
			}
			if (retVal < 0)
			{
				std::cout << "ERROR: send " << errno << "."<< std::endl;
				std::string emptyMessage("");
				sendMsgToServer(cmdEnum::exitCmd, emptyMessage.c_str());
				return -1;
			}
		} while (errno == 11 or retVal != MAX_INPUT_MSG);

		// get reply from server into readIntoBuffer
		if (getConfirmation)
		{
			if (receiveFromSocket(socketFd) < 0) {
				sendMsgToServer(cmdEnum::exitCmd, msg);
				return -1;
			}

			std::string receivedMsg(readIntoBuffer);
			receivedMsg.pop_back();
			std::cout << receivedMsg << std::endl;
		}
		return 0;
	}


	/**
	* Create main socket attribute it with host name and port number.
	* @return 0 upon success, -1 upon failure.
	*/
	int establish() {

		memset(localSocket, 0, sizeof(*localSocket));
		localSocket->sin_addr.s_addr = inet_addr(ipAddress);
		localSocket->sin_family = AF_INET;
		localSocket->sin_port = htons(serverPort);
		return 0;
	}


	/**
	 * Receives data from socket with fd. Sets readIntoBuffer with data.
	 * Assumed message is sent each time as a block of length MAX_INPUT_MSG.
	 * Returns -1 upon failure.
	 */
	int receiveFromSocket(int fd)
	{
		int retVal = 0;
		memset(readIntoBuffer, 0, sizeof(char) * MAX_INPUT_MSG);
		do {
			errno = 0;
			retVal += recv(fd, (void *) (readIntoBuffer + retVal), MAX_INPUT_MSG - retVal, 0);
			if (retVal < 0)
			{
				std::cout << "ERROR: recv " << errno << "."<< std::endl;
				std::string emptyMessage("");
				sendMsgToServer(cmdEnum::exitCmd, emptyMessage.c_str(), false);
				return -1;
			}
		} while (errno == 11 or retVal != MAX_INPUT_MSG);
		return 0;
	}




public:
	/** Constructor. */
	WhatsAppClient(const char *clientName, int serverPort, char *address) :
			name(clientName), serverPort(serverPort), ipAddress(address)
	{
		localSocket = new whatsAppSocket();
		establish();
		readIntoBuffer = new char[MAX_INPUT_MSG];
	};

	/** Destructor. */
	~WhatsAppClient()
	{
		delete localSocket;
		close(socketFd);
		delete [] readIntoBuffer;
	}

	/**
	 * Set up socket and connect to server.
	 */
	int connectToServer() {
		if ((socketFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			std::cout << "ERROR: bind " << errno << "."<< std::endl;
			return -1;
		}
		if (connect(socketFd, (struct sockaddr*)localSocket, sizeof(*localSocket)) < 0)
		{
			std::cout << "ERROR: connect " << errno << "."<< std::endl;
			return -1;
		}

		// create sets for use of select() when communicate()
		FD_ZERO(&selectSet);
		FD_SET(socketFd, &selectSet);
		FD_ZERO(&cinSet);
		FD_SET(0, &cinSet);

		// ask for connection by client name
		sendMsgToServer(NO_CMD_TYPE, name.c_str(), true);

		// check confirmation message from server
		if (strncmp(readIntoBuffer, takenName, strlen(takenName)) == 0) {
			return NO_CONNECTION;
		}
		return 0;
	}

	/**
	 * Client's socket has been set up, and now he will communicate in an ongoing loop with the
	 * server.
	 */
	int communicate()
	{
		struct timeval tv;
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		while (true)
		{
			// check std::cin
			char buf[MAX_INPUT_MSG] = {0};
			std::string input(buf);

			fd_set copySet(cinSet);
			if (select(1, &copySet, NULL, NULL, &tv) > 0)
			{
				getline(std::cin, input);
				input += "\n";
				int retVal = parseMessage(input.c_str());
				if (retVal < 0)
				{
					std::string emptyMessage("");
					sendMsgToServer(cmdEnum::exitCmd, emptyMessage.c_str());
					return -1;
				}
				else if (retVal == EXIT_CLIENT) {return 0;}

			}

			// check if server has sent message and if so, handle it (cout)
			int selected = 1;
			while (selected > 0)
			{
				fd_set copySocketSet(selectSet);
				selected = select(socketFd + 1, &copySocketSet, NULL, NULL, &tv);
				if (selected > 0)
				{
					if (receiveFromSocket(socketFd) < 0) { return -1; }
					std::string received(readIntoBuffer);

					// server is closing, confirm message received, and end self
					if (received == serverTerminating)
					{
						sendMsgToServer(NO_CMD_TYPE, gotMessage, false);
						return BAD_EXIT;
					}

					// confirm received message
					sendMsgToServer(NO_CMD_TYPE, gotMessage, false);

					received.pop_back();
					std::cout << received << std::endl;
				}
			}
		}
	}






};


int main (int argc, char* argv[])
{
	if (argc != 4)
	{
		std::cerr << "Invalid arguments." << std::endl;
		return -1;
	}
	char* clientName = argv[1];

	// check that name is legal
	for (unsigned int i = 0; i < strlen(clientName); ++i)
	{
		if ((clientName[i] < 'a' or clientName[i] > 'z') and (clientName[i] < 'A' or clientName[i] > 'Z') and
			(clientName[i] < '0' or clientName[i] > '9'))
		{
			// invalid char found
			std::cerr << "Invalid arguments." << std::endl;
			return -1;
		}
	}

	std::string serverAddress = argv[2];
	int serverPort = atoi(argv[3]);
	WhatsAppClient* client = new WhatsAppClient(clientName, serverPort, argv[2]);
	int connectionSuccess = client->connectToServer();
	if (connectionSuccess != 0)
	{
		delete client;
		exit(int(NO_CONNECTION == connectionSuccess));
	}
	int retVal = client->communicate();
	std::cout << "Unregistered successfully." << std::endl;
	delete client;
	if (retVal == BAD_EXIT)
	{
		exit(1);
	}
	exit(int(-1 == retVal));

}

