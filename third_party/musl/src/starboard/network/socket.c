// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#if SB_API_VERSION < 16

#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <stdio.h>

#include "starboard/file.h"
#include "starboard/socket.h"
#include "starboard/file.h"
#include "starboard/time.h"
#include "../pthread/pthread.h"

// Internal database function to convert SbSocket/SbFile object to
// integer, i.e. POSIX file descriptor.
typedef struct FileOrSocket {
  bool is_file;
  SbFile file;
  SbSocket socket;
} FileOrSocket;

typedef struct Node {
  int key;
  struct FileOrSocket* value;
  struct Node* next;
} Node;

typedef struct HashMap {
  int size;
  Node** array;
} HashMap;

static pthread_mutex_t lock;
static HashMap* map = NULL;
const int mapSize = 2048;
static int mapRecCount = 0;
const socklen_t kAddressLengthIpv4 = 4;
#if SB_HAS(IPV6)
const socklen_t kAddressLengthIpv6 = 16;
#endif
static int get(int key, bool take, FileOrSocket** valuePtr);

static void createHashMap(int size) {
  map = (HashMap*)malloc(sizeof(HashMap));
  memset(map, 0, sizeof(sizeof(HashMap)));
  map->size = size;
  map->array = (Node**)malloc(size * sizeof(Node*));
  memset(map->array, 0, size * sizeof(Node*));
}

static void destroyHashMap(void){
  if (map!= NULL){
    if (map->array!= NULL){
      free(map->array);
    }
    free(map);
  }
}

static int generateKey(void){
  static int key = 0;
  key++;
  if (key == 0x7FFFFFFF){
    key = 1;
  }
  return key;
}

static int hash(int key, int size) {
  return key % size;
}

static int put(FileOrSocket* value) {
  if (map == NULL) {
    pthread_mutex_init(&lock, 0);
    createHashMap(mapSize);
  }
  if (mapRecCount == mapSize){
    return -1;
  }

  // avoid collision in map
  FileOrSocket* valuePtr = NULL;
  int key = generateKey();
  while (get(key, false, &valuePtr) == 0){
    key = generateKey();
  }

  pthread_mutex_lock(&lock);

  int index = hash(key, map->size);
  Node* node = (Node*)malloc(sizeof(Node));
  node->key = key;
  node->value = value;
  node->next = map->array[index];
  map->array[index] = node;
  mapRecCount++;

  pthread_mutex_unlock(&lock);
  return key;
}

static int get(int key, bool take, FileOrSocket** valuePtr) {
  int index = 0;
  int status = 0;
  if (map == NULL) {
    return -1;
  }

  pthread_mutex_lock(&lock);

  index = hash(key, map->size);
  Node* node = map->array[index];
  if (node != NULL) {
    if (node->key == key) {
      *valuePtr = node->value;
    }
    if (take){
      free(map->array[index]);
      map->array[index] = NULL;
      mapRecCount--;
    }
    status = 0;
  } else {
    status = -1;
  }

  pthread_mutex_unlock(&lock);

  return status;
}

int TranslateSocketErrnoSbToPosix(SbSocketError sbError) {
  switch (sbError) {
    case kSbSocketOk:
      return 0;
    case kSbSocketPending:
      return EINPROGRESS;
    case kSbSocketErrorConnectionReset:
      return ECONNRESET;
    case kSbSocketErrorFailed:
    default:
      return -1;
  }
}

int ConvertSocketAddressPosixToSb(const struct sockaddr* address, SbSocketAddress* sbAddress){
  if (address == NULL){
    return -1;
  }
  struct sockaddr_in* addr_in = (struct sockaddr_in*)address;
  switch (addr_in->sin_family){
    case AF_INET:
      sbAddress->type = kSbSocketAddressTypeIpv4;
      memcpy(sbAddress->address, &addr_in->sin_addr, kAddressLengthIpv4);
      break;
#if SB_HAS(IPV6)
    case AF_INET6:
      sbAddress->type = kSbSocketAddressTypeIpv6;
      memcpy(sbAddress->address, &addr_in->sin_addr, kAddressLengthIpv6);
      break;
#endif
  }
  sbAddress->port = addr_in->sin_port;

  return 0;
}

