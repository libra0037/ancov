#define CA_CERT_DIR_PATH "/etc/ssl/certs"
#define SVR_CERT_PATH "cert.pem"
#define SVR_KEY_PATH "key.pem"
#define ADMINISTRATOR -1

#ifdef _MSC_VER
#define _REGEX_MAX_COMPLEXITY_COUNT 0
#define _REGEX_MAX_STACK_COUNT 0
#endif
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include <stack>
#include <unordered_map>
#ifdef _WIN32
#include <Windows.h>
#endif
