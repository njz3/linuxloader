#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    // Rewrites an assembly program that relies on OPTION NV_fragment_program2 / NV_vertex_program2-3 (the fp40/vp40
    // output of cgc) into plain ARB_fragment_program / ARB_vertex_program, for drivers that only know the ARB
    // grammar. Returns a malloc'ed string, or NULL when the program has no NVIDIA option or uses a construct that
    // cannot be expressed (flow control, unknown opcode...).
    char *nvProgramToArb(const char *program, int len, int isFragment);

#ifdef __cplusplus
}
#endif
