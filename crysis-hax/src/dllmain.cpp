#include "includes.h"

HMODULE hMyModule;
HWND hGame;

LPVOID pPresentAddress;
WNDPROC pWndProc;

using fPresent = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
fPresent pPresent = NULL;

LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

DWORD ammo_instruction_address;
DWORD weapon_recoil_instruction_address;

void infinity_energy(DWORD cry_game_module);
void god_mode(DWORD cry_game_module);
void infinity_ammo(DWORD ammo_instruction_address, bool is_enable);
void no_recoil(DWORD weapon_recoil_instruction_address, bool is_enable);
void no_take_damage(DWORD health_take_instruction_address, bool is_enable);

namespace vars
{
	bool menu_open = false;

	bool unload_dll = false;

	bool enable_infinity_energy = false;
	float my_energy = 999.f;
	bool enable_god_mode = false;
	float my_health = 228.f;
	bool enable_unlimited_ammo = false;
	bool enable_no_recoil = false;
	/*bool enable_no_take_damage = false;*/
}

namespace menu_param
{
	ImVec2 vWindowSize;
	ImVec2 vWindowPos;

	ImFont* font_Main;
	ImFont* font_Credits;
}

#define WindowName  "alternative hack for crysis"

ImVec2 operator+(ImVec2 p1, ImVec2 p2) { return ImVec2(p1.x + p2.x, p1.y + p2.y); }
ImVec2 operator-(ImVec2 p1, ImVec2 p2) { return ImVec2(p1.x - p2.x, p1.y - p2.y); }
ImVec2 operator/(ImVec2 p1, ImVec2 p2) { return ImVec2(p1.x / p2.x, p1.y / p2.y); }
ImVec2 operator*(ImVec2 p1, int value) { return ImVec2(p1.x * value, p1.y * value); }
ImVec2 operator*(ImVec2 p1, float value) { return ImVec2(p1.x * value, p1.y * value); }

ImVec4 operator+(float val, ImVec4 p2) { return ImVec4(val + p2.x, val + p2.y, val + p2.z, val + p2.w); }
ImVec4 operator*(float val, ImVec4 p2) { return ImVec4(val * p2.x, val * p2.y, val * p2.z, val * p2.w); }
ImVec4 operator*(ImVec4 p2, float val) { return ImVec4(val * p2.x, val * p2.y, val * p2.z, val * p2.w); }
ImVec4 operator-(ImVec4 p1, ImVec4 p2) { return ImVec4(p1.x - p2.x, p1.y - p2.y, p1.z - p2.z, p1.w - p2.w); }
ImVec4 operator*(ImVec4 p1, ImVec4 p2) { return ImVec4(p1.x * p2.x, p1.y * p2.y, p1.z * p2.z, p1.w * p2.w); }
ImVec4 operator/(ImVec4 p1, ImVec4 p2) { return ImVec4(p1.x / p2.x, p1.y / p2.y, p1.z / p2.z, p1.w / p2.w); }

ImVec4 boxGaussianIntegral(ImVec4 x)
{
	const ImVec4 s = ImVec4(x.x > 0 ? 1.0f : -1.0f, x.y > 0 ? 1.0f : -1.0f, x.z > 0 ? 1.0f : -1.0f, x.w > 0 ? 1.0f : -1.0f);
	const ImVec4 a = ImVec4(fabsf(x.x), fabsf(x.y), fabsf(x.z), fabsf(x.w));
	const ImVec4 res = 1.0f + (0.278393f + (0.230389f + 0.078108f * (a * a)) * a) * a;
	const ImVec4 resSquared = res * res;
	return s - s / (resSquared * resSquared);
}

ImVec4 boxLinearInterpolation(ImVec4 x)
{
	const float maxClamp = 1.0f;
	const float minClamp = -1.0f;
	return ImVec4(x.x > maxClamp ? maxClamp : x.x < minClamp ? minClamp : x.x,
		x.y > maxClamp ? maxClamp : x.y < minClamp ? minClamp : x.y,
		x.z > maxClamp ? maxClamp : x.z < minClamp ? minClamp : x.z,
		x.w > maxClamp ? maxClamp : x.w < minClamp ? minClamp : x.w);
}

float boxShadow(ImVec2 lower, ImVec2 upper, ImVec2 point, float sigma, bool linearInterpolation)
{
	const ImVec2 pointLower = point - lower;
	const ImVec2 pointUpper = point - upper;
	const ImVec4 query = ImVec4(pointLower.x, pointLower.y, pointUpper.x, pointUpper.y);
	const ImVec4 pointToSample = query * (sqrtf(0.5f) / sigma);
	const ImVec4 integral = linearInterpolation ? 0.5f + 0.5f * boxLinearInterpolation(pointToSample) : 0.5f + 0.5f * boxGaussianIntegral(pointToSample);
	return (integral.z - integral.x) * (integral.w - integral.y);
}

struct RectangleShadowSettings
{
	// Inputs
	bool    linear = false;
	float   sigma = 11.585f;
	ImVec2  padding = ImVec2(50, 50);
	ImVec2  rectPos = ImVec2(50, 50);
	ImVec2  rectSize = ImVec2(120, 120);
	ImVec2  shadowOffset = ImVec2(0, 0);
	ImVec2  shadowSize = ImVec2(120, 50);
	ImColor shadowColor = ImColor(0.6f, 0.6f, 0.6f, 1.0f);

