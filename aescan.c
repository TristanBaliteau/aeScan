#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_HOSTS     256
#define MAX_PORTS     65535
#define MAX_OPEN_PORTS 128

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_RED     "\033[31m"

typedef struct {
  char ip[16];
  char hostname[256];
} Host;

typedef struct {
  struct in_addr network;
  int index;
} ThreadArg;

typedef struct {
  char ip[16];
  int port;
} PortScanArg;

typedef struct {
  int ports[MAX_OPEN_PORTS];
  int count;
  pthread_mutex_t mutex;
} OpenPorts;

static Host hosts[MAX_HOSTS];
static int host_count = 0;
static pthread_mutex_t host_mutex = PTHREAD_MUTEX_INITIALIZER;
static OpenPorts open_ports = { .count = 0, .mutex = PTHREAD_MUTEX_INITIALIZER };

static int ping_ip(const char *ip) {
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "ping -c 1 -W 1 %s > /dev/null 2>&1", ip);
  return system(cmd) == 0;
}

static void resolve_hostname(const char *ip, char *hostname, size_t len) {
  struct in_addr addr;
  if (inet_aton(ip, &addr)) {
    struct hostent *host = gethostbyaddr(&addr, sizeof(addr), AF_INET);
    snprintf(hostname, len, "%s", (host && host->h_name) ? host->h_name : "N/A");
  } else {
    snprintf(hostname, len, "N/A");
  }
}

static int get_local_network(struct in_addr *ip, struct in_addr *mask) {
  struct ifaddrs *ifaddr = NULL;
  if (getifaddrs(&ifaddr) == -1) return 0;

  for (struct ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
    if (ifa->ifa_flags & IFF_LOOPBACK) continue;

    *ip   = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
    *mask = ((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr;
    freeifaddrs(ifaddr);
    return 1;
  }

  freeifaddrs(ifaddr);
  return 0;
}

static void *scan_host(void *arg) {
  ThreadArg *targ = arg;
  struct in_addr addr = { .s_addr = htonl(ntohl(targ->network.s_addr) + targ->index) };
  char ip[INET_ADDRSTRLEN];

  snprintf(ip, sizeof(ip), "%s", inet_ntoa(addr));
  if (ping_ip(ip)) {
    pthread_mutex_lock(&host_mutex);
    if (host_count < MAX_HOSTS) {
      snprintf(hosts[host_count].ip, sizeof(hosts[host_count].ip), "%s", ip);
      resolve_hostname(ip, hosts[host_count].hostname, sizeof(hosts[host_count].hostname));
      printf(COLOR_GREEN "[%d] %-15s" COLOR_RESET " - " COLOR_YELLOW "%s\n" COLOR_RESET,
             host_count + 1, hosts[host_count].ip, hosts[host_count].hostname);
      host_count++;
    }
    pthread_mutex_unlock(&host_mutex);
  }

  free(targ);
  return NULL;
}

static void *scan_port(void *arg) {
  PortScanArg *psa = arg;
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) { free(psa); return NULL; }

  struct sockaddr_in target = {
    .sin_family = AF_INET,
    .sin_port = htons(psa->port)
  };
  inet_pton(AF_INET, psa->ip, &target.sin_addr);

  struct timeval timeout = { .tv_sec = 1 };
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  if (connect(sock, (struct sockaddr *)&target, sizeof(target)) == 0) {
    pthread_mutex_lock(&open_ports.mutex);
    if (open_ports.count < MAX_OPEN_PORTS) {
      open_ports.ports[open_ports.count++] = psa->port;
    }
    pthread_mutex_unlock(&open_ports.mutex);
  }

  close(sock);
  free(psa);
  return NULL;
}

static void scan_ports(const char *ip) {
  pthread_t threads[MAX_PORTS];
  printf(COLOR_CYAN "\n[ Scanning %s ]\n" COLOR_RESET, ip);

  for (int i = 1; i <= MAX_PORTS; i++) {
    PortScanArg *arg = malloc(sizeof(PortScanArg));
    if (!arg) continue;
    snprintf(arg->ip, sizeof(arg->ip), "%s", ip);
    arg->port = i;
    pthread_create(&threads[i - 1], NULL, scan_port, arg);
  }

  for (int i = 0; i < MAX_PORTS; i++) {
    pthread_join(threads[i], NULL);
  }

  if (open_ports.count == 0) {
    printf(COLOR_RED "> No open ports found on " COLOR_YELLOW "%s\n" COLOR_RESET, ip);
  } else {
    for (int i = 0; i < open_ports.count; i++) {
      printf(COLOR_GREEN "> Port " COLOR_YELLOW "%d" COLOR_GREEN " is OPEN\n" COLOR_RESET,
             open_ports.ports[i]);
    }
  }
}

int main(void) {
  struct in_addr ip, mask, network;
  pthread_t threads[254];

  printf(COLOR_CYAN "\n╔════════════════════════════════════════════╗\n");
  printf(           "║                ⟦ aeScan ⟧                  ║\n");
  printf(           "║         adaptive network scanner           ║\n");
  printf(           "╚════════════════════════════════════════════╝\n" COLOR_RESET);

  if (!get_local_network(&ip, &mask)) {
    fprintf(stderr, COLOR_RED "✖ Error: Failed to detect local network.\n" COLOR_RESET);
    return 1;
  }

  network.s_addr = ip.s_addr & mask.s_addr;

  printf(COLOR_CYAN "\n[ Local Network Information ]\n" COLOR_RESET);
  printf("> IP Address   : " COLOR_YELLOW "%s\n" COLOR_RESET, inet_ntoa(ip));
  printf("> Subnet Mask  : " COLOR_YELLOW "%s\n" COLOR_RESET, inet_ntoa(mask));
  printf("> Network CIDR : " COLOR_YELLOW "%s/%d\n" COLOR_RESET,
         inet_ntoa(network), __builtin_popcount(ntohl(mask.s_addr)));

  printf(COLOR_CYAN "\n[ Scanning Network for Hosts ]\n" COLOR_RESET);
  for (int i = 1; i < 255; i++) {
    ThreadArg *arg = malloc(sizeof(ThreadArg));
    if (!arg) continue;
    arg->network = network;
    arg->index = i;
    pthread_create(&threads[i - 1], NULL, scan_host, arg);
  }

  for (int i = 0; i < 254; i++) {
    pthread_join(threads[i], NULL);
  }

  if (host_count == 0) {
    printf(COLOR_RED "\n✖ No hosts detected on the local network.\n\n" COLOR_RESET);
    return 1;
  }

  printf(COLOR_CYAN "\n[ Select Host to Scan Ports ]\n" COLOR_RESET);
  printf("> Enter host number (1-%d): ", host_count);
  fflush(stdout);

  int choice;
  if (scanf("%d", &choice) != 1 || choice < 1 || choice > host_count) {
    printf(COLOR_RED "\n✖ Invalid selection.\n\n" COLOR_RESET);
    return 1;
  }

  scan_ports(hosts[choice - 1].ip);
  printf(COLOR_CYAN "\n[ ✔ Scan Completed ]\n\n" COLOR_RESET);
  return 0;
}