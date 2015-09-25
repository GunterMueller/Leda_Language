/// Copyright 1993-2015 Timothy A. Budd
// -----------------------------------------------------------------------------
//  This file is part of
/// ---     Leda: Multiparadigm Programming Language
// -----------------------------------------------------------------------------
//
//  Leda is free software: you can redistribute it and/or modify it under the
//  terms of the MIT license, see file "COPYING" included in this distribution.
//
// -----------------------------------------------------------------------------
/// Title: Test system pointer and value sizes
// -----------------------------------------------------------------------------

#include <stdio.h>
#include "memory.h"

void main()
{
    printf
    (
        "size pointer %d size int %d size real %d\n",
        sizeof(struct ledaValue*),
        sizeof(int),
        sizeof(double)
    );
}


// -----------------------------------------------------------------------------
