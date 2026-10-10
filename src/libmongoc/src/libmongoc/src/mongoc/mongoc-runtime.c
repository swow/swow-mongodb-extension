/* Optional waiting integration. No protocol, TLS or BSON work is moved to
 * another thread. A cancelled DNS waiter must not free a running DNS job. */
#include <mongoc/mongoc-runtime.h>
#include <bson/bson.h>
#include <common-atomic-private.h>
#include <mlib/time_point.h>
#include <errno.h>
#include <string.h>
#ifndef _WIN32
#include <netdb.h>
#else
#include <ws2tcpip.h>
#endif
#if defined(MONGOC_HAVE_RES_NSEARCH) || defined(MONGOC_HAVE_RES_SEARCH)
#include <resolv.h>
#endif

#ifdef _MSC_VER
static __declspec(thread) const php_mongodb_runtime_hooks *runtime_hooks;
#else
static __thread const php_mongodb_runtime_hooks *runtime_hooks;
#endif

void mongoc_runtime_set_hooks(const php_mongodb_runtime_hooks *hooks)
{
   runtime_hooks = hooks;
}

const php_mongodb_runtime_hooks *mongoc_runtime_hooks(void)
{
   return runtime_hooks && runtime_hooks->active() ? runtime_hooks : NULL;
}

int mongoc_runtime_poll(php_mongodb_pollfd_t *fds, size_t count, int timeout_ms)
{
   const php_mongodb_runtime_hooks *hooks = mongoc_runtime_hooks();
   if (hooks && timeout_ms != 0) {
      return hooks->poll(fds, count, timeout_ms);
   }
#ifdef _WIN32
   return WSAPoll(fds, (ULONG) count, timeout_ms);
#else
   return poll(fds, count, timeout_ms);
#endif
}

void mongoc_runtime_sleep(int64_t usec)
{
   const php_mongodb_runtime_hooks *hooks = mongoc_runtime_hooks();
   if (hooks && usec > 0) {
      hooks->sleep(usec);
   } else {
      mlib_sleep_for(usec, us);
   }
}

typedef struct {
   int32_t references;
   int result;
   int error;
   char *node;
   char *service;
   struct addrinfo hints;
   struct addrinfo *addresses;
} mongoc_runtime_address_job;

static void address_cleanup(void *ptr)
{
   mongoc_runtime_address_job *job = ptr;
   if (mcommon_atomic_int32_fetch_add(&job->references, -1, mcommon_memory_order_seq_cst) != 1) {
      return;
   }
   if (job->addresses) {
      freeaddrinfo(job->addresses);
   }
   bson_free(job->node);
   bson_free(job->service);
   bson_free(job);
}

static void address_work(void *ptr)
{
   mongoc_runtime_address_job *job = ptr;
   job->result = getaddrinfo(job->node, job->service, &job->hints, &job->addresses);
   job->error = errno;
#ifdef _WIN32
   job->error = WSAGetLastError();
#endif
}

int mongoc_runtime_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **result)
{
   const php_mongodb_runtime_hooks *hooks = mongoc_runtime_hooks();
   mongoc_runtime_address_job *job;
   int ret;
   int saved_errno = errno;
#ifdef _WIN32
   int saved_socket_error = WSAGetLastError();
#endif
   if (!hooks) {
      return getaddrinfo(node, service, hints, result);
   }
   job = bson_malloc0(sizeof(*job));
   job->references = 2;
   job->node = node ? bson_strdup(node) : NULL;
   job->service = service ? bson_strdup(service) : NULL;
   job->hints = *hints;
   job->hints.ai_addr = NULL;
   job->hints.ai_canonname = NULL;
   job->hints.ai_next = NULL;
   if (!hooks->work(address_work, address_cleanup, job)) {
      address_cleanup(job);
      errno = ECANCELED;
#ifdef _WIN32
      WSASetLastError(WSAEINTR);
      return WSAEINTR;
#else
      return EAI_SYSTEM;
#endif
   }
   ret = job->result;
   *result = job->addresses;
   job->addresses = NULL;
#ifdef _WIN32
   errno = saved_errno;
   WSASetLastError(ret == 0 ? saved_socket_error : job->error);
#else
   errno = ret == EAI_SYSTEM ? job->error : saved_errno;
#endif
   address_cleanup(job);
   return ret;
}

