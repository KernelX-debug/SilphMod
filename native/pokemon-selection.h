#ifndef SILPH_POKEMON_SELECTION_H
#define SILPH_POKEMON_SELECTION_H
#include <stdint.h>
#include <stdbool.h>
#define SILPH_KEEP_BYTES 136u
#define SILPH_REASON_SELECTED_SPECIES UINT32_C(262144)
#define SILPH_SELECTION_MAX 1063u
typedef struct {uint16_t species,form,key;} SilphPokemonForm;
static const SilphPokemonForm silph_pokemon_forms[]={
    {19,46,1026},
    {20,48,1027},
    {26,50,1028},
    {27,52,1029},
    {28,54,1030},
    {37,56,1031},
    {38,58,1032},
    {50,60,1033},
    {51,62,1034},
    {52,64,1035},
    {52,2335,1036},
    {53,66,1037},
    {74,68,1038},
    {75,70,1039},
    {76,72,1040},
    {77,2336,1041},
    {78,2337,1042},
    {79,2582,1043},
    {80,2583,1044},
    {83,2338,1045},
    {88,74,1046},
    {89,76,1047},
    {103,78,1048},
    {105,80,1049},
    {110,944,1050},
    {122,2339,1051},
    {144,2801,1052},
    {145,2800,1053},
    {146,2799,1054},
    {199,2584,1055},
    {222,2340,1056},
    {263,946,1057},
    {264,948,1058},
    {554,2341,1059},
    {555,2342,1060},
    {555,2343,1061},
    {562,2344,1062},
    {618,2345,1063},
};
static inline bool silph_species_regional(uint32_t species){
    for(unsigned i=0;i<sizeof(silph_pokemon_forms)/sizeof(*silph_pokemon_forms);i++)if(silph_pokemon_forms[i].species==species)return true;return false;
}
static inline uint32_t silph_selection_key(uint32_t species,uint32_t form){
    for(unsigned i=0;i<sizeof(silph_pokemon_forms)/sizeof(*silph_pokemon_forms);i++)if(silph_pokemon_forms[i].species==species&&silph_pokemon_forms[i].form==form)return silph_pokemon_forms[i].key;return species;
}
static inline bool silph_selection_has(const uint8_t *bits,uint32_t bytes,uint32_t species,uint32_t form){
    uint32_t key=silph_selection_key(species,form);return key&&key<bytes*8&&(bits[key/8]&(1u<<(key%8)));
}
#endif
