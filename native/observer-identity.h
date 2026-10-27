#ifndef SILPH_OBSERVER_IDENTITY_H
#define SILPH_OBSERVER_IDENTITY_H
#include <stdbool.h>
#include <string.h>
static inline bool silph_observer_name_valid(const char *name){
    if(!name)return false;if(!strcmp(name,"libsilph-native-observer.so"))return true;
    const char *prefix="libsilph-native-observer-";size_t length=strlen(prefix);
    if(strncmp(name,prefix,length))return false;const char *id=name+length;
    return strlen(id)==21&&id[0]>='1'&&id[0]<='3'&&id[1]=='-'
        &&strspn(id+2,"0123456789abcdef")==16&&!strcmp(id+18,".so");
}
#endif
