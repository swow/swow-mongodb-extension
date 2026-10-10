/* Optional native waiting provider; original driver semantics own the result. */
#ifndef MONGOC_RUNTIME_H
#define MONGOC_RUNTIME_H
#include "php_mongodb_runtime.h"
#include <mongoc/mongoc.h>
struct addrinfo;

void mongoc_runtime_set_hooks(const php_mongodb_runtime_hooks *hooks);
const php_mongodb_runtime_hooks *mongoc_runtime_hooks(void);
int mongoc_runtime_poll(php_mongodb_pollfd_t *fds, size_t count, int timeout_ms);
void mongoc_runtime_sleep(int64_t usec);
int mongoc_runtime_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **result);
#if defined(MONGOC_HAVE_RES_NSEARCH) || defined(MONGOC_HAVE_RES_SEARCH)
int mongoc_runtime_res_search(const char *host, int cls, int type, unsigned char *answer, int length, bool tcp);
#endif
#ifdef MONGOC_HAVE_DNSAPI
#include <windns.h>
DNS_STATUS mongoc_runtime_dns_query(const char *host, WORD type, DWORD options, PDNS_RECORD *records);
#endif
#endif