int ConvertSocketAddressSbToPosix(const SbSocketAddress* sbAddress, struct sockaddr* address){
  if (sbAddress == NULL){
    return -1;
  }
  struct sockaddr_in* addr_in = (struct sockaddr_in*)address;
  switch (sbAddress->type){
    case kSbSocketAddressTypeIpv4:
      addr_in->sin_family = AF_INET;
      memcpy(&addr_in->sin_addr, sbAddress->address, kAddressLengthIpv4);
      break;
#if SB_HAS(IPV6)
    case kSbSocketAddressTypeIpv6:
      addr_in->sin_family = AF_INET6;
      memcpy(&addr_in->sin_addr, sbAddress->address, kAddressLengthIpv6);
      break;
#endif
    default:{}
  }
  addr_in->sin_port = sbAddress->port;

  return 0;
}

// The exported POSIX APIs
//
int open(const char* path, int oflag, ...) {
  bool out_created;
  SbFileError out_error;

  if (path == NULL){
    return -1;
  }

  FileOrSocket* value = (FileOrSocket*)malloc(sizeof(struct FileOrSocket));
  memset(value, 0, sizeof(struct FileOrSocket));
  value->is_file = true;

  // TODO: b/302715109 map posix flags to SB file flags
  int open_flags = 0;
  // O_APPEND, O_ASYNC, O_CLOEXEC, O_CREAT, O_DIRECT, O_DIRECTORY, O_DSYNC
  // O_EXCL, O_LARGEFILE, O_NOATIME, O_NOCTTY, O_NOFOLLOW,
  // O_NONBLOCK or O_NDELAY, O_PATH, O_SYNC, O_TMPFILE, O_TRUNC

  value->file = SbFileOpen(path, open_flags, &out_created, &out_error);
  if (!SbFileIsValid(value->file)){
    free(value);
    return -1;
  }

  int result = put(value);
  if (result <= 0){
    SbFileClose(value->file);
    free(value);
  }
  return result;
}

int socket(int domain, int type, int protocol){
  int address_type, socket_protocol;
  switch (domain){
    case AF_INET:
        address_type = kSbSocketAddressTypeIpv4;
        break;
    case AF_INET6:
        address_type = kSbSocketAddressTypeIpv6;
        break;
    default:
        return -1;
  }
  switch (protocol){
    case IPPROTO_TCP:
        socket_protocol = kSbSocketProtocolTcp;
        break;
    case IPPROTO_UDP:
        socket_protocol = kSbSocketProtocolUdp;
        break;
    default:
        return -1;
  }

  FileOrSocket* value = (FileOrSocket*)malloc(sizeof(struct FileOrSocket));
  memset(value, 0, sizeof(struct FileOrSocket));
  value->is_file = false;
  value->socket = SbSocketCreate(address_type, socket_protocol);
  if (!SbSocketIsValid(value->socket)){
    free(value);
    return -1;
  }

  int result = put(value);
  if (result <= 0){
    SbSocketDestroy(value->socket);
    free(value);
  }
  return result;
}

int close(int fd){
  if (fd <= 0) {
    return -1;
  }
  FileOrSocket* valueptr = NULL;
  if (get(fd, true, &valueptr) != 0) {
    return -1;
  }
  if (valueptr != NULL) {
    bool result = false;
    if (valueptr->is_file == false){
      result = SbSocketDestroy(valueptr->socket);
    } else {
      result = SbFileClose(valueptr->file);
    }
    if (!result){
      return -1;
    }
    return 0;
  }
  return -1;
}

