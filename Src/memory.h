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
/// Title: Memory management for the Leda system
///  Description:
//    Uses a variation on the Baker two-space algorithm
//
//    The fundamental data type is the object.
//    The first field in an object is a size, the low order two
//    bits being used to maintain:
//    * binary flag, used if data is binary
//    * indirection flag, used if object has been relocated
//    The first two data fields are always the class
//    and a surrounding context, while remaining
//    data fields are values (either binary or object pointers)
//
//    A few objects (class tables, other items that are guaranteed
//    not to change) are allocated in static memory space -- space
//    which is not ever garbage collected.
//    The only pointer from static memory back to dynamic memory is
//    the global context.
// -----------------------------------------------------------------------------

#ifndef memory_h
#define memory_h

// -----------------------------------------------------------------------------
///  ledaValue
// -----------------------------------------------------------------------------

struct ledaValue
{
    int size;
    struct ledaValue* data[0];
};

// memoryBase holds the pointer to the current space,
// memoryPointer is the pointer into this space.
// To allocate, decrement memoryPointer by the correct amount.
// If the result is less than memoryBase, then garbage collection
// must take place

extern struct ledaValue* memoryPointer;
extern struct ledaValue* memoryBase;

// -----------------------------------------------------------------------------
///  Roots for the memory space
// -----------------------------------------------------------------------------
//- These are traced down during memory management

# define ROOTSTACKLIMIT 250
extern struct ledaValue* rootStack[];
extern int rootTop;
extern struct ledaValue* globalContext;
extern struct ledaValue* currentContext;


// -----------------------------------------------------------------------------
///  entry points
// -----------------------------------------------------------------------------

void gcinit(int, int);
struct ledaValue* gcollect(int);
struct ledaValue* staticAllocate(int);

# define gcalloc(sz) (((memoryPointer-=((sz)+2))<memoryBase)?                  \
    gcollect(sz):(memoryPointer->size=(sz)<<2,memoryPointer))

# ifndef gcalloc
    extern struct ledaValue* gcalloc(int);
# endif

int yyerror(char* s);


// -----------------------------------------------------------------------------
#endif // memory_h
// -----------------------------------------------------------------------------
