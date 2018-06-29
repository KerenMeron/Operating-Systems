
#include <stdio.h>
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
#include <algorithm>

#define MAX_WAITING_SOCKETS 10
#define MAX_INPUT_MSG 1000
#define EXIT 44

enum cmdEnum{createGroupCmd, sendCmd, whoCmd, exitCmd};


typedef struct sockaddr_in whatsAppSocket;

/** Buffers */
const char * gotConnection = "Connection received.\n";
const char* successfulConnection = "Connected successfully.\n";
const char* takenName = "Client name is already in use.\n";
const char* exitMsg = "Unregistered successfully.\n";
const char* sendFail = "ERROR: failed to send.\n";
const char* successfulSent = "Sent successfully.\n";
const char* groupCreatedSuccess = " was created successfully.\n";
const char* groupCreatedFailure = "ERROR: failed to create group \"";
const char* serverTerminating = "Terminate.\n";
const char* gotMessage = "Got message.\n";
const char* invalidMsg = "ERROR: Invalid input.\n";


char * readIntoBuffer;
char sendBuffer[MAX_INPUT_MSG] = {0};
fd_set cinFdset;
struct hostent *hp;


struct timeval tv;


int handleNewSocket();
int establish();
int runServer();




class WhatsAppServer{

private:
	std::string myName = "main socket";
	int portNum;
	int mainSocketFd;
	std::map<std::string, int> * nameToSocketFd;
	std::map<std::string, std::vector<int>*> * groups;
	fd_set socketFds;
	whatsAppSocket* mainSocketAddr;
	whatsAppSocket address;


	/**
 	* Receives data from socket with fd. Sets readIntoBuffer with data. Returns -1 upon failure.
 	*/
	int receiveFromSocket(int fd) {
		int retVal = 0;
		memset(readIntoBuffer, 0, sizeof(char) * MAX_INPUT_MSG);
		do {
			errno = 0;
			retVal += recv(fd, (void *) (readIntoBuffer + retVal), MAX_INPUT_MSG - retVal, 0);
			if (retVal < 0) {
				std::cerr << "ERROR: recv " << errno << "."<< std::endl;
				return -1;
			}
		} while(errno == 11 or retVal != MAX_INPUT_MSG);
		return 0;
	}



	/**
	 * Read input from std::cin.
	 * @return EXIT if exit was typed, 0 otherwise.
	 */
	int receiveFromUser()
	{
		char buf[MAX_INPUT_MSG] = {0};
		std::string input(buf);

		fd_set copySet(cinFdset);
		if (select(1, &copySet, NULL, NULL, &tv) > 0)
		{
			getline(std::cin, input);
			if (input == "exit" or input == "EXIT")
			{
				for (auto it = nameToSocketFd->begin(); it != nameToSocketFd->end(); ++it)
				{
					if (it->second != mainSocketFd)
					{
					if (sendMsgToClient(serverTerminating, it->second, true) < 0) { return -1;};
					}
				}
				std::cout << "EXIT: command is typed: server is shutting down" << std::endl;
				return EXIT;
			} else
			{
				std::cout << "ERROR: invalid input." << std::endl;
			}
		}
		return 0;
	}


	/**
	 * Read new content in sockets from clients.
	 */
	int readFromSockets(fd_set* select_set)
	{
		// check if there is an available socket waiting
		for (auto it = nameToSocketFd->begin(); it != nameToSocketFd->end();)
		{
			int fd = (it++)->second;
			if (FD_ISSET(fd, select_set))
			{
				if (receiveFromSocket(fd) < 0)
				{
					return -1;
				}
				if (handleMessageReceived(fd) < 0) {
					return -1;
				}
				// read from std::cin after every operation
				if (receiveFromUser() == EXIT) { return 0;}
			}
		}
		return 0;
	}


	/**
	 * Message from socket was read into readIntoBuffer.
	 * Handle message logic.
	 */
	int handleMessageReceived(int calleeFd)
	{
		int command = atoi(&(readIntoBuffer[0]));

		switch (command)
		{
			case createGroupCmd:
				return executeGroup(calleeFd);
			case sendCmd:
				return executeSend(calleeFd, (readIntoBuffer + 2));
			case whoCmd:
				return executeWho(calleeFd);
			case exitCmd:
				return executeExit(calleeFd);
			default:
				if (sendMsgToClient(invalidMsg, calleeFd) < 0) { return -1;}
		}
		return 0;
	}

