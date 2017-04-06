extern "C" 
{
#include "ica.h"
#include "ica-c2h.h"
}

typedef struct _ICO_C2H
{
    VD_C2H  Header;
    USHORT  usMaxDataSize;      /* maximum data block size */
} ICO_C2H, * PICO_C2H;