	int  rings = 10;
	int  spacingBetweenRings = 1;
	int  samplesPerCornerSide = 20;
	int  spacingBetweenSamples = 1;

	// Outputs
	int totalVertices = 0;
	int totalIndices = 0;
};

void drawRectangleShadowVerticesAdaptive(RectangleShadowSettings& settings)
{
	const int    samplesSpan = settings.samplesPerCornerSide * settings.spacingBetweenSamples;
	const int    halfWidth = static_cast<int>(settings.rectSize.x / 2);
	const int    numSamplesInHalfWidth = (halfWidth / settings.spacingBetweenSamples) == 0 ? 1 : halfWidth / settings.spacingBetweenSamples;
	const int    numSamplesWidth = samplesSpan > halfWidth ? numSamplesInHalfWidth : settings.samplesPerCornerSide;
	const int    halfHeight = static_cast<int>(settings.rectSize.y / 2);
	const int    numSamplesInHalfHeight = (halfHeight / settings.spacingBetweenSamples) == 0 ? 1 : halfHeight / settings.spacingBetweenSamples;
	const int    numSamplesHeight = samplesSpan > halfHeight ? numSamplesInHalfHeight : settings.samplesPerCornerSide;
	const int    numVerticesInARing = numSamplesWidth * 4 + numSamplesHeight * 4 + 4;
	const ImVec2 whiteTexelUV = ImGui::GetIO().Fonts->TexUvWhitePixel;
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const ImVec2 rectangleTopLeft = origin + settings.rectPos;
	const ImVec2 rectangleBottomRight = rectangleTopLeft + settings.rectSize;
	const ImVec2 rectangleTopRight = rectangleTopLeft + ImVec2(settings.rectSize.x, 0);
	const ImVec2 rectangleBottomLeft = rectangleTopLeft + ImVec2(0, settings.rectSize.y);

	ImColor shadowColor = settings.shadowColor;
	settings.totalVertices = numVerticesInARing * settings.rings;
	settings.totalIndices = 6 * (numVerticesInARing) * (settings.rings - 1);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->PrimReserve(settings.totalIndices, settings.totalVertices);
	const ImDrawVert* shadowVertices = drawList->_VtxWritePtr;
	ImDrawVert* vertexPointer = drawList->_VtxWritePtr;

	for (int r = 0; r < settings.rings; ++r)
	{
		const float  adaptiveScale = (r / 2.5f) + 1;
		const ImVec2 ringOffset = ImVec2(adaptiveScale * r * settings.spacingBetweenRings, adaptiveScale * r * settings.spacingBetweenRings);
		for (int j = 0; j < 4; ++j)
		{
			ImVec2      corner;
			ImVec2      direction[2];
			const float spacingBetweenSamplesOnARing = static_cast<float>(settings.spacingBetweenSamples);
			switch (j)
			{
			case 0:
				corner = rectangleTopLeft + ImVec2(-ringOffset.x, -ringOffset.y);
				direction[0] = ImVec2(1, 0) * spacingBetweenSamplesOnARing;
				direction[1] = ImVec2(0, 1) * spacingBetweenSamplesOnARing;
				for (int i = 0; i < numSamplesWidth; ++i)
				{
					const ImVec2 point = corner + direction[0] * (numSamplesWidth - i);
					shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, point - settings.shadowOffset, settings.sigma, settings.linear);
					vertexPointer->pos = point;
					vertexPointer->uv = whiteTexelUV;
					vertexPointer->col = shadowColor;
					vertexPointer++;
				}

				shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, corner - settings.shadowOffset, settings.sigma, settings.linear);
				vertexPointer->pos = corner;
				vertexPointer->uv = whiteTexelUV;
				vertexPointer->col = shadowColor;
				vertexPointer++;

				for (int i = 0; i < numSamplesHeight; ++i)
				{
					const ImVec2 point = corner + direction[1] * (i + 1);
					shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, point - settings.shadowOffset, settings.sigma, settings.linear);
					vertexPointer->pos = point;
					vertexPointer->uv = whiteTexelUV;
					vertexPointer->col = shadowColor;
					vertexPointer++;
				}
				break;
			case 1:
				corner = rectangleBottomLeft + ImVec2(-ringOffset.x, +ringOffset.y);
				direction[0] = ImVec2(1, 0) * spacingBetweenSamplesOnARing;
				direction[1] = ImVec2(0, -1) * spacingBetweenSamplesOnARing;
				for (int i = 0; i < numSamplesHeight; ++i)
				{
					const ImVec2 point = corner + direction[1] * (numSamplesHeight - i);
					shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, point - settings.shadowOffset, settings.sigma, settings.linear);
					vertexPointer->pos = point;
					vertexPointer->uv = whiteTexelUV;
					vertexPointer->col = shadowColor;
					vertexPointer++;
				}

				shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, corner - settings.shadowOffset, settings.sigma, settings.linear);
				vertexPointer->pos = corner;
				vertexPointer->uv = whiteTexelUV;
				vertexPointer->col = shadowColor;
				vertexPointer++;

				for (int i = 0; i < numSamplesWidth; ++i)
				{
					const ImVec2 point = corner + direction[0] * (i + 1);
					shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, point - settings.shadowOffset, settings.sigma, settings.linear);
					vertexPointer->pos = point;
					vertexPointer->uv = whiteTexelUV;
					vertexPointer->col = shadowColor;
					vertexPointer++;
				}
				break;
			case 2:
				corner = rectangleBottomRight + ImVec2(+ringOffset.x, +ringOffset.y);
				direction[0] = ImVec2(-1, 0) * spacingBetweenSamplesOnARing;
				direction[1] = ImVec2(0, -1) * spacingBetweenSamplesOnARing;
				for (int i = 0; i < numSamplesWidth; ++i)
				{
					const ImVec2 point = corner + direction[0] * (numSamplesWidth - i);
					shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, point - settings.shadowOffset, settings.sigma, settings.linear);
					vertexPointer->pos = point;
					vertexPointer->uv = whiteTexelUV;
					vertexPointer->col = shadowColor;
					vertexPointer++;
				}

				shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, corner - settings.shadowOffset, settings.sigma, settings.linear);
				vertexPointer->pos = corner;
				vertexPointer->uv = whiteTexelUV;
				vertexPointer->col = shadowColor;
				vertexPointer++;

				for (int i = 0; i < numSamplesHeight; ++i)
				{
					const ImVec2 point = corner + direction[1] * (i + 1);
					shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, point - settings.shadowOffset, settings.sigma, settings.linear);
					vertexPointer->pos = point;
					vertexPointer->uv = whiteTexelUV;
					vertexPointer->col = shadowColor;
					vertexPointer++;
				}
				break;
			case 3:
				corner = rectangleTopRight + ImVec2(+ringOffset.x, -ringOffset.y);
				direction[0] = ImVec2(-1, 0) * spacingBetweenSamplesOnARing;
				direction[1] = ImVec2(0, 1) * spacingBetweenSamplesOnARing;
				for (int i = 0; i < numSamplesHeight; ++i)
				{
					const ImVec2 point = corner + direction[1] * (numSamplesHeight - i);
					shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, point - settings.shadowOffset, settings.sigma, settings.linear);
					vertexPointer->pos = point;
					vertexPointer->uv = whiteTexelUV;
					vertexPointer->col = shadowColor;
					vertexPointer++;
				}

				shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, corner - settings.shadowOffset, settings.sigma, settings.linear);
				vertexPointer->pos = corner;
				vertexPointer->uv = whiteTexelUV;
				vertexPointer->col = shadowColor;
				vertexPointer++;

				for (int i = 0; i < numSamplesWidth; ++i)
				{
					const ImVec2 point = corner + direction[0] * (i + 1);
					shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, point - settings.shadowOffset, settings.sigma, settings.linear);
					vertexPointer->pos = point;
					vertexPointer->uv = whiteTexelUV;
					vertexPointer->col = shadowColor;
					vertexPointer++;
				}
				break;
			}
		}
	}

	ImDrawIdx idx = (ImDrawIdx)drawList->_VtxCurrentIdx;

	for (int r = 0; r < settings.rings - 1; ++r)
	{
		const ImDrawIdx startOfRingIndex = idx;
		for (int i = 0; i < numVerticesInARing - 1; ++i)
		{
			drawList->_IdxWritePtr[0] = idx + 0;
			drawList->_IdxWritePtr[1] = idx + 1;
			drawList->_IdxWritePtr[2] = idx + numVerticesInARing;
			drawList->_IdxWritePtr[3] = idx + 1;
			drawList->_IdxWritePtr[4] = idx + numVerticesInARing + 1;
			drawList->_IdxWritePtr[5] = idx + numVerticesInARing;

			idx += 1;
			drawList->_IdxWritePtr += 6;
		}

		drawList->_IdxWritePtr[0] = idx + 0;
		drawList->_IdxWritePtr[1] = startOfRingIndex + 0;
		drawList->_IdxWritePtr[2] = startOfRingIndex + numVerticesInARing;
		drawList->_IdxWritePtr[3] = idx + 0;
		drawList->_IdxWritePtr[4] = startOfRingIndex + numVerticesInARing;
		drawList->_IdxWritePtr[5] = idx + numVerticesInARing;

		drawList->_IdxWritePtr += 6;
		idx += 1;
	}
	drawList->_VtxCurrentIdx += settings.totalVertices;
}