int bind(int socket, const struct sockaddr* address, socklen_t address_len) {
  if (address == NULL || socket <= 0){
    return -1;
  }
  FileOrSocket *fileOrSock = NULL;
  if (get(socket, false, &fileOrSock) != 0){
    errno = EBADF;
    return -1;
  }
  if (fileOrSock == NULL || fileOrSock->is_file == true) {
    errno = EBADF;
    return -1;
  }

  SbSocketAddress local_address = {0};
  ConvertSocketAddressPosixToSb(address, &local_address);

  SbSocketError sbError = SbSocketBind(fileOrSock->socket, &local_address);
  errno = TranslateSocketErrnoSbToPosix(sbError);

  if (sbError == kSbSocketOk) {
    return 0;
  }
  return -1;
}

int listen(int socket, int backlog) {
  if (socket <= 0){
    return -1;
  }
  FileOrSocket *fileOrSock = NULL;
  if (get(socket, false, &fileOrSock) != 0){
    errno = EBADF;
    return -1;
  }
  if (fileOrSock == NULL || fileOrSock->is_file == true) {
    errno = EBADF;
    return -1;
  }

  SbSocketError sbError = SbSocketListen(fileOrSock->socket);
  errno = TranslateSocketErrnoSbToPosix(sbError);

  if (sbError == kSbSocketOk) {
    return 0;
  }
  return -1;
}

int accept(int socket, struct sockaddr* addr, socklen_t* addrlen) {
  if (socket <= 0){
    return -1;
  }
  FileOrSocket *fileOrSock = NULL;
  if (get(socket, false, &fileOrSock) != 0){
    errno = EBADF;
    return -1;
  }
  if (fileOrSock == NULL || fileOrSock->is_file == true) {
    errno = EBADF;
    return -1;
  }

  FileOrSocket* value = (FileOrSocket*)malloc(sizeof(struct FileOrSocket));
  memset(value, 0, sizeof(struct FileOrSocket));
  value->is_file = false;
  value->socket = SbSocketAccept(fileOrSock->socket);
  if (!SbSocketIsValid(value->socket)){
    free(value);
    return -1;
  }

  return put(value);
}

int connect(int socket, const struct sockaddr* name, socklen_t namelen) {
  if (socket <= 0 || name == NULL){
    return -1;
  }
  FileOrSocket *fileOrSock = NULL;
  if (get(socket, false, &fileOrSock) != 0){
    errno = EBADF;
    return -1;
  }
  if (fileOrSock == NULL || fileOrSock->is_file == true) {
    errno = EBADF;
    return -1;
  }

  SbSocketAddress local_address = {0};
  ConvertSocketAddressPosixToSb(name, &local_address);

  SbSocketError sbError = SbSocketConnect(fileOrSock->socket, &local_address);
  errno = TranslateSocketErrnoSbToPosix(sbError);

  if (sbError == kSbSocketOk || sbError == kSbSocketPending) {
    return 0;
  }
  return -1;
}

ssize_t send(int sockfd, const void* buf, size_t len, int flags) {
  if (sockfd <= 0){
    return -1;
  }
  FileOrSocket *fileOrSock = NULL;
  if (get(sockfd, false, &fileOrSock) != 0){
    errno = EBADF;
    return -1;
  }
  if (fileOrSock == NULL || fileOrSock->is_file == true) {
    errno = EBADF;
    return -1;
  }

  return SbSocketSendTo(fileOrSock->socket, buf, len, NULL);
}

ssize_t sendto(int sockfd, const void* buf, size_t len, int flags,
              const struct sockaddr* dest_addr,
              socklen_t dest_len) {
  if (sockfd <= 0){
    return -1;
  }
  FileOrSocket *fileOrSock = NULL;
  if (get(sockfd, false, &fileOrSock) != 0){
    errno = EBADF;
    return -1;
  }
  if (fileOrSock == NULL || fileOrSock->is_file == true) {
    errno = EBADF;
    return -1;
  }

  SbSocketAddress local_address = {0};
  ConvertSocketAddressPosixToSb(dest_addr, &local_address);

  return SbSocketSendTo(fileOrSock->socket, buf, len, dest_addr == NULL? NULL: &local_address);
}

ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
  if (sockfd <= 0){
    return -1;
  }
  FileOrSocket *fileOrSock = NULL;
  if (get(sockfd, false, &fileOrSock) != 0){
    errno = EBADF;
    return -1;
  }
  if (fileOrSock == NULL || fileOrSock->is_file == true) {
    errno = EBADF;
    return -1;
  }

  return SbSocketReceiveFrom(fileOrSock->socket, buf, len, NULL);
}

