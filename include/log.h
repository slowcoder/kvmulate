#pragma once

#include <stdarg.h>
#include <stdlib.h>

#include "types.h"

enum {
	LOGLEVEL_ERROR,
	LOGLEVEL_WARN,
	LOGLEVEL_INFO,
	LOGLEVEL_VERB,
	LOGLEVEL_DEBUG,
};

extern int LogLevel;

#ifdef __cplusplus
extern "C" {
#endif
void __logi(int level, const char *pzFunc,const char *pzFile,int line,const char *pzMessage,...);
void emu_dump(void);
#ifdef __cplusplus
}
#endif

#define LOGE(x,...) __logi(LOGLEVEL_ERROR, __func__,__FILE__,__LINE__,x,##__VA_ARGS__)
#define LOGW(x,...) __logi(LOGLEVEL_WARN, __func__,__FILE__,__LINE__,x,##__VA_ARGS__)
#define LOGI(x,...) __logi(LOGLEVEL_INFO, __func__,__FILE__,__LINE__,x,##__VA_ARGS__)
#define LOGV(x,...) __logi(LOGLEVEL_VERB, __func__,__FILE__,__LINE__,x,##__VA_ARGS__)
#define LOGD(x,...) __logi(LOGLEVEL_DEBUG, __func__,__FILE__,__LINE__,x,##__VA_ARGS__)
#define ASSERT(c,x,...) { if( !(c) ) { __logi(0, __func__,__FILE__,__LINE__,x,##__VA_ARGS__); /*emu_dump();*/ abort(); } }

#define LOG LOGI