RectangleShadowSettings shadowSettings;

void drawShadow()
{
	/*ImGui::SetNextWindowSize(ImVec2(400, 800), ImGuiCond_Once);
	ImGui::Begin("Test Shadows");
	ImGui::Checkbox("Linear Falloff", &shadowSettings.linear);
	ImGui::SliderFloat("Shadow Sigma", &shadowSettings.sigma, 0, 50);
	ImGui::SliderInt("Corner samples", &shadowSettings.samplesPerCornerSide, 1, 20);
	ImGui::ColorPicker3("Shadow Color", &shadowSettings.shadowColor.Value.x, ImGuiColorEditFlags_PickerHueWheel);
	ImGui::End();*/

	static ImColor backgroundColor(255, 255, 255, 0);
	//shadowSettings.shadowColor = ImColor(255, 172, 19);
	shadowSettings.shadowColor = ImColor(0, 0, 0);
	shadowSettings.rectPos = shadowSettings.padding;
	shadowSettings.rectSize = menu_param::vWindowSize;
	shadowSettings.shadowSize.x = menu_param::vWindowSize.x + 100.f;
	shadowSettings.shadowSize.y = menu_param::vWindowSize.y + 100.f;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, (ImU32)backgroundColor);
	ImGui::PushStyleColor(ImGuiCol_Border, (ImU32)backgroundColor);

	ImGui::Begin("##MainWindowShadow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
	ImGui::SetWindowPos(ImVec2(menu_param::vWindowPos.x - 50.f, menu_param::vWindowPos.y - 50.f));
	ImGui::SetWindowSize(shadowSettings.shadowSize);
	drawRectangleShadowVerticesAdaptive(shadowSettings);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 origin(ImGui::GetCursorScreenPos());
	drawList->AddRect(origin, origin + shadowSettings.shadowSize, ImColor(255, 0, 0, 1));
	ImGui::End();
	ImGui::PopStyleColor();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
}

