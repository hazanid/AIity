#pragma once

#include "AIityWorldState.h"

namespace AIity
{
std::string SerializeWorld(const FWorldState& State);
bool DeserializeWorld(const std::string& Text, FWorldState& State, std::string& Error);
}