	/**
	 * Create group requested in message that is in readIntoBuffer.
	 * message is of format <group_name> <member1,member2,...>
	 */
	int executeGroup(int calleeFd)
	{
		// extract group name
		std::string groupName = "";
		int i = 2;
		while (readIntoBuffer[i] != ' ')
		{
			groupName += readIntoBuffer[i];
			++i;
		}
		++i;

		// check group name is not taken
		if (nameToSocketFd->find(groupName.c_str()) != nameToSocketFd->end()
			or groups->find(groupName.c_str()) != groups->end())
		{
			std::string failMsg = groupCreatedFailure + groupName + "\".";
			sendMsgToClient(failMsg.c_str(), calleeFd);
			failMsg = getFdName(calleeFd) + ": " + failMsg;
			std::cout << failMsg << std::endl;
			return 0;
		}

		// get group members
		std::vector<int> *groupMembers = new std::vector<int>();
		std::string members = "";
		int num_of_members = 0;
		// check if any member names are invalid
		while (readIntoBuffer[i] != '\n')
		{
			std::string name = "";
			while (readIntoBuffer[i] != ',' and readIntoBuffer[i] != '\n')
			{
				name += readIntoBuffer[i];
				++i;
			}
			++i;
			if (nameToSocketFd->find(name.c_str()) == nameToSocketFd->end())
			{
				std::string failMsg = groupCreatedFailure + groupName + "\".";
				sendMsgToClient(failMsg.c_str(), calleeFd);
				failMsg = getFdName(calleeFd) + ": " + failMsg;
				std::cout << failMsg << std::endl;
				return 0;
			}
			groupMembers->push_back((*nameToSocketFd)[name]);
			++num_of_members;
		}
		if (num_of_members == 1 and std::find(groupMembers->begin(), groupMembers->end(),
											  calleeFd) != groupMembers->end())
		{
			std::string failMsg = groupCreatedFailure + groupName + "\".";
			sendMsgToClient(failMsg.c_str(), calleeFd);
			failMsg = getFdName(calleeFd) + ": " + failMsg;
			std::cout << failMsg << std::endl;
			return 0;
		}
		// add caller to group member (if not already there)
		std::string calleeName = getFdName(calleeFd);
		if (std::find(groupMembers->begin(), groupMembers->end(), calleeFd) == groupMembers->end())
		{
			groupMembers->push_back(calleeFd);
		}

		groups->insert(std::make_pair(groupName, groupMembers));

		std::string successMsg = "Group \"" + groupName + "\"" + groupCreatedSuccess;
		sendMsgToClient(successMsg.c_str(), calleeFd);
		successMsg = getFdName(calleeFd) + ": " + successMsg;
		successMsg.pop_back();
		std::cout << successMsg << std::endl;
		return 0;
	}

	/**
	 * Assumes fd exists!
	 */
	std::string getFdName(int fd)
	{
		for (auto it = nameToSocketFd->begin(); it != nameToSocketFd->end(); ++it)
		{
			if (it->second == fd)
			{
				return it->first;
			}
		}
		std::cerr << "gedFdName: fd does not exist." << std::endl;
		exit(1);
	}


	/**
	 * Send the calleeFd an alphabetically sorted list of connected client names.
	 * @return -1 on failure, otherwise 0.
	 */
	int executeWho(int calleeFd)
	{
		std::string names = "";
		for (auto it = nameToSocketFd->begin(); it != nameToSocketFd->end(); ++it)
		{
			if (it->first != myName)
			{
				names += it->first;
				names += ",";
			}
		}
		names.pop_back();
		names += "\n";
		if (sendMsgToClient(names.c_str(), calleeFd) < 0) { return -1;}
		std::cout << getFdName(calleeFd) << ": Requests the currently connected client names." << std::endl;
		return 0;
	}

	/**
	 * Unregisters the calleeFd from the server and removes it from all groups.
	 */
	int executeExit(int calleeFd)
	{
		std::string calleeName = getFdName(calleeFd);
		auto clientPairPtr = nameToSocketFd->find(calleeName);
		std::string output(exitMsg);
		output.pop_back();
		std::cout << clientPairPtr->first << ": " << output << std::endl;
		nameToSocketFd->erase(clientPairPtr);

		for (auto it2 = groups->begin(); it2 != groups->end(); ++it2)
		{
			auto foundFd = std::find(it2->second->begin(), it2->second->end(), calleeFd);
			if (foundFd != it2->second->end())
			{
				it2->second->erase(foundFd);
				break;
			}
		}
		if (sendMsgToClient(exitMsg, calleeFd) < 0) { return -1;}
		FD_CLR(calleeFd, &socketFds);
		return 0;

	}


