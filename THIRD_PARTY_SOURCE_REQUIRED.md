# Phase 2 dependency note

This Phase 2 delivery is an overlay ZIP because the complete vendored llama.cpp tree from baseline commit `696529857642f607b86618a7f24c13e888c49bdc` was not available to the file-generation environment.

The existing repository must retain:

`third_party/llama.cpp/`

at the exact pinned commit:

`391fac16460f15233a7740550d858ac96df3419d`

Do not run the vendor script as part of Phase 2 validation if the existing vendored source is already present. Do not replace or re-vendor the pinned tree.