namespace menu_utils
{
	void tabs(const char* lebel[], const int arr_, ImVec2 size, int& cur_page, float x_offset = 0.f, bool* click_callback = false)
	{
		for (int i = 0; i < arr_; i++)
		{
			auto& style = ImGui::GetStyle();
			ImVec4 save_shit[2];
			save_shit[0] = style.Colors[ImGuiCol_Button];
			save_shit[1] = style.Colors[ImGuiCol_ButtonHovered];
			if (i == cur_page)
			{
				style.Colors[ImGuiCol_Button] = style.Colors[ImGuiCol_ButtonActive];
				style.Colors[ImGuiCol_ButtonHovered] = style.Colors[ImGuiCol_ButtonActive];
			}

			if (ImGui::Button(lebel[i], ImVec2(size.x + x_offset, size.y)))
			{
				if (cur_page != i && click_callback != nullptr)
					*click_callback = true;
				cur_page = i;
			}

			if (i == cur_page)
			{
				style.Colors[ImGuiCol_Button] = save_shit[0];
				style.Colors[ImGuiCol_ButtonHovered] = save_shit[1];
			}

			if (i != arr_ - 1)
				ImGui::SameLine();
		}
	}

	void set_color(bool white)
	{
		auto& style = ImGui::GetStyle();
		if (white)
		{
			style.Colors[ImGuiCol_FrameBg] = ImColor(200, 200, 200);
			style.Colors[ImGuiCol_FrameBgHovered] = ImColor(220, 220, 220);
			style.Colors[ImGuiCol_FrameBgActive] = ImColor(230, 230, 230);
			style.Colors[ImGuiCol_Separator] = ImColor(180, 180, 180);
			style.Colors[ImGuiCol_CheckMark] = ImColor(255, 172, 19);
			style.Colors[ImGuiCol_SliderGrab] = ImColor(255, 172, 19);
			style.Colors[ImGuiCol_SliderGrabActive] = ImColor(255, 172, 19);
			style.Colors[ImGuiCol_ScrollbarBg] = ImColor(120, 120, 120);
			style.Colors[ImGuiCol_ScrollbarGrab] = ImColor(255, 172, 19);
			style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarGrab);
			style.Colors[ImGuiCol_ScrollbarGrabActive] = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarGrab);
			style.Colors[ImGuiCol_Header] = ImColor(160, 160, 160);
			style.Colors[ImGuiCol_HeaderHovered] = ImColor(200, 200, 200);
			style.Colors[ImGuiCol_Button] = ImColor(180, 180, 180);
			style.Colors[ImGuiCol_ButtonHovered] = ImColor(200, 200, 200);
			style.Colors[ImGuiCol_ButtonActive] = ImColor(230, 230, 230);
			style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.78f, 0.78f, 0.78f, 1.f);
			style.Colors[ImGuiCol_WindowBg] = ImColor(220, 220, 220, 0.7 * 255);
			style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
			style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.40f, 0.40f, 0.80f, 0.20f);
			style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.40f, 0.40f, 0.55f, 0.80f);
			style.Colors[ImGuiCol_Border] = ImVec4(0.72f, 0.72f, 0.72f, 0.70f);
			style.Colors[ImGuiCol_TitleBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.83f);
			style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.75f, 0.75f, 0.75f, 0.87f);
			style.Colors[ImGuiCol_Text] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
			style.Colors[ImGuiCol_ChildBg] = ImVec4(0.72f, 0.72f, 0.72f, 0.76f);
			style.Colors[ImGuiCol_PopupBg] = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
			style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.81f, 0.81f, 0.81f, 1.00f);
			style.Colors[ImGuiCol_Tab] = ImVec4(0.61f, 0.61f, 0.61f, 0.79f);
			style.Colors[ImGuiCol_TabHovered] = ImVec4(0.71f, 0.71f, 0.71f, 0.80f);
			style.Colors[ImGuiCol_TabActive] = ImVec4(0.77f, 0.77f, 0.77f, 0.84f);
			style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.73f, 0.73f, 0.73f, 0.82f);
			style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.58f, 0.58f, 0.58f, 0.84f);
		}
		else
		{
			ImGui::StyleColorsClassic();
			style.Colors[ImGuiCol_FrameBg] = ImColor(3, 3, 3);
			style.Colors[ImGuiCol_FrameBgHovered] = ImColor(20, 20, 20);
			style.Colors[ImGuiCol_FrameBgActive] = ImColor(30, 30, 30);
			style.Colors[ImGuiCol_Separator] = ImColor(8, 8, 8);
			style.Colors[ImGuiCol_CheckMark] = ImColor(255, 172, 19);
			style.Colors[ImGuiCol_SliderGrab] = ImColor(255, 172, 19);
			style.Colors[ImGuiCol_SliderGrabActive] = ImColor(255, 172, 19);
			style.Colors[ImGuiCol_PopupBg] = ImColor(12, 12, 12);
			style.Colors[ImGuiCol_ScrollbarBg] = ImColor(12, 12, 12);
			style.Colors[ImGuiCol_ScrollbarGrab] = ImColor(255, 172, 19);
			style.Colors[ImGuiCol_ScrollbarGrab] = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarGrab);
			style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarGrab);
			style.Colors[ImGuiCol_ScrollbarGrabActive] = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarGrab);
			style.Colors[ImGuiCol_Border] = ImColor(12, 12, 12);
			style.Colors[ImGuiCol_ChildBg] = ImColor(16, 16, 16);
			style.Colors[ImGuiCol_Header] = ImColor(16, 16, 16);
			style.Colors[ImGuiCol_HeaderHovered] = ImColor(20, 20, 20);
			style.Colors[ImGuiCol_HeaderActive] = ImColor(30, 30, 30);
			style.Colors[ImGuiCol_Button] = ImColor(8, 8, 8);
			style.Colors[ImGuiCol_ButtonHovered] = ImColor(20, 20, 20);
			style.Colors[ImGuiCol_ButtonActive] = ImColor(30, 30, 30);
			style.Colors[ImGuiCol_Text] = ImColor(255, 255, 255);
			style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.78f, 0.78f, 0.78f, 1.f);
		}
	}
}

