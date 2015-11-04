/*
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * tun.c - tun device functions
 */
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <linux/ethtool.h>
#include <sys/uio.h>

#include "config.h"
#include "clatd.h"

int rx_checksum_offloaded = 0;

/* function: tun_open
 * tries to open the tunnel device
 */
int tun_open() {
  int fd;

  fd = open("/dev/tun", O_RDWR);
  if(fd < 0) {
    fd = open("/dev/net/tun", O_RDWR);
  }

  return fd;
}

/* function: tun_alloc
 * creates a tun interface and names it
 * dev - the name for the new tun device
 */
int tun_alloc(char *dev, int fd) {
  struct ifreq ifr;
  int err;

  memset(&ifr, 0, sizeof(ifr));

  ifr.ifr_flags = IFF_TUN;
  if( *dev ) {
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ-1] = '\0';
  }

  if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ){
    close(fd);
    return err;
  }
  strcpy(dev, ifr.ifr_name);

  if (rx_checksum_offloaded) {
    ioctl(fd, TUNSETNOCSUM, 1);
  }

  return 0;
}

/* function: set_nonblocking
 * sets a filedescriptor to non-blocking mode
 * fd - the filedescriptor
 * returns: 0 on success, -1 on failure
 */
int set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL);
  if (flags == -1) {
    return flags;
  }
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* function: send_tun
 * sends a clat_packet to a tun interface
 * fd      - the tun filedescriptor
 * out     - the packet to send
 * iov_len - the number of entries in the clat_packet
 * returns: number of bytes read on success, -1 on failure
 */
int send_tun(int fd, clat_packet out, int iov_len) {
  return writev(fd, out, iov_len);
}

/* function: get_ethtool_feature_val
 * gets if a particular ethtool feature is enabled
 * dev     - the device name to query the feature on
 * cmd     - the feature to query
 * returns: 1 if feature is enabled, 0 if disabled
 */
int get_ethtool_feature_val(char *dev, int cmd) {
  int fd;
  struct ifreq ifr;
  struct ethtool_value eval;

  if (!dev){
    return 0;
  }

  if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
    return 0;
  }

  memset(&ifr, 0, sizeof(ifr));
  memset(&eval, 0, sizeof(eval));
  strlcpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name));
  eval.cmd = cmd;
  eval.data = 0;
  ifr.ifr_data = (caddr_t)&eval;
  if (ioctl(fd, SIOCETHTOOL, &ifr) == -1) {
      close(fd);
      return 0;
  }

  close(fd);

  if (!eval.data) {
    return 0;
  }

  return 1;
}

/* function: check_csum_offload
 * checks if GRO and RXCSUM are enabled on the device
 * dev     - the device name to query on
 * returns: 1 if checksum is offloaded, 0 if checksum needs
 *          to be validated in network stack.
 */
int check_csum_offload(char *dev) {
  if (!dev){
    return 0;
  }

  if(get_ethtool_feature_val(dev, ETHTOOL_GGRO) &&
     get_ethtool_feature_val(dev, ETHTOOL_GRXCSUM)) {
    return 1;
  }

  return 0;
}
