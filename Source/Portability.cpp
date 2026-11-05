//  Float routines JUCE and the vendored HarfBuzz take from libm, redefined
//  inside the plug-in so its binary does not ask libm for a symbol version
//  that older hosts do not ship. See the Build note in README.md.

#ifdef __linux__
/*
    The float math JUCE and HarfBuzz call through libm is: std::atan2 and
    std::sqrt on floats (PathStrokeType, Point.h) and sqrtf in hb-ft.cc.
    glibc 2.43 moved the default symbol versions of sqrtf and atan2f, so a
    binary linked on a glibc 2.43 system states "sqrtf@GLIBC_2.43" and
    "atan2f@GLIBC_2.43" for the two calls, and a host whose loader resolves an
    older libm refuses to load the plug-in at all rather than simply missing
    the two functions: Bitwig Studio 6.1.4 runs as a Flatpak whose root ships
    glibc 2.42, and its scan reported

        /usr/lib/x86_64-linux-gnu/libm.so.6: version `GLIBC_2.43' not found
        (required by .../Re.Trektor.so)

    Defining both here binds those calls inside the plug-in's own DSO, so the
    newest libm version the binary asks for stays at GLIBC_2.38, which such
    hosts do have.

    The names are aliased in assembly instead of being declared as sqrtf and
    atan2f directly: GCC ignores a visibility attribute on a libc name
    ("visibility attribute ignored because it conflicts with previous
    declaration"), so such definitions would be exported and interpose on
    every other plug-in that shares the host process. .hidden is set by hand
    here, which keeps them out of the dynamic symbol table. This file is
    built without LTO (see CMakeLists.txt) because the LTO plugin resolves
    these aliases before it has seen rtr_sqrtf and rtr_atan2f.

    sqrtf compiles to the sqrtss instruction, so libm is not involved at all,
    and atan2f takes libm's double atan2, whose version is unchanged since
    GLIBC_2.2.5 and which is more accurate than the float one it replaces.
*/
extern "C" {
    double atan2 (double, double);

    float rtr_sqrtf (float x) { return __builtin_sqrtf (x); }

    float rtr_atan2f (float y, float x) { return (float) atan2 ((double) y, (double) x); }
}

__asm__ (".globl sqrtf\n"
         " .hidden sqrtf\n"
         " .type sqrtf, @function\n"
         " .set sqrtf, rtr_sqrtf\n\n"
         " .globl atan2f\n"
         " .hidden atan2f\n"
         " .type atan2f, @function\n"
         " .set atan2f, rtr_atan2f");
#endif