enum GLOBALS_PAGES
{
	PAGE_GLOBALS_MISC,
	PAGE_GLOBALS_CONFIG
};

void MenuRun()
{
	static int m_Page = 0;

	ImGui::SetNextWindowSizeConstraints(ImVec2(499.f, 399.f), ImVec2(999.f, 899.f));
	ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x - 22.f, ImGui::GetIO().DisplaySize.y - 65.f), ImGuiCond_Once);

	auto WndFlags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
	ImGui::Begin(WindowName, nullptr, WndFlags);

	menu_param::vWindowSize = ImGui::GetWindowSize();
	menu_param::vWindowPos = ImGui::GetWindowPos();

	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4);
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 17);

	ImGui::PushFont(menu_param::font_Main);
	ImGui::Text("alternative");
	ImGui::PopFont();

	ImGui::SameLine();

	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 17);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);

	const char* lebels[] = {
		"Misc", "Config"
	};

	menu_utils::tabs(lebels, IM_ARRAYSIZE(lebels), ImVec2(ImGui::GetContentRegionAvail().x / 2.f, 20), m_Page, -4.f);

	ImGui::BeginChild("1", ImVec2(0, ImGui::GetWindowSize().y - 60.f), true);

	if (m_Page == PAGE_GLOBALS_MISC)
	{
		ImGui::BeginChild("Misc", ImVec2(), true);

		ImGui::Checkbox("Unlimited energy", &vars::enable_infinity_energy);
		ImGui::SliderFloat("Set energy", &vars::my_energy, 0.f, 1000.f, "%.0f");
		ImGui::Checkbox("God mode", &vars::enable_god_mode);
		ImGui::SliderFloat("Set health", &vars::my_health, 0.f, 1000.f, "%.0f");
	
		std::string unlimited_ammo_text = vars::enable_unlimited_ammo ? "Disable unlimited ammo" : "Enable unlimited ammo";
		if (ImGui::Button(unlimited_ammo_text.c_str()))
		{
			vars::enable_unlimited_ammo = !vars::enable_unlimited_ammo;
			infinity_ammo(ammo_instruction_address, vars::enable_unlimited_ammo);
		}

		std::string no_recoil_text = vars::enable_no_recoil ? "Disable no recoil" : "Enable no recoil";
		if (ImGui::Button(no_recoil_text.c_str()))
		{
			vars::enable_no_recoil = !vars::enable_no_recoil;
			no_recoil(weapon_recoil_instruction_address, vars::enable_no_recoil);
		}

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.5f, 1.5f, 1.f));
		if (ImGui::Button("Quit", ImVec2(ImGui::GetWindowSize().x - 15.f, 40)))
			vars::unload_dll = true;
		ImGui::PopStyleColor();

		ImGui::EndChild();
	}
	else if (m_Page == PAGE_GLOBALS_CONFIG)
	{
		ImGui::BeginChild("Config", ImVec2(), true);

		ImGui::Text("Coming soon...");

		if (ImGui::Button("Color style"))
		{
			static bool isWhite = true;
			isWhite = !isWhite;
			menu_utils::set_color(isWhite);
		}

		ImGui::EndChild();
	}

	ImGui::EndChild();
	ImGui::PushFont(menu_param::font_Credits);
	ImGui::Text("Build date: %s, %s.", __DATE__, __TIME__);
	ImGui::SameLine();
	ImGui::SetCursorPosX(ImGui::GetWindowSize().x - ImGui::CalcTextSize("by zerrocxste").x - 15.f);
	ImGui::Text("by zerrocxste");
	ImGui::PopFont();

	ImGui::End();

	drawShadow();
}

namespace console
{
	FILE* output_stream = nullptr;

	void attach(const char* name)
	{
		if (AllocConsole())
		{
			freopen_s(&output_stream, "conout$", "w", stdout);
		}
		SetConsoleTitle(name);
	}

	void detach()
	{
		if (output_stream)
		{
			fclose(output_stream);
		}
		FreeConsole();
	}
}

namespace memory_utils
{
	#ifdef _WIN64
		#define DWORD_OF_BITNESS DWORD64
		#define PTRMAXVAL ((PVOID)0x000F000000000000)
	#elif _WIN32
		#define DWORD_OF_BITNESS DWORD
		#define PTRMAXVAL ((PVOID)0xFFF00000)
	#endif

