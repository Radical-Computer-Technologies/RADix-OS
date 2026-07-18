#ifndef RAD_SYSROOT_LANGINFO_H
#define RAD_SYSROOT_LANGINFO_H

typedef int nl_item;
#define CODESET 1
char *nl_langinfo(nl_item item);

#endif
