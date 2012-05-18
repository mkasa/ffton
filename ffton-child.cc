// -*- mode:C++; c-basic-offset:2; tab-width:2 -*-
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <netdb.h>
#include <getopt.h>
#include <stackdump.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <udt.h>

using namespace std;

class UDTSocket {
  UDTSOCKET sock;
  sockaddr_in my_addr;
  const char* lastError;

public:
  static bool name2addr(const char* addressString, in_addr& inetAddr) {
    while(true) {
      const char* p = addressString;
      while(*p) {
        if(*p != '.' && (*p < '0' || '9' < *p)) break;
        ++p;
      }
      if(*p) break;
      inet_pton(AF_INET, addressString, &inetAddr);
      return true;
    }
    addrinfo hints;
    memset(&hints, 0, sizeof(addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* info;
    const int result = getaddrinfo(addressString, 0, &hints, &info);
    if(result != 0 || info == NULL) return false;
    for(addrinfo* i = info; i != NULL; ) {
      if(i->ai_family == AF_INET) {
        inetAddr = ((sockaddr_in*)i->ai_addr)->sin_addr;
        freeaddrinfo(info);
        return true;
      }
    }
    freeaddrinfo(info);
    return false;
  }
  inline UDTSocket() {
    sock = UDT::socket(AF_INET, SOCK_STREAM, 0);
    lastError = NULL;
  }
  inline UDTSocket(UDTSOCKET vsock) {
    sock = vsock;
    lastError = NULL;
  }
  inline UDTSocket(UDTSOCKET vsock, sockaddr_in peer) {
    sock = vsock;
    my_addr = peer;
    lastError = NULL;
  }
  inline ~UDTSocket() {
    close();
  }
  int setRendezvous() {
    bool rendezvous = true;
    return UDT::setsockopt(sock, 0, UDT_RENDEZVOUS, &rendezvous, sizeof(rendezvous));
  }
  int setLinger() {
    bool linger = true;
    return UDT::setsockopt(sock, 0, UDT_LINGER, &linger, sizeof(linger));
  }
  /// returns UDT::ERROR if error.
  int bind(int listenPortNumber) {
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(listenPortNumber);
    my_addr.sin_addr.s_addr = INADDR_ANY; // NOTE: if you have time, change this function to take an IP with default value INADDR_ANY, though I think we use only INADDR_ANY in most cases.
    memset(&(my_addr.sin_zero), 0, 8);
    return UDT::bind(sock, (sockaddr*)&my_addr, sizeof(my_addr));
  }
  inline int listen(int numQueue = 10) {
    return UDT::listen(sock, numQueue);
  }
  int connect(int listenPortNumber, const char* peerAddressName, int peerPort) {
    const int result1 = setRendezvous();
    if(result1 == UDT::ERROR) return result1;
    const int result2 = setLinger();
    if(result2 == UDT::ERROR) return result2;
    const int result3 = bind(listenPortNumber);
    if(result3 == UDT::ERROR) return result3;
    return connect(peerAddressName, peerPort);
  }
  int connect(const char* peerAddressName, int peerPort) {
    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(peerPort);
    const bool result = name2addr(peerAddressName, serv_addr.sin_addr);
    if(!result) {
      lastError = "Address translation failed";
      return UDT::ERROR;
    }
    memset(&(serv_addr.sin_zero), 0, 8);
    return UDT::connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr));
  }
  string getLastError() {
    return lastError ? lastError : UDT::getlasterror().getErrorMessage();
  }
  inline UDTSocket accept() {
    UDTSOCKET asock;
    sockaddr_in peer_addr;
    int namelen;
    asock = UDT::accept(sock, (sockaddr*)&peer_addr, &namelen);
    return UDTSocket(asock, peer_addr);
  }
  inline int recv(void* data, int bufSize, int options = 0) {
    return UDT::recv(sock, (char*)data, bufSize, options);
  }
  inline int recvWithRetry(void* data, int bufSize, int options = 0) {
    const int origBufSize = bufSize;
    while(0 < bufSize) {
      const size_t r = UDT::recv(sock, (char*)data, bufSize, options);
      if(r == UDT::ERROR) return r;
      data = (char*)data + r;
      bufSize -= r;
    }
    return origBufSize;
  }
  inline int send(const void* data, int dataSize, int options = 0) {
    return UDT::send(sock, (const char*)data, dataSize, options);
  }
  inline int sendWithRetry(const void* data, int dataSize, int options = 0) {
    const int origDataSize = dataSize;
    while(0 < dataSize) {
      const size_t r = UDT::send(sock, (const char*)data, dataSize, options);
      if(r == UDT::ERROR) return r;
      data = (char*)data + r;
      dataSize -= r;
    }
    return origDataSize;
  }
  inline int64_t sendfile(fstream& st, int64_t start, int64_t size) {
    return UDT::sendfile(sock, st, start, size);
  }
  inline int64_t recvfile(fstream& st, int64_t start, int64_t size) {
    return UDT::recvfile(sock, st, start, size);
  }
  inline void close() {
    UDT::close(sock);
  }
};