#if defined(MONGOC_HAVE_RES_NSEARCH) || defined(MONGOC_HAVE_RES_SEARCH)
typedef struct {
   int32_t references;
   char *host;
   int cls;
   int type;
   int length;
   int result;
   int error;
   int dns_error;
   bool tcp;
   unsigned char answer[];
} mongoc_runtime_dns_job;

static void dns_cleanup(void *ptr)
{
   mongoc_runtime_dns_job *job = ptr;
   if (mcommon_atomic_int32_fetch_add(&job->references, -1, mcommon_memory_order_seq_cst) == 1) {
      bson_free(job->host);
      bson_free(job);
   }
}

static void dns_work(void *ptr)
{
   mongoc_runtime_dns_job *job = ptr;
#ifdef MONGOC_HAVE_RES_NSEARCH
   struct __res_state state = {0};
   res_ninit(&state);
   if (job->tcp) {
      state.options |= RES_USEVC;
   }
   job->result = res_nsearch(&state, job->host, job->cls, job->type, job->answer, job->length);
#else
   job->result = res_search(job->host, job->cls, job->type, job->answer, job->length);
#endif

   job->error = errno;
   job->dns_error = h_errno;
#ifdef MONGOC_HAVE_RES_NDESTROY
   res_ndestroy(&state);
#elif defined(MONGOC_HAVE_RES_NCLOSE)
   res_nclose(&state);
#endif
}

int mongoc_runtime_res_search(const char *host, int cls, int type, unsigned char *answer, int length, bool tcp)
{
   const php_mongodb_runtime_hooks *hooks = mongoc_runtime_hooks();
   mongoc_runtime_dns_job *job = bson_malloc0(sizeof(*job) + length);
   int ret;
   job->references = 2;
   job->host = bson_strdup(host);
   job->cls = cls;
   job->type = type;
   job->length = length;
   job->tcp = tcp;
   if (!hooks->work(dns_work, dns_cleanup, job)) {
      dns_cleanup(job);
      errno = ECANCELED;
      h_errno = TRY_AGAIN;
      return -1;
   }
   ret = job->result;
   if (ret > 0) {
      memcpy(answer, job->answer, (size_t) ret < (size_t) length ? (size_t) ret : (size_t) length);
   }
   errno = job->error;
   h_errno = job->dns_error;
   dns_cleanup(job);
   return ret;
}
#endif

#ifdef MONGOC_HAVE_DNSAPI
typedef struct {
   int32_t references;
   char *host;
   WORD type;
   DWORD options;
   DNS_STATUS result;
   PDNS_RECORD records;
} mongoc_runtime_windows_dns_job;

static void windows_dns_cleanup(void *ptr)
{
   mongoc_runtime_windows_dns_job *job = ptr;
   if (mcommon_atomic_int32_fetch_add(&job->references, -1, mcommon_memory_order_seq_cst) == 1) {
      if (job->records) {
         DnsRecordListFree(job->records, DnsFreeRecordList);
      }
      bson_free(job->host);
      bson_free(job);
   }
}

static void windows_dns_work(void *ptr)
{
   mongoc_runtime_windows_dns_job *job = ptr;
   job->result = DnsQuery_UTF8(job->host, job->type, job->options, NULL, &job->records, NULL);
}

DNS_STATUS mongoc_runtime_dns_query(const char *host, WORD type, DWORD options, PDNS_RECORD *records)
{
   const php_mongodb_runtime_hooks *hooks = mongoc_runtime_hooks();
   mongoc_runtime_windows_dns_job *job;
   DNS_STATUS ret;
   if (!hooks) {
      return DnsQuery_UTF8(host, type, options, NULL, records, NULL);
   }
   job = bson_malloc0(sizeof(*job));
   job->references = 2;
   job->host = bson_strdup(host);
   job->type = type;
   job->options = options;
   if (!hooks->work(windows_dns_work, windows_dns_cleanup, job)) {
      windows_dns_cleanup(job);
      return ERROR_CANCELLED;
   }
   ret = job->result;
   *records = job->records;
   job->records = NULL;
   windows_dns_cleanup(job);
   return ret;
}
#endif
