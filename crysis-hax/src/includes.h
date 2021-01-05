#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <iostream>
#include <vector>
#include <Psapi.h>

#include <dxgi.h>
#pragma comment (lib, "dxgi.lib")

#include <d3d10.h>
#pragma comment (lib, "d3d10.lib")

#include "minhook/minhook.h"
#pragma comment (lib, "minhook.lib")

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_dx10.h"
#include "imgui/imgui_impl_win32.h"