int PortNumberString2PortNumber(const char* portNumber)
{
	const int num = atoi(portNumber);
	if(num < 1024 || 65536 <= num) {
		fprintf(stderr, "Wrong port number. The port number must be within 1024 to 65535 (inclusive).\n");
		exit(4);
	}
	return num;
}

void sender(char* fname[], int numFiles, const char* peerName, const char* peerPortStr, const char* myPortStr)
{
  const int myPort = PortNumberString2PortNumber(myPortStr);
  const int port = PortNumberString2PortNumber(peerPortStr);
  UDTSocket s;
  if(s.connect(myPort, peerName, port) == UDT::ERROR) {
    fprintf(stderr, "ERROR: %s\n", s.getLastError().c_str());
    exit(5);
  }
  for(int fileIndex = 0; fileIndex < numFiles; ++fileIndex) {
    const char* fileName = fname[fileIndex];
    const int fd = open(fname[fileIndex], O_RDONLY | O_LARGEFILE);
    fprintf(stderr, "INFO: sending '%s'\n", fileName);
    if(fd == -1) {
      fprintf(stderr, "ERROR: Could not open '%s'\n", fileName);
      continue;
    }
    struct stat st;
    if(fstat(fd, &st) == -1) {
      fprintf(stderr, "ERROR: Could not obtain the file size of '%s'\n", fileName);
      continue;
    }
    const int64_t fileSize = st.st_size;
    if(s.send(&fileSize, sizeof(fileSize)) == UDT::ERROR) {
      fprintf(stderr, "ERROR: %s\n", s.getLastError().c_str());
      exit(6);
    }
    const size_t BUFFER_SIZE = 16 * 16 * 1024 * 1024;
    char* buffer = new char[BUFFER_SIZE];
    bool readAllBytesFromFile = false;
    int64_t sentSize = 0;
    int64_t prevCheckPoint = 0;
    while(!readAllBytesFromFile) {
      const size_t rb = read(fd, buffer, BUFFER_SIZE);
      if(rb == -1) {
        fprintf(stderr, "ERROR: Error while reading from '%s'\n", fileName);
        return;
      }
      if(0 < rb) {
        const size_t r = s.sendWithRetry(buffer, rb);
        if(r == UDT::ERROR) {
          fprintf(stderr, "ERROR: Network error while sending '%s'\n", fileName);
          return;
        }
        sentSize += rb;
        if(prevCheckPoint + 1024 * 1024 * 16 < sentSize) {
          prevCheckPoint = sentSize;
          fprintf(stderr, "INFO: %dMbytes sent                   \n", int(sentSize / 1024 / 1024));
        }
      } else {
        readAllBytesFromFile = true;
      }
    }
    close(fd);
    fprintf(stderr, "INFO: %llu bytes sent\n", sentSize);
    delete[] buffer;
  }
}

