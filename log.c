#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "log.h"

int LogLevel = LOGLEVEL_INFO;

void __logi(int level, const char *pzFunc,const char *pzFile,int line,const char *pzMessage,...) {
	char tmpStr[1024],tmpStr2[1024];
	va_list ap;

	if (level > LogLevel)
		return;
	snprintf(tmpStr,1023,"%s@%s:%i: ",pzFunc,pzFile,line);

	va_start(ap,pzMessage);
	vsnprintf(tmpStr2,1023,pzMessage,ap);
	va_end(ap);

	strcat(tmpStr,tmpStr2);

	printf("%s\n",tmpStr);
}
