#include "PCSX2MemoryDomain.h"

using namespace cli;
using namespace System;
using namespace RTCV;
using namespace NetCore;
using namespace Vanguard;
using namespace Runtime::InteropServices;
using namespace Diagnostics;

#using <System.dll>
#define EERAM_SIZE 33554432
#define WORD_SIZE 4
#define BIG_ENDIAN false


delegate void MessageDelegate(Object^);

String ^ EERAM::Name::get()
{
  return "EERAM";
}

long long EERAM::Size::get()
{
    return EERAM_SIZE;
}

int EERAM::WordSize::get()
{
  return WORD_SIZE;
}

bool EERAM::BigEndian::get()
{
  return BIG_ENDIAN;
}

unsigned char EERAM::PeekByte(long long addr)
{
    if (addr < EERAM_SIZE)
    {
    }
    return 0;
}

array<unsigned char> ^ EERAM::PeekBytes(long long address, int length)
{
  array<unsigned char> ^ bytes = gcnew array<unsigned char>(length);
  for (int i = 0; i < length; i++)
  {
    bytes[i] = PeekByte(address + i);
  }
  return bytes;
}

void EERAM::PokeByte(long long addr, unsigned char val)
{
  if (addr < EERAM_SIZE)
  {
  }
}