void receiver(char* fname[], int numFiles, const char* peerName, const char* peerPortStr, const char* myPortStr)
{
  const int myPort = PortNumberString2PortNumber(myPortStr);
	const int port = PortNumberString2PortNumber(peerPortStr);
	UDTSocket s;
	if(s.connect(myPort, peerName, port) == UDT::ERROR) {
		fprintf(stderr, "ERROR: %s\n", s.getLastError().c_str());
		exit(8);
  }
  for(int fileIndex = 0; fileIndex < numFiles; ++fileIndex) {
    const char* fileName = fname[fileIndex];
    const int fd = open(fileName, O_LARGEFILE | O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if(fd == -1) {
      fprintf(stderr, "ERROR: Could not open '%s'\n", fileName);
      return;
    }
    int64_t fileSize = 0;
    if(s.recv(&fileSize, sizeof(fileSize)) == UDT::ERROR) {
      fprintf(stderr, "ERROR: %s\n", s.getLastError().c_str());
      exit(6);
    }
    fprintf(stderr, "INFO: receiving %llu bytes\n", fileSize);
    const size_t BUFFER_SIZE = 16 * 16 * 1024 * 1024;
    char* buffer = new char[BUFFER_SIZE];
    int64_t receivedSize = 0;
    int64_t prevCheckPoint = 0;
    while(receivedSize < fileSize) {
      const size_t rb = s.recv(buffer, BUFFER_SIZE);
      if(rb == UDT::ERROR) {
        fprintf(stderr, "ERROR: Error while reading from socket.\n");
        return;
      }
      if(0 < rb) {
        size_t remainingBytes = rb;
        char* p = buffer;
        while(0 < remainingBytes) {
          const ssize_t f = write(fd, p, remainingBytes);
          if(f == -1) {
            fprintf(stderr, "ERROR: Error while writing to file '%s'\n", fileName);
            perror("ERROR ");
            return;
          }
          p += f;
          remainingBytes -= f;
        }
        receivedSize += rb;
        if(prevCheckPoint + 1024 * 1024 * 16 < receivedSize) {
          prevCheckPoint = receivedSize;
          fprintf(stderr, "INFO: %dMbytes received (%d%%)\r", int(receivedSize / 1024 / 1024), int(receivedSize * 100 / fileSize));
        }
      }
    }
    close(fd);
    delete[] buffer;
    fprintf(stderr, "INFO: received %llu bytes (100%%)\n", receivedSize);
  }
}

int main(int argc, char *argv[]) {
    GDB_On_SEGV gos(argv[0]);
	int c;
	bool isSender = false;
	bool isReceiver = false;
	while(1) {
		static struct option long_options[] = {
			{"send",    no_argument      ,         0, 's' /*equiv. short flag*/},
			{"recv",    no_argument      ,         0, 'r' /*equiv. short flag*/},
			{0, 0, 0, 0} // end of long options
		};
		int option_index = 0;
		c = getopt_long(argc, argv, "", long_options, &option_index);
		if(c == -1) break;
		switch(c) {
		case 0:
			// you can see long_options[option_index].name/flag and optarg (null if no argument).
			break;
		case 's':
			// you can see short options here. optarg is an argument.
			isSender = true;
			break;
		case 'r':
			isReceiver = true;
			break;
		}
	}
	if(argc < optind + 4 /* # of non-option arguments */) {
		fprintf(stderr, "usage: ffton [options] [--send | --recv] <peername> <peerport> <myport> <file(s)>\n");
		return 1;
	}
	if(isSender && isReceiver) {
		fprintf(stderr, "You can't specify --send and --recv at once!\n");
		return 2;
	}
	if(!isSender && !isReceiver) {
		fprintf(stderr, "Please specify --send or --recv\n");
		return 3;
	}
	const int numFiles = argc - optind - 3;
	if(isSender)   { sender(  &argv[optind + 3], numFiles, argv[optind], argv[optind + 1], argv[optind + 2]); }
	if(isReceiver) { receiver(&argv[optind + 3], numFiles, argv[optind], argv[optind + 1], argv[optind + 2]); }
}
