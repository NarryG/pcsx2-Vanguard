#include "PrecompiledHeader.h"
#include "UnmanagedHelper.h"

void UnmanagedHelper::SaveState(const wxString &file)
{
    StateCopy_SaveToFile(file);
}

void UnmanagedHelper::LoadState(const wxString &file)
{
    StateCopy_LoadFromFile(file);
}