	/**
	 * Send message to group or client.
	 * @param msg: <name> <content>
	 */
	int executeSend(int calleeFd, const char* msg)
	{
		std::vector<int> recipients;
		int successFlag = 0;

		// extract callee socket name
		std::string calleeName = "";
		for (auto it = nameToSocketFd->begin(); it != nameToSocketFd->end(); ++it)
		{
			if (it->second == calleeFd)
			{
				calleeName = it->first;
				break;
			}
		}

		// extract receiver name
		std::string receiverName = "";
		int i = 0;
		while (msg[i] != ' ')
		{
			receiverName += msg[i];
			++i;
		}

		// extract message
		std::string message = "";
		while (msg[i] != '\n')
		{
			++i;
			message += msg[i];
		}
		std::string finalMsg = calleeName + ": " + message;
		message.pop_back();
		// if receiver is client name, send message
		auto it = nameToSocketFd->find(receiverName);
		if (it != nameToSocketFd->end())
		{
			successFlag = 1;
			recipients.push_back(it->second);
		}
		else
			// send to all group members
		{
			for (auto it = groups->begin(); it != groups->end(); ++it) {
				if ((it->first).compare(receiverName) == 0) {

					// client cannot send to group he is not part of
					if (std::find(it->second->begin(), it->second->end(), calleeFd) ==
						it->second->end()) {
						successFlag = 0;
						break;
					}
					successFlag = 1;
					// send to all socket fds in group
					for (auto it2 = it->second->begin(); it2 != it->second->end(); ++it2) {
						if (*it2 != calleeFd) {
							recipients.push_back(*it2);
						}
					}
				}
			}
		}
		if (successFlag == 0)
		{
			std::cout << calleeName.c_str() << ": ERROR: failed to send \"" << message << "\" "
					"to " << receiverName << "." << std::endl;
			if (sendMsgToClient(sendFail, calleeFd, false) < 0) { return -1; }
		} else
		{
			for (auto it = recipients.begin(); it != recipients.end(); ++it)
			{
				if (sendMsgToClient(finalMsg.c_str(), *it, true) < 0) { return -1;}
			}
			if (sendMsgToClient(successfulSent, calleeFd) < 0) { return -1;}
			std::cout << calleeName.c_str() << ": \"" << message << "\" was sent successfully to "
					  << receiverName << "." << std::endl;
		}

		return 0;
	}



	/**
	 * Send message with command type to client. Receive confirmation from client. If message was
	 * not received or error occurred.
	 */
	int sendMsgToClient(const char* msg, int clientFd, bool getConfirmation=false)
	{
		unsigned long length = strlen(msg);
		memcpy(sendBuffer, msg, length);
		sendBuffer[length - 1] = '\n';

		int retVal = 0;
		do {
			errno = 0;
			retVal += send(clientFd, sendBuffer + retVal, MAX_INPUT_MSG - retVal, 0);
			if (retVal == MAX_INPUT_MSG) {
				break;
			}
			if (retVal < 0)
			{
				std::cout << "ERROR: send " << errno << "."<< std::endl;
				std::fill(sendBuffer, sendBuffer + MAX_INPUT_MSG, '\0');
				return -1;
			}
		} while (errno == 11 or retVal != MAX_INPUT_MSG);

		// reinitialize sendBuffer, and receive message confirmation from client if requested
		std::fill(sendBuffer, sendBuffer + MAX_INPUT_MSG, '\0');
		if (getConfirmation and receiveFromSocket(clientFd) < 0) { return -1;}
		return 0;
	}