	bool is_valid_ptr(PVOID ptr)
	{
		return (ptr >= (PVOID)0x10000) && (ptr < PTRMAXVAL) && ptr != nullptr && !IsBadReadPtr(ptr, sizeof(ptr));
	}

	DWORD_OF_BITNESS get_base()
	{
		return (DWORD_OF_BITNESS)GetModuleHandle(0);
	}

	template<class T>
	void write(std::vector<DWORD_OF_BITNESS>address, T value)
	{
		size_t lengh_array = address.size() - 1;
		DWORD_OF_BITNESS relative_address;
		relative_address = address[0];
		for (int i = 1; i < lengh_array + 1; i++)
		{
			if (is_valid_ptr((LPVOID)relative_address) == false)
				return;

			if (i < lengh_array)
				relative_address = *(DWORD_OF_BITNESS*)(relative_address + address[i]);
			else
			{
				T* writable_address = (T*)(relative_address + address[lengh_array]);
				*writable_address = value;
			}
		}
	}

	template<class T>
	T read(std::vector<DWORD_OF_BITNESS>address)
	{
		size_t lengh_array = address.size() - 1;
		DWORD_OF_BITNESS relative_address;
		relative_address = address[0];
		for (int i = 1; i < lengh_array + 1; i++)
		{
			if (is_valid_ptr((LPVOID)relative_address) == false)
				return 0;

			if (i < lengh_array)
				relative_address = *(DWORD_OF_BITNESS*)(relative_address + address[i]);
			else
			{
				T readable_address = *(T*)(relative_address + address[lengh_array]);
				return readable_address;
			}
		}
	}

	DWORD_OF_BITNESS get_module_size(DWORD_OF_BITNESS address)
	{
		return PIMAGE_NT_HEADERS(address + (DWORD_OF_BITNESS)PIMAGE_DOS_HEADER(address)->e_lfanew)->OptionalHeader.SizeOfImage;
	}

	DWORD_OF_BITNESS find_pattern(HMODULE module, const char* pattern, const char* mask)
	{
		DWORD_OF_BITNESS base = (DWORD_OF_BITNESS)module;
		DWORD_OF_BITNESS size = get_module_size(base);

		DWORD_OF_BITNESS patternLength = (DWORD_OF_BITNESS)strlen(mask);

		for (DWORD_OF_BITNESS i = 0; i < size - patternLength; i++)
		{
			bool found = true;
			for (DWORD_OF_BITNESS j = 0; j < patternLength; j++)
			{
				found &= mask[j] == '?' || pattern[j] == *(char*)(base + i + j);
			}

			if (found)
			{
				return base + i;
			}
		}

		return NULL;
	}

	void patch_instruction(DWORD_OF_BITNESS instruction_address, const char* instruction_bytes, int sizeof_instruction_byte)
	{
		DWORD dwOldProtection;

		VirtualProtect((LPVOID)instruction_address, sizeof_instruction_byte, PAGE_EXECUTE_READWRITE, &dwOldProtection);

		memcpy((LPVOID)instruction_address, instruction_bytes, sizeof_instruction_byte);

		VirtualProtect((LPVOID)instruction_address, sizeof_instruction_byte, dwOldProtection, NULL);

		FlushInstructionCache(GetCurrentProcess(), (LPVOID)instruction_address, sizeof_instruction_byte);
	}
}

void infinity_energy(DWORD cry_game_module)
{
	static std::initializer_list<DWORD>energy_ptr{ cry_game_module, 0x0029CD54, 0x2C, 0x44, 0x54, 0x3C, 0x34 };

	memory_utils::write(energy_ptr, vars::my_energy * 2.f);
}

void god_mode(DWORD cry_game_module)
{
	static std::initializer_list<DWORD>health_ptr{ cry_game_module, 0x0029CCDC, 0x2C, 0x18, 0x3C, 0x4, 0x40 };

	memory_utils::write(health_ptr, vars::my_health * 2.f);
}

void infinity_ammo(DWORD ammo_instruction_address, bool is_enable)
{
	if (is_enable)
	{
		memory_utils::patch_instruction(ammo_instruction_address, "\x90\x90\x90", 3);
	}
	else
	{
		memory_utils::patch_instruction(ammo_instruction_address, "\x89\x50\x14", 3);
	}
}

void no_recoil(DWORD weapon_recoil_instruction_address, bool is_enable)
{
	if (is_enable)
	{
		memory_utils::patch_instruction(weapon_recoil_instruction_address, "\x90\x90", 2);
	}
	else
	{
		memory_utils::patch_instruction(weapon_recoil_instruction_address, "\x89\x01", 2);
	}
}

void no_take_damage(DWORD health_take_instruction_address, bool is_enable)
{
	if (is_enable)
	{
		memory_utils::patch_instruction(health_take_instruction_address, "\x90\x90\x90\x90\x90", 5);
	}
	else
	{
		memory_utils::patch_instruction(health_take_instruction_address, "\xF3\x0F\x11\x46\x40", 5);
	}
}

ID3D10Device* device = nullptr;

