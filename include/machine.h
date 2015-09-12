#ifndef MACHINE_H
#define MACHINE_H

/* Adopt types from wx/defs.h to suit our definitions */
#include <wx/defs.h>

#ifdef WORDS_BIGENDIAN
#define BYTES_HIGHFIRST  1
#endif

#ifdef wxUint64
typedef  wxUint64 uint64;
#endif
typedef  wxUint32 uint32;
typedef  wxUint16 uint16;
typedef  wxUint8 uint8;

#ifdef wxInt64
typedef  wxInt64 int64;
#endif
typedef  wxInt32 int32;
typedef  wxInt16 int16;
typedef  wxInt8 int8;



typedef int8 sint8;
typedef int16 sint16;
typedef int32 sint32;

#endif