	/**
	 * Get new client connection. Check if client name already exists and accept or reject
	 * accordingly.
	 * @return 0 on success, -1 otherwise.
	 */
	int handleNewSocket()
	{
		unsigned int socketSize = sizeof(whatsAppSocket);
		int newFd = accept(mainSocketFd, (struct sockaddr*)&address, &socketSize);
		while(newFd != -1)
		{
			// get client name from connection
			if (receiveFromSocket(newFd) < 0)
			{
				return -1;
			}

			std::string *name = new std::string(readIntoBuffer);
			name->pop_back();

			// do not allow more than 30 connected clients
			if (nameToSocketFd->size() >= 30)
			{
				std::cout << *name << " failed to connect." << std::endl;
				if (sendMsgToClient(takenName, newFd) < 0) { return -1;}
			}

			else if (nameToSocketFd->find(*name) == nameToSocketFd->end()) {
				// client name is new, register new connection
				nameToSocketFd->insert(std::make_pair(*name, newFd));
				FD_SET(newFd, &socketFds);
				std::cout << *name << " connected." << std::endl;
				if (sendMsgToClient(successfulConnection, newFd) < 0) {
					FD_CLR(newFd, &socketFds);
					nameToSocketFd->erase(*name);
					return -1;}
			} else
			{
				// client name already exists
				std::cout << *name << " failed to connect." << std::endl;
				if (sendMsgToClient(takenName, newFd) < 0) { return -1;}
			}
			delete name;
			newFd = accept(mainSocketFd, (struct sockaddr*)&address, &socketSize);
		}
//		FD_CLR(mainSocketFd, socketFds);
		return 0;
	}


	/**
	 * Create main socket attribute it with host name and port number.
	 * @return 0 upon success, -1 upon failure.
	 */
	int establish()
	{
		memset(mainSocketAddr, 0 , sizeof(*mainSocketAddr));
		mainSocketAddr->sin_family = AF_INET;
		mainSocketAddr->sin_addr.s_addr = INADDR_ANY;
		mainSocketAddr->sin_port = htons(portNum);
		return 0;
	}

public:


	/**
	 * Get server up and running. Bind it
	 * @return
	 */
	int runServer(){
		establish();
		if((mainSocketFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0){
			std::cout << "ERROR: socket " << errno << "."<< std::endl;
			return -1;
		}
		FD_SET(mainSocketFd, &socketFds);
		fd_set selectSet;

		if(bind(mainSocketFd,(struct sockaddr*) mainSocketAddr, sizeof(*mainSocketAddr)) < 0 ){
			close(mainSocketFd);
			std::cout << "ERROR: bind " << errno << "."<< std::endl;
			return -1;
		}
		listen(mainSocketFd, MAX_WAITING_SOCKETS);

		// wait up to n seconds for communication at select()
		struct timeval tv;
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		 (*nameToSocketFd)[myName] = mainSocketFd;

		while(true)
		{
			// get new client connection
			auto highest_fd = std::max_element(nameToSocketFd->begin(),nameToSocketFd->end(),
			[](const std::pair<std::string,int> & p1,const std::pair<std::string,int> & p2){
				return p1.second < p2.second;
			});
			selectSet = socketFds;
			int selected = select(highest_fd->second +1, &selectSet, NULL, NULL, &tv);

			if (selected > 0)
			{
				if (FD_ISSET(mainSocketFd, &selectSet))
				{
					if (handleNewSocket() < 0) { return -1;}
					// read from std::cin after every operation
					if (receiveFromUser() == EXIT) { return 0;}
					if (selected > 1)
					{
						if (readFromSockets(&selectSet) < 0) { return -1;}
					}
				}
				else if (readFromSockets(&selectSet) < 0)
				{
					// error occurred, cleanup before exit
					return -1;
				}
			} else
			{
				if (receiveFromUser() == EXIT) { return 0;}
			}
		}
	}


	/**
	 * Class WhatsAppServer constructor.
	 */
	WhatsAppServer(int portNum):portNum(portNum){

		mainSocketAddr = new whatsAppSocket();
		nameToSocketFd = new std::map<std::string, int>();
		groups = new std::map<std::string, std::vector<int>*>();
		FD_ZERO(&socketFds);
		readIntoBuffer = new char[MAX_INPUT_MSG];
		std::fill(sendBuffer, sendBuffer + MAX_INPUT_MSG, '\0');
		FD_SET(0, &cinFdset);
		tv.tv_sec = 2;
		tv.tv_usec = 0;
	};



	/**
	 * Class WhatsAppServer destructor.
	 */
	~WhatsAppServer(){

		for(auto it = groups->begin(); it != groups->end();++it)
		{
			delete it->second;
		}
		delete [] readIntoBuffer;
		delete nameToSocketFd;
		delete groups;
		delete mainSocketAddr;
		close(mainSocketFd);
	}

};



int main (int argc, char* argv[])
{
	if (argc != 2)
	{
		std::cerr << "Invalid arguments." << std::endl;
		return -1;
	}
	int serverPort = atoi(argv[1]);
	WhatsAppServer server(serverPort);
	int retVal = server.runServer();
	exit(int(-1 == retVal));
}