HRESULT WINAPI Present_Hooked(IDXGISwapChain* swapchain, UINT sync_internal, UINT flags)
{
	static auto once = [swapchain, sync_internal, flags]() -> int
	{
		if (SUCCEEDED(swapchain->GetDevice(__uuidof(ID3D10Device), (void**)&device)))
		{
			ImGui::CreateContext();
			ImGui::StyleColorsClassic();

			auto& style = ImGui::GetStyle();

			style.FrameRounding = 3.f;
			style.ChildRounding = 3.f;
			style.ChildBorderSize = 1.f;
			style.ScrollbarSize = 0.6f;
			style.ScrollbarRounding = 3.f;
			style.GrabRounding = 3.f;
			style.WindowRounding = 0.f;

			style.Colors[ImGuiCol_FrameBg] = ImColor(200, 200, 200);
			style.Colors[ImGuiCol_FrameBgHovered] = ImColor(220, 220, 220);
			style.Colors[ImGuiCol_FrameBgActive] = ImColor(230, 230, 230);
			style.Colors[ImGuiCol_Separator] = ImColor(180, 180, 180);
			style.Colors[ImGuiCol_CheckMark] = ImColor(255, 172, 19);
			style.Colors[ImGuiCol_SliderGrab] = ImColor(255, 172, 19);
			style.Colors[ImGuiCol_SliderGrabActive] = ImColor(255, 172, 19);
			style.Colors[ImGuiCol_ScrollbarBg] = ImColor(120, 120, 120);
			style.Colors[ImGuiCol_ScrollbarGrab] = ImColor(255, 172, 19);
			style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarGrab);
			style.Colors[ImGuiCol_ScrollbarGrabActive] = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarGrab);
			style.Colors[ImGuiCol_Header] = ImColor(160, 160, 160);
			style.Colors[ImGuiCol_HeaderHovered] = ImColor(200, 200, 200);
			style.Colors[ImGuiCol_Button] = ImColor(180, 180, 180);
			style.Colors[ImGuiCol_ButtonHovered] = ImColor(200, 200, 200);
			style.Colors[ImGuiCol_ButtonActive] = ImColor(230, 230, 230);
			style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.78f, 0.78f, 0.78f, 1.f);
			style.Colors[ImGuiCol_WindowBg] = ImColor(220, 220, 220, 0.7 * 255);
			style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
			style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.40f, 0.40f, 0.80f, 0.20f);
			style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.40f, 0.40f, 0.55f, 0.80f);
			style.Colors[ImGuiCol_Border] = ImVec4(0.72f, 0.72f, 0.72f, 0.70f);
			style.Colors[ImGuiCol_TitleBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.83f);
			style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.75f, 0.75f, 0.75f, 0.87f);
			style.Colors[ImGuiCol_Text] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
			style.Colors[ImGuiCol_ChildBg] = ImVec4(0.72f, 0.72f, 0.72f, 0.76f);
			style.Colors[ImGuiCol_PopupBg] = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
			style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.81f, 0.81f, 0.81f, 1.00f);
			style.Colors[ImGuiCol_Tab] = ImVec4(0.61f, 0.61f, 0.61f, 0.79f);
			style.Colors[ImGuiCol_TabHovered] = ImVec4(0.71f, 0.71f, 0.71f, 0.80f);
			style.Colors[ImGuiCol_TabActive] = ImVec4(0.77f, 0.77f, 0.77f, 0.84f);
			style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.73f, 0.73f, 0.73f, 0.82f);
			style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.58f, 0.58f, 0.58f, 0.84f);

			auto& io = ImGui::GetIO();
			io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 15.0f, NULL, io.Fonts->GetGlyphRangesCyrillic());
			menu_param::font_Main = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 21.f);
			menu_param::font_Credits = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 15.f);
			ImGui_ImplWin32_Init(hGame);
			ImGui_ImplDX10_Init(device);
			ImGui_ImplDX10_CreateDeviceObjects();

			ImGuiWindowFlags flags_color_edit = ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_NoInputs;
			ImGui::SetColorEditOptions(flags_color_edit);

			std::cout << __FUNCTION__ << " first called!" << std::endl;
		}
		return true;
	}();

	auto draw_scene = []() -> void
	{
		if (vars::unload_dll)
			return;

		ImGui::NewFrame();

		ImGui_ImplDX10_NewFrame();
		ImGui_ImplWin32_NewFrame();

		if (vars::menu_open)
		{
			ImGui::GetIO().MouseDrawCursor = true;
			MenuRun();
		}
		else
		{
			ImGui::GetIO().MouseDrawCursor = false;
		}

		ImGui::EndFrame();

		ImGui::Render();

		ImGui_ImplDX10_RenderDrawData(ImGui::GetDrawData());
	};

	draw_scene();

	return pPresent(swapchain, sync_internal, flags);
}

