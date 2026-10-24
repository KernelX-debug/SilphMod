#ifndef SILPH_MANAGED_PROTO_H
#define SILPH_MANAGED_PROTO_H
#include "managed-action.h"
typedef struct {
    uint32_t abi,reserved;
    uint64_t encounter;
    double latitude,longitude,player_latitude,player_longitude;
    int32_t item,count;
    uint32_t quality,curve;
    char fort[161],location[17];
} SilphRequestInput;
int silph_proto_enable(void *,SilphManagedResolver,void *);
int silph_proto_validate(uint32_t,const SilphRequestInput *);
int silph_proto_request(void *,uint32_t,const SilphRequestInput *,SilphManagedRef *);
int silph_proto_token(void *,double,double,char *,uint32_t);
#endif