ssize_t recvfrom(int sockfd,
                void* buf,
                size_t len,
                int flags,
                struct sockaddr* address,
                socklen_t* address_len) {
  if (sockfd <= 0){
    return -1;
  }
  FileOrSocket *fileOrSock = NULL;
  if (get(sockfd, false, &fileOrSock) != 0){
    errno = EBADF;
    return -1;
  }
  if (fileOrSock == NULL || fileOrSock->is_file == true) {
    errno = EBADF;
    return -1;
  }

  SbSocketAddress local_address = {0};
  ConvertSocketAddressPosixToSb(address, &local_address);

  return SbSocketReceiveFrom(fileOrSock->socket, buf, len, address == NULL? NULL: &local_address);
}

int getsockname(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen){
  if (sockfd <= 0){
    return -1;
  }
  FileOrSocket *fileOrSock = NULL;
  if (get(sockfd, false, &fileOrSock) != 0){
    errno = EBADF;
    return -1;
  }
  if (fileOrSock == NULL || fileOrSock->is_file == true) {
    errno = EBADF;
    return -1;
  }
  SbSocketAddress out_address = {0};
  int result = SbSocketGetLocalAddress(fileOrSock->socket, &out_address)? 0: -1;
  return result;
}

int setsockopt (int sockfd, int level, int optname, const void* optval,
                      socklen_t optlen){
  if (sockfd <= 0){
    return -1;
  }
  FileOrSocket *fileOrSock = NULL;
  if (get(sockfd, false, &fileOrSock) != 0){
    errno = EBADF;
    return -1;
  }
  if (fileOrSock == NULL || fileOrSock->is_file == true) {
    errno = EBADF;
    return -1;
  }

  if (level == SOL_SOCKET || level == SOL_TCP || level == IPPROTO_TCP) {

    int* operation = (int*)optval;
    switch (optname){
      case SO_BROADCAST:{
        bool bool_value = (*operation == 1)? true:false;
        return SbSocketSetBroadcast(fileOrSock->socket, bool_value) == true? 0:-1;
      }
      case SO_REUSEADDR:{
        bool bool_value = *operation == 1? true:false;
        return SbSocketSetReuseAddress(fileOrSock->socket, bool_value) == true? 0:-1;
      }
      case SO_RCVBUF:{
        return SbSocketSetReuseAddress(fileOrSock->socket, *operation) == true? 0:-1;
      }
      case SO_SNDBUF:{
        return SbSocketSetSendBufferSize(fileOrSock->socket, *operation) == true? 0:-1;
      }
      case SO_KEEPALIVE:{
        bool bool_value = *operation == 1? true:false;
        if (bool_value == false){
          return SbSocketSetTcpKeepAlive(fileOrSock->socket, false, 0) == true? 0:-1;
        }
      }
      case TCP_KEEPIDLE:{
        /* function SbSocketSetTcpKeepAlive() also calls setsockopt() with operation code
           TCP_KEEPINTVL. Therefore there is not need to take care of case TCP_KEEPINTVL
           separately.*/
        if (*operation > 0){
          SbTime period_microsecond = *operation;
          period_microsecond *= 1000000;
          return SbSocketSetTcpKeepAlive(fileOrSock->socket, true, period_microsecond) == true? 0:-1;
        }
        break;
      }
      case TCP_KEEPINTVL:{
        /* function SbSocketSetTcpKeepAlive() also calls setsockopt() with operation code
           TCP_KEEPINTVL. Therefore there is not need to take care of case TCP_KEEPINTVL
           separately when TCP_KEEPIDLE is set.*/
        break;
        return 0;
      }
      case TCP_NODELAY: {
        int* operation = (int*)optval;
        bool bool_value = *operation == 1? true:false;
        return SbSocketSetTcpNoDelay(fileOrSock->socket, bool_value) == true? 0:-1;
      }
      case IP_ADD_MEMBERSHIP: {
        SbSocketAddress* addr = (SbSocketAddress*)optval;
        return SbSocketJoinMulticastGroup(fileOrSock->socket, addr) == true? 0:-1;
      }
      default:
        return -1;
    }
  } else {
      return -1;
  }

  return 0;
}