void hook_dx10()
{
	IDXGIFactory* factory;

	if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory)))
	{
		std::cout << __FUNCTION__ << " > Error: failed create DXGI factory\n";
	}

	IDXGIAdapter* adapter;
	if (factory->EnumAdapters(0, &adapter) == DXGI_ERROR_NOT_FOUND)
	{
		std::cout << __FUNCTION__ << " > Error: enum adapters not found\n";
	}

	DXGI_SWAP_CHAIN_DESC scd{};
	ZeroMemory(&scd, sizeof(scd));
	scd.BufferCount = 1;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	scd.OutputWindow = hGame;
	scd.SampleDesc.Count = 1;
	scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	bool isWindowed = false; // kekw

	auto style = GetWindowLong(hGame, GWL_STYLE);

	bool isBORDERLESS = style & WS_POPUP;
	bool isFULLSCREEN_OR_WINDOWED = style & WS_CAPTION;
	bool isFULLSCREEN = style & WS_MINIMIZE;

	if (isBORDERLESS) {

		std::cout << "this shit is borderless\n";
		isWindowed = true;
	}
	else if (isFULLSCREEN_OR_WINDOWED) {
		if (isFULLSCREEN)
			std::cout << "this shit is fullscreen\n";
		else {
			std::cout << "this shit is windowed\n";
			isWindowed = true;
		}
	}

	scd.Windowed = isWindowed;
	scd.BufferDesc.RefreshRate.Numerator = 60;
	scd.BufferDesc.RefreshRate.Denominator = 1;

	IDXGISwapChain* swapchain = nullptr;

	if (FAILED(D3D10CreateDeviceAndSwapChain(adapter, D3D10_DRIVER_TYPE_HARDWARE, NULL, NULL, D3D10_SDK_VERSION, &scd, &swapchain, &device)))
	{
		std::cout << __FUNCTION__ << " > Error: failed create device\n";
	}

	void** pVTableSwapChain = *reinterpret_cast<void***>(swapchain);

	swapchain->Release();

	pPresentAddress = reinterpret_cast<LPVOID>(pVTableSwapChain[8]);

	if (MH_CreateHook(pPresentAddress, &Present_Hooked, (LPVOID*)&pPresent) != MH_OK)
	{
		std::cout << "failed create hook\n";
		return;
	}
	if (MH_EnableHook(pPresentAddress) != MH_OK)
	{
		std::cout << "failed enable hook\n";
		return;
	}
}

HRESULT WndProc_Hooked(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (Msg == WM_KEYDOWN && wParam == VK_INSERT)
		vars::menu_open = !vars::menu_open;

	if (vars::menu_open && ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam))
		return TRUE;

	return CallWindowProc(pWndProc, hWnd, Msg, wParam, lParam);
}

void start_hack()
{
	console::attach("crysis hax");

	std::system("cls");

	const char* you_are_welcome_message = {
		"\n"
		"Hello!\n"
		"hack for crysis version: 1.1.1.6115\n"
		"compilation date: %s\n"
		"hack function key:\n"
		"Inset - open menu\n"
		"\n"
	};

	char welcome_message[256];
	sprintf(welcome_message, you_are_welcome_message, __DATE__);

	std::cout << welcome_message << std::endl;

	hGame = FindWindow(NULL, "- Crysis DX10 - Feb 27 2008 (17:19:29)");

	if (hGame == NULL)
	{
		std::cout << __FUNCTION__ << " > Error find window\n";
		FreeLibraryAndExitThread(hMyModule, 1);
	}

	MH_Initialize();

	hook_dx10();

	pWndProc = (WNDPROC)SetWindowLong(hGame, GWL_WNDPROC, (LONG)&WndProc_Hooked);
	
	HMODULE cry_game_hmodule = GetModuleHandle("CryGame.dll");

	DWORD cry_game_module = (DWORD)cry_game_hmodule;

	std::cout << "CryGame.dll address: 0x" << cry_game_module << std::endl << std::endl;

	ammo_instruction_address = memory_utils::find_pattern(cry_game_hmodule, "\x89\x50\x14\xEB\x21", "xxxxx");

	weapon_recoil_instruction_address = memory_utils::find_pattern(cry_game_hmodule, "\x89\x01\x89\x51\x04\xE8", "xxxxxx");
	/*health_take_damage_address = memory_utils::find_pattern(cry_game_hmodule, "\xF3\x0F\x11\x46\x40\x7B", "xxxxxx");*/

	while (true)
	{
		if (vars::unload_dll)
			break;
   
		if (vars::enable_infinity_energy)
		{
			infinity_energy(cry_game_module);
		}

		if (vars::enable_god_mode)
		{
			god_mode(cry_game_module);
		}

		/*
		static bool pressed_no_take_damage_key = false;
		bool my_key_no_take_damage = GetAsyncKeyState('Q');
		if (my_key_no_take_damage && pressed_no_take_damage_key == false)
		{
			enable_no_take_damage = !enable_no_take_damage;
			pressed_no_take_damage_key = true;
			no_take_damage(health_take_damage_address, enable_no_take_damage);
			std::cout << "no take damage is: " << enable_no_take_damage << std::endl;
		}
		else if (my_key_no_take_damage == false)
		{
			pressed_no_take_damage_key = false;
		}*/

		Sleep(1);
	}

	vars::menu_open = false;
	Sleep(30);

	if (vars::enable_unlimited_ammo)
	{
		std::cout << "unlimited ammo bytes is patched, disable...\n";
		infinity_ammo(ammo_instruction_address, false);
	}
	
	if (vars::enable_no_recoil)
	{
		std::cout << "no recoil bytes is patched, disable...\n";
		no_recoil(weapon_recoil_instruction_address, false);
	}

	SetWindowLong(hGame, GWL_WNDPROC, (LONG)pWndProc);

	ImGui_ImplDX10_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	Sleep(100);

	MH_DisableHook(pPresentAddress);
	MH_DisableHook(pPresentAddress);
	Sleep(100);

	MH_Uninitialize();

	std::cout << "free library\n";
	FreeLibraryAndExitThread(hMyModule, 0);
}

BOOL APIENTRY DllMain( HMODULE hModule,
					   DWORD  ul_reason_for_call,
					   LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		hMyModule = hModule;
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)start_hack, 0, 0, 0);
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

