#include "transfer-policy.h"

size_t silph_transfer_input_size(void) { return sizeof(SilphTransferInput); }
int silph_transfer_evaluate(const SilphTransferInput *input, size_t bytes, SilphTransferVerdict *output) {
    if (!output || (uintptr_t)output%_Alignof(SilphTransferVerdict)) return 1;
    if (input && ((uintptr_t)input%_Alignof(SilphTransferInput)
        || ((uintptr_t)output>=(uintptr_t)input && (uintptr_t)output-(uintptr_t)input<sizeof(*input))
        || ((uintptr_t)input>(uintptr_t)output && (uintptr_t)input-(uintptr_t)output<sizeof(*output)))) return 1;
    output->decision=SILPH_VERDICT_UNKNOWN;output->reasons=SILPH_REASON_INVALID;
    if (!input || bytes!=sizeof(*input) || input->abi!=SILPH_TRANSFER_ABI || input->reserved
        || (input->valid&~15u) || (input->known_flags&~1023u) || (input->flags&~input->known_flags)
        || (input->options_present&~127u) || (input->options_known&~input->options_present)
        || (input->options&~input->options_known)) return 1;
    if ((input->valid&SILPH_VALID_ID) && input->owned_id && (input->known_flags&SILPH_DATA_EGG)
        && (input->flags&SILPH_DATA_EGG)) {
        output->decision=SILPH_VERDICT_RETAIN;output->reasons=SILPH_REASON_EGG;return 0;
    }
    if (input->valid!=15u || !input->owned_id || input->species<=0
        || input->attack<0 || input->attack>15 || input->defense<0 || input->defense>15 || input->stamina<0 || input->stamina>15
        || input->pokemon_class<0 || input->pokemon_class>3 || (input->known_flags&127u)!=127u
        || (input->options_known&3u)!=3u) return 0;
    uint32_t present=input->options_present, known=input->options_known, options=input->options, flags=input->flags;
    if ((present&SILPH_KEEP_RESEARCH) && !(known&SILPH_KEEP_RESEARCH)) { output->reasons=SILPH_REASON_ORIGIN;return 0; }
    if ((options&SILPH_KEEP_RESEARCH) && (input->source<SILPH_SOURCE_WILD || input->source>SILPH_SOURCE_SAMPLE)) {
        output->reasons=SILPH_REASON_ORIGIN;return 0;
    }
    uint32_t reasons=(options&SILPH_KEEP_RESEARCH) && input->source==SILPH_SOURCE_RESEARCH ? SILPH_REASON_RESEARCH : 0;
    const uint32_t optional[]={SILPH_KEEP_DYNAMAX,SILPH_KEEP_HATCHED,SILPH_KEEP_BACKGROUND,SILPH_KEEP_SHINY_BACKGROUND};
    const uint32_t required[]={SILPH_DATA_DYNAMAX,SILPH_DATA_HATCHED,SILPH_DATA_BACKGROUND,SILPH_DATA_BACKGROUND};
    for (unsigned i=0;i<4;i++) if (((present&optional[i]) && !(known&optional[i]))
        || ((options&optional[i]) && !(input->known_flags&required[i]))) return 0;
    if ((flags&SILPH_DATA_SHINY) && (options&SILPH_KEEP_SHINY)) reasons|=SILPH_REASON_SHINY;
    if (input->attack==15 && input->defense==15 && input->stamina==15 && (options&SILPH_KEEP_HUNDO)) reasons|=SILPH_REASON_HUNDO;
    if ((options&SILPH_KEEP_DYNAMAX) && (flags&SILPH_DATA_DYNAMAX)) reasons|=SILPH_REASON_DYNAMAX;
    if ((options&SILPH_KEEP_HATCHED) && (flags&SILPH_DATA_HATCHED)) reasons|=SILPH_REASON_HATCHED;
    if ((options&SILPH_KEEP_BACKGROUND) && (flags&SILPH_DATA_BACKGROUND)) reasons|=SILPH_REASON_BACKGROUND;
    if ((options&SILPH_KEEP_SHINY_BACKGROUND) && (flags&SILPH_DATA_SHINY) && (flags&SILPH_DATA_BACKGROUND)) reasons|=SILPH_REASON_SHINY_BACKGROUND;
    const uint32_t protected_flags[]={SILPH_DATA_FAVORITE,SILPH_DATA_EGG,SILPH_DATA_DEPLOYED,SILPH_DATA_BUDDY,SILPH_DATA_FUSION,SILPH_DATA_LIMITED};
    const uint32_t protected_reasons[]={SILPH_REASON_FAVORITE,SILPH_REASON_EGG,SILPH_REASON_DEPLOYED,SILPH_REASON_BUDDY,SILPH_REASON_FUSION,SILPH_REASON_LIMITED};
    for (unsigned i=0;i<6;i++) if (flags&protected_flags[i]) reasons|=protected_reasons[i];
    if (input->pokemon_class) reasons|=SILPH_REASON_SPECIAL;
    output->decision=reasons?SILPH_VERDICT_RETAIN:SILPH_VERDICT_CANDIDATE;
    output->reasons=reasons?reasons:SILPH_REASON_SESSION_CANDIDATE;return 0;
}