int fcntl(int fd, int cmd, ... /*arg*/) {
  if (fd <= 0){
    return -1;
  }
  /* This function was used to set socket non-blocking,
   * however since SbSocketCreate() sets the socket to
   * non-blocking by default, we don't need to set it again.*/
  return 0;
}

void freeaddrinfo(struct addrinfo *ai)
{
  struct addrinfo* ptr = ai;
  while (ai != NULL){
    if (ai->ai_addr != NULL){
      free(ai->ai_addr);
    }
    ai = ai->ai_next;
    free(ptr);
    ptr = ai;
  }
}

int getaddrinfo(const char* node, const char* service, const struct addrinfo* hints, struct addrinfo** res)
{
  int filters = 0;
  if (hints != NULL){
    if (hints->ai_family == AF_INET) {
      filters &= kSbSocketResolveFilterIpv4;
    }
    if (hints->ai_family == AF_INET6) {
      filters &= kSbSocketResolveFilterIpv6;
    }
  }
  SbSocketResolution* sbSockResolve = SbSocketResolve(node, filters);
  if (sbSockResolve == NULL){
    return -1;
  }

  struct addrinfo* ai = (struct addrinfo*)malloc(sizeof(struct addrinfo));
  memset(ai, 0, sizeof(struct addrinfo));
  *res = ai;

  for(int i = 0; i < sbSockResolve->address_count; i++){
    ai->ai_addr = (struct sockaddr*)malloc(sizeof(struct sockaddr));
    memset(ai->ai_addr, 0, sizeof(struct sockaddr));
    ConvertSocketAddressSbToPosix( &sbSockResolve->addresses[i], ai->ai_addr);
    ai->ai_addrlen = sizeof(struct sockaddr);

    if (sbSockResolve->addresses[i].type == kSbSocketAddressTypeIpv4) {
      ai->ai_family = AF_INET;
    }
#if SB_HAS(IPV6)
    if (sbSockResolve->addresses[i].type == kSbSocketAddressTypeIpv6) {
      ai->ai_family = AF_INET6;
    }
#endif
    if (i < sbSockResolve->address_count - 1){
      ai->ai_next = (struct addrinfo*)malloc(sizeof(struct addrinfo));
      memset(ai->ai_next, 0, sizeof(struct addrinfo));
      ai = ai->ai_next;
    }
  }

  SbSocketFreeResolution(sbSockResolve);
  return 0;
}

void freeifaddrs(struct ifaddrs* ifa){
  struct ifaddrs* ptr = ifa;
  while (ifa != NULL){
    if (ifa->ifa_addr != NULL){
      free(ifa->ifa_addr);
    }
    ifa = ifa->ifa_next;
    free(ptr);
    ptr = ifa;
  }
}

int getifaddrs(struct ifaddrs** ifap) {

  SbSocketAddress sbAddress = {0};
  if (!SbSocketGetInterfaceAddress(NULL, &sbAddress, NULL)){
    return -1;
  }

  *ifap = (struct ifaddrs*)malloc(sizeof(struct ifaddrs));
  memset(*ifap, 0, sizeof(struct ifaddrs));
  struct ifaddrs* ifa = *ifap;
  ifa->ifa_addr = (struct sockaddr*)malloc(sizeof(struct sockaddr));
  memset(ifa->ifa_addr, 0, sizeof(struct sockaddr));
  ConvertSocketAddressSbToPosix(&sbAddress, ifa->ifa_addr);
  return 0;
}

#endif  // SB_API_VERSION < 16
