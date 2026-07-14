#ifndef RADIX_SYSROOT_LANGINFO_H
#define RADIX_SYSROOT_LANGINFO_H

typedef int nl_item;
#define CODESET 1
char *nl_langinfo(nl_item item);

#endif
