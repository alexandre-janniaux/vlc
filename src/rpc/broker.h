#ifndef RPC_BROKER_HH
#define RPC_BROKER_HH

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __cplusplus
#include <stdbool.h>
#endif

int vlc_broker_Init(void);
int vlc_broker_CreateAccess(const char* url, bool preparse);

#ifdef __cplusplus
}
#endif


#endif
