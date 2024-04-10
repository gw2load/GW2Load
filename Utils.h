#pragma once

#include "Common.h"

std::string_view GetWndProcMessageName(UINT msg);

const void* fptr(const auto* p)
{
    return static_cast<const void*>(p);
}