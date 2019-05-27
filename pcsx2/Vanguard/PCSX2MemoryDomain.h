#pragma once
public ref class EERAM : RTCV::CorruptCore::IMemoryDomain
{
public:
  property System::String^ Name { virtual System::String^ get(); }
  property long long Size { virtual long long get(); }
  property int WordSize { virtual int get(); }
  property bool BigEndian { virtual bool get(); }
  virtual unsigned char PeekByte(long long addr);
  virtual array<unsigned char> ^ PeekBytes(long long address, int length);
  virtual void PokeByte(long long addr, unsigned char val);
};