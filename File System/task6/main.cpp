#include <arpa/inet.h>
#include <iostream>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

int main() {
  // 得到套接字描述符
  int sockfd;

  /* code */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  struct ifconf ifc;
  caddr_t buf;
  int len = 100;

  // 初始化ifconf结构
  ifc.ifc_len = 1024;
  if ((buf = (caddr_t)malloc(1024)) == NULL) {
    cout << "malloc error" << endl;
    exit(-1);
  }
  ifc.ifc_buf = buf;

  // 获取所有接口信息

  /* code */
  ioctl(sockfd, SIOCGIFCONF, &ifc);

  // 遍历每一个ifreq结构
  struct ifreq *ifr;
  struct ifreq ifrcopy;
  ifr = (struct ifreq *)buf;
  for (int i = (ifc.ifc_len / sizeof(struct ifreq)); i > 0; i--) {
    // 接口名
    cout << "interface name: " << ifr->ifr_name << endl;
    // ipv4地址
    cout << "inet addr: "
         << inet_ntoa(((struct sockaddr_in *)&(ifr->ifr_addr))->sin_addr)
         << endl;

    // 获取广播地址
    ifrcopy = *ifr;

    /* code */
    ioctl(sockfd, SIOCGIFBRDADDR, &ifc);

    cout << "broad addr: "
         << inet_ntoa(((struct sockaddr_in *)&(ifrcopy.ifr_addr))->sin_addr)
         << endl;
    // 获取mtu
    ifrcopy = *ifr;

    /* code */
    ioctl(sockfd, SIOCGIFMTU, &ifc);

    cout << "mtu: " << ifrcopy.ifr_mtu << endl;
    cout << endl;
    ifr++;
  }

  return 0;
}