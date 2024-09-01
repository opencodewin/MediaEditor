#ifndef IMGUI_WIDGET_H
#define IMGUI_WIDGET_H

#include <functional>
#include <vector>
#include <algorithm>
#include <set>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_json.h>
#include <immat.h>

namespace ImGui {
enum ImGuiKnobType
{
    IMKNOB_TICK = 0,
    IMKNOB_TICK_DOT,
    IMKNOB_TICK_WIPER,
    IMKNOB_WIPER,
    IMKNOB_WIPER_TICK,
    IMKNOB_WIPER_DOT,
    IMKNOB_WIPER_ONLY,
    IMKNOB_STEPPED_TICK,
    IMKNOB_STEPPED_DOT,
    IMKNOB_SPACE,
};

struct ColorSet 
{
    ImVec4 base;
    ImVec4 hovered;
    ImVec4 active;
};

template<typename T, typename SIGNED_T>                     IMGUI_API T     RoundScalarWithFormatKnobT(const char* format, ImGuiDataType data_type, T v);
template<typename T, typename SIGNED_T, typename FLOAT_T>   IMGUI_API bool  SliderBehaviorKnobT(const ImRect& bb, ImGuiID id, ImGuiDataType data_type, T* v, T v_min, T v_max, const char* format, float power, ImGuiSliderFlags flags, ImRect* out_grab_bb);
template<typename T, typename FLOAT_T>                      IMGUI_API float SliderCalcRatioFromValueT(ImGuiDataType data_type, T v, T v_min, T v_max, float power, float linear_zero_pos);

IMGUI_API bool SliderBehavior(const ImRect& bb, ImGuiID id, ImGuiDataType data_type, void* p_v, const void* p_min, const void* p_max, const char* format, float power, ImGuiSliderFlags flags, ImRect* out_grab_bb);

IMGUI_API void UvMeter(char const *label, ImVec2 const &size, int *value, int v_min, int v_max, int steps = 10, int* stack = nullptr, int* count = nullptr, float background = 0.f, std::map<float, float> segment = {});
IMGUI_API void UvMeter(char const *label, ImVec2 const &size, float *value, float v_min, float v_max, int steps = 10, float* stack = nullptr, int* count = nullptr, float background = 0.f, std::map<float, float> segment = {});
IMGUI_API void UvMeter(ImDrawList *draw_list, char const *label, ImVec2 const &size, int *value, int v_min, int v_max, int steps = 10, int* stack = nullptr, int* count = nullptr, float background = 0.f, std::map<float, float> segment = {});
IMGUI_API void UvMeter(ImDrawList *draw_list, char const *label, ImVec2 const &size, float *value, float v_min, float v_max, int steps = 10, float* stack = nullptr, int* count = nullptr, float background = 0.f, std::map<float, float> segment = {});

IMGUI_API bool Knob(char const *label, float *p_value, float v_min, float v_max, float v_step, float v_default, float size,
                    ColorSet circle_color, ColorSet wiper_color, ColorSet track_color, ColorSet tick_color,
                    ImGuiKnobType type = IMKNOB_WIPER, char const *format = nullptr, int tick_steps = 0);

IMGUI_API bool Fader(const char* label, const ImVec2& size, int* v, const int v_min, const int v_max, const char* format = nullptr, float power = 1.0f);

IMGUI_API void RoundProgressBar(float radius, float *p_value, float v_min, float v_max, ColorSet bar_color, ColorSet progress_color, ColorSet text_color);

IMGUI_API bool HoverButton(ImDrawList *draw_list, const char * label, ImVec2 pos, ImVec2 size, std::string tooltips = "", ImVec4 hover_color = ImVec4(0.5f, 0.5f, 0.75f, 1.0f));

// Splitter
IMGUI_API bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f, float delay = 0.f);

// Based on the code from: https://github.com/benoitjacquier/imgui
IMGUI_API bool ColorCombo(const char* label,ImVec4 *pColorOut=NULL,bool supportsAlpha=false,float width=0.f,bool closeWhenMouseLeavesIt=true);
// Based on the code from: https://github.com/benoitjacquier/imgui
IMGUI_API bool ColorChooser(bool* open,ImVec4* pColorOut=NULL, bool supportsAlpha=true);

// ToggleButton
IMGUI_API bool ToggleButton(const char* str_id, bool* v);
IMGUI_API bool ToggleButton(const char *str_id, bool *v, const ImVec2 &size);
IMGUI_API bool BulletToggleButton(const char* label,bool* v, ImVec2 &pos, ImVec2 &size);

// RotateButton
IMGUI_API bool RotateButton(const char* label, const ImVec2& size_arg = ImVec2(0, 0), int rotate = 0);

// Input with int64
IMGUI_API bool InputInt64(const char* label, int64_t* v, int step = 1, int step_fast = 100, ImGuiInputTextFlags flags = 0);

// CheckButton
IMGUI_API bool CheckButton(const char* label, bool* pvalue, ImVec4 CheckButtonColor, bool useSmallButton = false, float checkedStateAlphaMult = 0.5f);

// RotateCheckButton
IMGUI_API bool RotateCheckButton(const char* label, bool* pvalue, ImVec4 CheckButtonColor, int rotate = 0, ImVec2 CheckButtonSize = ImVec2(0, 0), float checkedStateAlphaMult = 0.5f);

// ColoredButtonV1: code posted by @ocornut here: https://github.com/ocornut/imgui/issues/4722
// [Button rounding depends on the FrameRounding Style property (but can be overridden with the last argument)]
IMGUI_API bool ColoredButton(const char* label, const ImVec2& size, ImU32 text_color, ImU32 bg_color_1, ImU32 bg_color_2, float frame_rounding_override=-1.f, float disabled_brightness = 0.5f);

// new ProgressBar
// Please note that you can tweak the "format" argument if you want to add a prefix (or a suffix) piece of text to the text that appears at the right of the bar.
// returns the value "fraction" in 0.f-1.f.
// It does not need any ID.
IMGUI_API float ProgressBar(const char* optionalPrefixText,float value,const float minValue=0.f,const float maxValue=1.f,const char* format="%1.0f%%",const ImVec2& sizeOfBarWithoutTextInPixels=ImVec2(-1,-1),
                const ImVec4& colorLeft=ImVec4(0,1,0,0.8),const ImVec4& colorRight=ImVec4(0,0.4,0,0.8),const ImVec4& colorBorder=ImVec4(0.25,0.25,1.0,1));

// ProgressBar with 0 as center
IMGUI_API void ProgressBarPanning(float fraction, const ImVec2& size_arg = ImVec2(-FLT_MIN, 0));

// Buffering ProgressBar
IMGUI_API bool      BufferingBar(const char* label, float value, const ImVec2& size_arg, const float circle_pos, const ImU32& bg_col, const ImU32& fg_col);

// Spin
IMGUI_API bool      SpinScaler(const char* label, ImGuiDataType data_type, void* data_ptr, const void* step, const void* step_fast, const char* format, ImGuiInputTextFlags flags);
IMGUI_API bool      SpinInt(const char* label, int* v, int step = 1, int step_fast = 100, ImGuiInputTextFlags flags = 0);
IMGUI_API bool      SpinFloat(const char* label, float* v, float step = 0.0f, float step_fast = 0.0f, const char* format = "%.3f", ImGuiInputTextFlags flags = 0);
IMGUI_API bool      SpinDouble(const char* label, double* v, double step = 0.0, double step_fast = 0.0, const char* format = "%.6f", ImGuiInputTextFlags flags = 0);

// See date formats https://man7.org/linux/man-pages/man1/date.1.html
IMGUI_API bool InputDate(const char* label, struct tm& date, const char* format = "%d/%m/%Y", bool sunday_first = true, bool close_on_leave = true, const char* left_button = "<", const char* right_button = ">", const char* today_button = "o");
IMGUI_API bool InputTime(const char* label, struct tm& date, bool with_seconds = false, bool close_on_leave = true);
IMGUI_API bool InputDateTime(const char* label, struct tm& date, const char* dateformat = "%d/%m/%Y %H:%M", bool sunday_first = true, bool with_seconds = false, bool close_on_leave = true, const char* left_button = "<", const char* right_button = ">", const char* today_button = "o");

// new PlotEx
IMGUI_API int   PlotEx(ImGuiPlotType plot_type, const char* label, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 frame_size, bool b_tooltops = true, bool b_comband = false);
IMGUI_API void  PlotLinesEx(const char* label, const float* values, int values_count, int values_offset = 0, const char* overlay_text = NULL, float scale_min = FLT_MAX, float scale_max = FLT_MAX, ImVec2 graph_size = ImVec2(0, 0), int stride = sizeof(float), bool b_tooltips = true, bool b_comband = false);
IMGUI_API void  PlotLinesEx(const char* label, float(*values_getter)(void* data, int idx), void* data, int values_count, int values_offset = 0, const char* overlay_text = NULL, float scale_min = FLT_MAX, float scale_max = FLT_MAX, ImVec2 graph_size = ImVec2(0, 0), bool b_tooltips = true, bool b_comband = false);

IMGUI_API void  PlotMatEx(ImGui::ImMat& mat, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, float scale_min, float scale_max, ImVec2 frame_size, bool filled = false, bool b_fast = false, float thick = 0.5, ImVec2 offset = ImVec2(0, 0));
IMGUI_API void  PlotMat(ImGui::ImMat& mat, const float* values, int values_count, int values_offset, float scale_min, float scale_max, ImVec2 graph_size, int stride, bool b_comband = false, bool b_fast = false, float thick = 0.5);
IMGUI_API void  PlotMat(ImGui::ImMat& mat, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, float scale_min, float scale_max, ImVec2 graph_size, bool b_comband = false, bool b_fast = false, float thick = 0.5);
IMGUI_API void  PlotMat(ImGui::ImMat& mat, ImVec2 pos, const float* values, int values_count, int values_offset, float scale_min, float scale_max, ImVec2 graph_size, int stride, bool b_comband = false, bool b_fast = false, float thick = 0.5);
IMGUI_API void  PlotMat(ImGui::ImMat& mat, ImVec2 pos, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, float scale_min, float scale_max, ImVec2 graph_size, bool b_comband = false, bool b_fast = false, float thick = 0.5);

// new menu item
IMGUI_API bool  MenuItemEx(const char* label, const char* icon, const char* shortcut = NULL, bool selected = false, bool enabled = true, const char* subscript = nullptr);
IMGUI_API bool  MenuItem(const char* label, const char* shortcut, bool selected, bool enabled, const char* subscript);  

// new Drag for timestamp(millisecond)
IMGUI_API bool DragTimeMS(const char* label, float* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const int decimals = 3, ImGuiSliderFlags flags = 0);

// Slider 2D and Slider 3D 
IMGUI_API bool InputVec2(char const* pLabel, ImVec2* pValue, ImVec2 const vMinValue, ImVec2 const vMaxValue, float const fScale = 1.0f, bool bInput = true, bool bHandle = true);
IMGUI_API bool InputVec3(char const* pLabel, ImVec4* pValue, ImVec4 const vMinValue, ImVec4 const vMaxValue, float const fScale = 1.0f);
IMGUI_API bool SliderScalar2D(char const* pLabel, float* fValueX, float* fValueY, const float fMinX, const float fMaxX, const float fMinY, const float fMaxY, float const fZoom = 1.0f, bool bInput = true, bool bHandle = true);
IMGUI_API bool SliderScalar3D(char const* pLabel, float* pValueX, float* pValueY, float* pValueZ, float const fMinX, float const fMaxX, float const fMinY, float const fMaxY, float const fMinZ, float const fMaxZ, float const fScale = 1.0f);

IMGUI_API bool RangeSelect2D(char const* pLabel, float* pCurMinX, float* pCurMinY, float* pCurMaxX, float* pCurMaxY, float const fBoundMinX, float const fBoundMinY, float const fBoundMaxX, float const fBoundMaxY, float const fScale /*= 1.0f*/);
IMGUI_API bool RangeSelectVec2(const char* pLabel, ImVec2* pCurMin, ImVec2* pCurMax, ImVec2 const vBoundMin, ImVec2 const vBoundMax, float const fScale /*= 1.0f*/);

// Bezier Widget
IMGUI_API bool  BezierSelect(const char *label, const ImVec2 size, float P[5]);    // P[4] is curve presets(0 - 24)
IMGUI_API float BezierValue(float dt01, float P[4], int step = 0);

// Experimental: CheckboxFlags(...) overload to handle multiple flags with a single call
// returns the value of the pressed flag (not the index of the check box), or zero
// flagAnnotations, when!=0, just displays a circle in the required checkboxes
// itemHoveredOut, when used, returns the index of the hovered check box (not its flag), or -1.
// pFlagsValues, when used, must be numFlags long, and must contain the flag values (not the flag indices) that the control must use.
// KNOWN BUG: When ImGui::SameLine() is used after it, the alignment is broken
IMGUI_API unsigned int CheckboxFlags(const char* label,unsigned int* flags,int numFlags,int numRows,int numColumns,unsigned int flagAnnotations=0,int* itemHoveredOut=NULL,const unsigned int* pFlagsValues=NULL);
IMGUI_API unsigned int CheckboxFlags(const char* label,int* flags,int numFlags,int numRows,int numColumns,unsigned int flagAnnotations=0,int* itemHoveredOut=NULL,const unsigned int* pFlagsValues=NULL);

// Color Processing
// func: ImU32(*func)(float const x, float const y)
template < typename Type >
inline
Type	ScaleFromNormalized(Type const x, Type const newMin, Type const newMax)
{
	return x * (newMax - newMin) + newMin;
}
template < bool IsBilinear, typename FuncType >
inline
void DrawColorDensityPlotEx(ImDrawList* pDrawList, FuncType func, float minX, float maxX, float minY, float maxY, ImVec2 position, ImVec2 size, int resolutionX, int resolutionY)
{
	ImVec2 const uv = ImGui::GetFontTexUvWhitePixel();

	float const sx = size.x / ((float)resolutionX);
	float const sy = size.y / ((float)resolutionY);

	float const dy = 1.0f / ((float)resolutionY);
	float const dx = 1.0f / ((float)resolutionX);
	float const hdx = 0.5f / ((float)resolutionX);
	float const hdy = 0.5f / ((float)resolutionY);

	for (int i = 0; i < resolutionX; ++i)
	{
		float x0;
		float x1;
		if (IsBilinear)
		{
			x0 = ScaleFromNormalized(((float)i + 0) * dx, minX, maxX);
			x1 = ScaleFromNormalized(((float)i + 1) * dx, minX, maxX);
		}
		else
		{
			x0 = ScaleFromNormalized(((float)i + 0) * dx + hdx, minX, maxX);
		}

		for (int j = 0; j < resolutionY; ++j)
		{
			float y0;
			float y1;
			if (IsBilinear)
			{
				y0 = ScaleFromNormalized(((float)(j + 0) * dy), maxY, minY);
				y1 = ScaleFromNormalized(((float)(j + 1) * dy), maxY, minY);
			}
			else
			{
				y0 = ScaleFromNormalized(((float)(j + 0) * dy + hdy), maxY, minY);
			}

			ImU32 const col00 = func(x0, y0);
			if (IsBilinear)
			{
				ImU32 const col01 = func(x0, y1);
				ImU32 const col10 = func(x1, y0);
				ImU32 const col11 = func(x1, y1);
				pDrawList->AddRectFilledMultiColor(	position + ImVec2(sx * (i + 0), sy * (j + 0)),
													position + ImVec2(sx * (i + 1), sy * (j + 1)),
													col00, col10, col11, col01);
			}
			else
			{
				pDrawList->AddRectFilledMultiColor(	position + ImVec2(sx * (i + 0), sy * (j + 0)),
													position + ImVec2(sx * (i + 1), sy * (j + 1)),
													col00, col00, col00, col00);
			}
		}
	}
}
// func: ImU32(*func)(float const t)
template <bool IsBilinear, typename FuncType>
inline
void	DrawColorBandEx(ImDrawList* pDrawList, ImVec2 const vpos, ImVec2 const size, FuncType func, int division, float gamma)
{
	float const width = size.x;
	float const height = size.y;

	float const fSlice = static_cast<float>(division);

	ImVec2 dA(vpos);
	ImVec2 dB(vpos.x + width / fSlice, vpos.y + height);

	ImVec2 const dD(ImVec2(width / fSlice, 0));

	auto curColor =	[gamma, &func](float const x, float const)
					{
						return func(ImPow(x, gamma));
					};

	DrawColorDensityPlotEx< IsBilinear >(pDrawList, curColor, 0.0f, 1.0f, 0.0f, 0.0f, vpos, size, division, 1);
}
template <bool IsBilinear, typename FuncType>
inline
void	DrawColorRingEx(ImDrawList* pDrawList, ImVec2 const curPos, ImVec2 const size, float thickness_, FuncType func, int division, float colorOffset)
{
	float const radius = ImMin(size.x, size.y) * 0.5f;

	float const dAngle = -2.0f * IM_PI / ((float)division);
	float angle = 0; //2.0f * IM_PI / 3.0f;

	ImVec2 offset = curPos + ImVec2(radius, radius);
	if (size.x < size.y)
	{
		offset.y += 0.5f * (size.y - size.x);
	}
	else if (size.x > size.y)
	{
		offset.x += 0.5f * (size.x - size.y);
	}

	float const thickness = ImSaturate(thickness_) * radius;

	ImVec2 const uv = ImGui::GetFontTexUvWhitePixel();
	pDrawList->PrimReserve(division * 6, division * 4);
	for (int i = 0; i < division; ++i)
	{
		float x0 = radius * ImCos(angle);
		float y0 = radius * ImSin(angle);

		float x1 = radius * ImCos(angle + dAngle);
		float y1 = radius * ImSin(angle + dAngle);

		float x2 = (radius - thickness) * ImCos(angle + dAngle);
		float y2 = (radius - thickness) * ImSin(angle + dAngle);

		float x3 = (radius - thickness) * ImCos(angle);
		float y3 = (radius - thickness) * ImSin(angle);

		pDrawList->PrimWriteIdx((ImDrawIdx)(pDrawList->_VtxCurrentIdx));
		pDrawList->PrimWriteIdx((ImDrawIdx)(pDrawList->_VtxCurrentIdx + 1));
		pDrawList->PrimWriteIdx((ImDrawIdx)(pDrawList->_VtxCurrentIdx + 2));

		pDrawList->PrimWriteIdx((ImDrawIdx)(pDrawList->_VtxCurrentIdx));
		pDrawList->PrimWriteIdx((ImDrawIdx)(pDrawList->_VtxCurrentIdx + 2));
		pDrawList->PrimWriteIdx((ImDrawIdx)(pDrawList->_VtxCurrentIdx + 3));

		float const t0 = fmodf(colorOffset + ((float)i) / ((float)division), 1.0f);
		ImU32 const uCol0 = func(t0);

		if (IsBilinear)
		{
			float const t1 = fmodf(colorOffset + ((float)(i + 1)) / ((float)division), 1.0f);
			ImU32 const uCol1 = func(t1);
			pDrawList->PrimWriteVtx(offset + ImVec2(x0, y0), uv, uCol0);
			pDrawList->PrimWriteVtx(offset + ImVec2(x1, y1), uv, uCol1);
			pDrawList->PrimWriteVtx(offset + ImVec2(x2, y2), uv, uCol1);
			pDrawList->PrimWriteVtx(offset + ImVec2(x3, y3), uv, uCol0);
		}
		else
		{
			pDrawList->PrimWriteVtx(offset + ImVec2(x0, y0), uv, uCol0);
			pDrawList->PrimWriteVtx(offset + ImVec2(x1, y1), uv, uCol0);
			pDrawList->PrimWriteVtx(offset + ImVec2(x2, y2), uv, uCol0);
			pDrawList->PrimWriteVtx(offset + ImVec2(x3, y3), uv, uCol0);
		}
		angle += dAngle;
	}
}

IMGUI_API void DrawHueBand(ImVec2 const vpos, ImVec2 const size, int division, float alpha, float gamma, float offset = 0.0f);
IMGUI_API void DrawHueBand(ImVec2 const vpos, ImVec2 const size, int division, float colorStartRGB[3], float alpha, float gamma);
IMGUI_API void DrawLumianceBand(ImVec2 const vpos, ImVec2 const size, int division, ImVec4 const& color, float gamma);
IMGUI_API void DrawSaturationBand(ImVec2 const vpos, ImVec2 const size, int division, ImVec4 const& color, float gamma);
IMGUI_API void DrawContrastBand(ImVec2 const vpos, ImVec2 const size, ImVec4 const& color);
IMGUI_API bool ColorRing(const char* label, float thickness, int split);

// Color Selector
IMGUI_API void HueSelector(char const* label, ImVec2 const size, float* hueCenter, float* hueWidth, float* featherLeft, float* featherRight, float defaultVal, float ui_zoom = 1.0f, int division = 32, float alpha = 1.0f, float hideHueAlpha = 0.75f, float offset = 0.0f);
IMGUI_API void LumianceSelector(char const* label, ImVec2 const size, float* lumCenter, float defaultVal, float vmin, float vmax, float ui_zoom = 1.0f, int division = 32, float gamma = 1.f, bool rgb_color = false, ImVec4 const color = ImVec4(1, 1, 1, 1));
IMGUI_API void GammaSelector(char const* label, ImVec2 const size, float* gammaCenter, float defaultVal, float vmin, float vmax, float ui_zoom = 1.0f, int division = 32);
IMGUI_API void SaturationSelector(char const* label, ImVec2 const size, float* satCenter, float defaultVal, float vmin, float vmax, float ui_zoom = 1.0f, int division = 32, float gamma = 1.f, bool rgb_color = false, ImVec4 const color = ImVec4(1, 1, 1, 1));
IMGUI_API void ContrastSelector(char const* label, ImVec2 const size, float* conCenter, float defaultVal, float ui_zoom = 1.0f, bool rgb_color = false, ImVec4 const color = ImVec4(1, 1, 1, 1));
IMGUI_API void TemperatureSelector(char const* label, ImVec2 const size, float* tempCenter, float defaultVal, float vmin, float vmax, float ui_zoom = 1.0f, int division = 32);
IMGUI_API bool BalanceSelector(char const* label, ImVec2 const size, ImVec4 * rgba, ImVec4 defaultVal, ImVec2* offset = nullptr, float ui_zoom = 1.0f, float speed = 1.0f, int division = 128, float thickness = 1.0f, float colorOffset = 0);

//////////////////////////////////////////////////////////////////////////
// Interactions
//////////////////////////////////////////////////////////////////////////
IMGUI_API bool IsPolyConvexContains(const std::vector<ImVec2>& pts, ImVec2 p);
IMGUI_API bool IsPolyConcaveContains(const std::vector<ImVec2>& pts, ImVec2 p);
IMGUI_API bool IsPolyWithHoleContains(const std::vector<ImVec2>& pts, ImVec2 p, ImRect* p_bb = NULL, int gap = 1, int strokeWidth = 1);

IMGUI_API bool IsMouseHoveringPolyConvex( const ImVec2& r_min, const ImVec2& r_max, const std::vector<ImVec2>& pts, bool clip = true );
IMGUI_API bool ItemHoverablePolyConvex( const ImRect& bb, ImGuiID id, const std::vector<ImVec2>& pts, ImGuiItemFlags item_flags );
IMGUI_API bool IsMouseHoveringPolyConcave( const ImVec2& r_min, const ImVec2& r_max, const std::vector<ImVec2>& pts, bool clip = true );
IMGUI_API bool ItemHoverablePolyConcave( const ImRect& bb, ImGuiID id, const std::vector<ImVec2>& pts, ImGuiItemFlags item_flags );
IMGUI_API bool IsMouseHoveringPolyWithHole( const ImVec2& r_min, const ImVec2& r_max, const std::vector<ImVec2>& pts, bool clip = true );
IMGUI_API bool ItemHoverablePolyWithHole( const ImRect& bb, ImGuiID id, const std::vector<ImVec2>& pts, ImGuiItemFlags item_flags );

IMGUI_API void DrawShapeWithHole( ImDrawList* draw, const std::vector<ImVec2>& poly, ImU32 color, ImRect* p_bb = NULL, int gap = 1, int strokeWidth = 1 );
IMGUI_API void DrawTriangleCursor( ImDrawList* pDrawList, ImVec2 targetPoint, float angle, float size, float thickness, ImU32 col );
IMGUI_API void DrawTriangleCursorFilled( ImDrawList* pDrawList, ImVec2 targetPoint, float angle, float size, ImU32 col );
IMGUI_API void DrawSignetCursor( ImDrawList* pDrawList, ImVec2 targetPoint, float width, float height, float height_ratio, float align01, float angle, float thickness, ImU32 col );
IMGUI_API void DrawSignetFilledCursor( ImDrawList* pDrawList, ImVec2 targetPoint, float width, float height, float height_ratio, float align01, float angle, ImU32 col );
IMGUI_API void SetCurrentWindowBackgroundImage( ImTextureID id, ImVec2 imgSize, bool fixedSize = false, ImU32 col = IM_COL32( 255, 255, 255, 255 ) );

IMGUI_API void DrawLinearLineGraduation( ImDrawList* drawlist, ImVec2 start, ImVec2 end,
										float mainLineThickness, ImU32 mainCol,
										int division0, float height0, float thickness0, float angle0, ImU32 col0,
										int division1 = -1, float height1 = -1.0f, float thickness1 = -1.0f, float angle1 = -1.0f, ImU32 col1 = 0u,
										int division2 = -1, float height2 = -1.0f, float thickness2 = -1.0f, float angle2 = -1.0f, ImU32 col2 = 0u );

IMGUI_API void DrawLogLineGraduation( ImDrawList* drawlist, ImVec2 start, ImVec2 end,
									float mainLineThickness, ImU32 mainCol,
									int division0, float height0, float thickness0, float angle0, ImU32 col0,
									int division1 = -1, float height1 = -1.0f, float thickness1 = -1.0f, float angle1 = -1.0f, ImU32 col1 = 0u );

IMGUI_API void DrawLinearCircularGraduation( ImDrawList* drawlist, ImVec2 center, float radius, float start_angle, float end_angle, int num_segments,
											float mainLineThickness, ImU32 mainCol,
											int division0, float height0, float thickness0, float angle0, ImU32 col0,
											int division1 = -1, float height1 = -1.0f, float thickness1 = -1.0f, float angle1 = -1.0f, ImU32 col1 = 0u,
											int division2 = -1, float height2 = -1.0f, float thickness2 = -1.0f, float angle2 = -1.0f, ImU32 col2 = 0u );

IMGUI_API void DrawLogCircularGraduation( ImDrawList* drawlist, ImVec2 center, float radius, float start_angle, float end_angle, int num_segments,
										float mainLineThickness, ImU32 mainCol,
										int division0, float height0, float thickness0, float angle0, ImU32 col0,
										int division1 = -1, float height1 = -1.0f, float thickness1 = -1.0f, float angle1 = -1.0f, ImU32 col1 = 0u );

// https://github.com/CedricGuillemet/imgInspect
/*
//example
Image pickerImage;
ImGui::ImageButton(pickerImage.textureID, ImVec2(pickerImage.mWidth, pickerImage.mHeight));
ImRect rc = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
ImVec2 mouseUVCoord = (io.MousePos - rc.Min) / rc.GetSize();
mouseUVCoord.y = 1.f - mouseUVCoord.y;
if (io.KeyShift && io.MouseDown[0] && mouseUVCoord.x >= 0.f && mouseUVCoord.y >= 0.f)
{
        int width = pickerImage.mWidth;
        int height = pickerImage.mHeight;

        ImGuiHelper::ImageInspect(width, height, pickerImage.GetBits(), mouseUVCoord, displayedTextureSize);
}
*/
IMGUI_API void ImageInspect(const int width,
                            const int height,
                            const unsigned char* const bits,
                            ImVec2 mouseUVCoord,
                            ImVec2 displayedTextureSize,
                            bool histogram_full = false,
                            int zoom_size = 8);

// Show Digital number
IMGUI_API void ShowDigitalTime(ImDrawList *draw_list, int64_t millisec, int show_millisec, ImVec2 pos, ImU32 color);
IMGUI_API void ShowDigitalTimeDuration(ImDrawList *draw_list, int64_t millisec, int64_t duration, int show_millisec, ImVec2 pos, ImU32 color);
IMGUI_API void ShowDigitalDateTime(ImDrawList *draw_list, int64_t millisec, int show_millisec, ImVec2 pos, ImU32 color);

// Rainbow Text
IMGUI_API void RainbowText(const char* text);
// Demo Window
#if IMGUI_BUILD_EXAMPLE
IMGUI_API void ShowExtraWidgetDemoWindow();
IMGUI_API void ShowImKalmanDemoWindow();
IMGUI_API void ShowImFFTDemoWindow();
IMGUI_API void ShowImSTFTDemoWindow();
#if IMGUI_VULKAN_SHADER
IMGUI_API void  ImVulkanTestWindow(const char* name, bool* p_open, ImGuiWindowFlags flags);
#endif
#endif
} // namespace ImGui

namespace ImGui
{
// Extensions to ImDrawList
IMGUI_API void AddConvexPolyFilledWithVerticalGradient(ImDrawList* dl, const ImVec2* points, const int points_count, ImU32 colTop, ImU32 colBot, float miny=-1.f, float maxy=-1.f);
IMGUI_API void PathFillWithVerticalGradientAndStroke(ImDrawList* dl, const ImU32& fillColorTop, const ImU32& fillColorBottom, const ImU32& strokeColor, bool strokeClosed=false, float strokeThickness = 1.0f, float miny=-1.f, float maxy=-1.f);
IMGUI_API void PathFillAndStroke(ImDrawList* dl,const ImU32& fillColor,const ImU32& strokeColor,bool strokeClosed=false, float strokeThickness = 1.0f);
IMGUI_API void AddRect(ImDrawList* dl,const ImVec2& a, const ImVec2& b,const ImU32& fillColor,const ImU32& strokeColor,float rounding = 0.0f, int rounding_corners = 0,float strokeThickness = 1.0f);
IMGUI_API void AddRectWithVerticalGradient(ImDrawList* dl,const ImVec2& a, const ImVec2& b,const ImU32& fillColorTop,const ImU32& fillColorBottom,const ImU32& strokeColor,float rounding = 0.0f, int rounding_corners = 0,float strokeThickness = 1.0f);
IMGUI_API void AddRectWithVerticalGradient(ImDrawList* dl,const ImVec2& a, const ImVec2& b,const ImU32& fillColor,float fillColorGradientDeltaIn0_05,const ImU32& strokeColor,float rounding = 0.0f, int rounding_corners = 0,float strokeThickness = 1.0f);
IMGUI_API void PathArcTo(ImDrawList* dl,const ImVec2& centre,const ImVec2& radii, float amin, float amax, int num_segments = 10);
IMGUI_API void AddEllipse(ImDrawList* dl,const ImVec2& centre, const ImVec2& radii,const ImU32& fillColor,const ImU32& strokeColor,int num_segments = 12,float strokeThickness = 1.f);
IMGUI_API void AddEllipseWithVerticalGradient(ImDrawList* dl, const ImVec2& centre, const ImVec2& radii, const ImU32& fillColorTop, const ImU32& fillColorBottom, const ImU32& strokeColor, int num_segments = 12, float strokeThickness = 1.f);
IMGUI_API void AddCircle(ImDrawList* dl,const ImVec2& centre, float radius,const ImU32& fillColor,const ImU32& strokeColor,int num_segments = 12,float strokeThickness = 1.f);
IMGUI_API void AddCircleWithVerticalGradient(ImDrawList* dl, const ImVec2& centre, float radius, const ImU32& fillColorTop, const ImU32& fillColorBottom, const ImU32& strokeColor, int num_segments = 12, float strokeThickness = 1.f);
// Overload of ImDrawList::addPolyLine(...) that takes offset and scale:
IMGUI_API void AddPolyLine(ImDrawList *dl,const ImVec2* polyPoints,int numPolyPoints,ImU32 strokeColor=IM_COL32_WHITE,float strokeThickness=1.f,bool strokeClosed=false, const ImVec2 &offset=ImVec2(0,0), const ImVec2& scale=ImVec2(1,1));
IMGUI_API void AddConvexPolyFilledWithHorizontalGradient(ImDrawList *dl, const ImVec2 *points, const int points_count, ImU32 colLeft, ImU32 colRight, float minx=-1.f, float maxx=-1.f);
IMGUI_API void PathFillWithHorizontalGradientAndStroke(ImDrawList *dl, const ImU32 &fillColorLeft, const ImU32 &fillColorRight, const ImU32 &strokeColor, bool strokeClosed=false, float strokeThickness = 1.0f, float minx=-1.f,float maxx=-1.f);
IMGUI_API void AddRectWithHorizontalGradient(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, const ImU32 &fillColorLeft, const ImU32 &fillColoRight, const ImU32 &strokeColor, float rounding = 0.0f, int rounding_corners = 0, float strokeThickness = 1.0f);
IMGUI_API void AddEllipseWithHorizontalGradient(ImDrawList *dl, const ImVec2 &centre, const ImVec2 &radii, const ImU32 &fillColorLeft, const ImU32 &fillColorRight, const ImU32 &strokeColor, int num_segments = 12, float strokeThickness = 1.0f);
IMGUI_API void AddCircleWithHorizontalGradient(ImDrawList *dl, const ImVec2 &centre, float radius, const ImU32 &fillColorLeft, const ImU32 &fillColorRight, const ImU32 &strokeColor, int num_segments = 12, float strokeThickness = 1.0f);
IMGUI_API void AddRectWithHorizontalGradient(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, const ImU32 &fillColor, float fillColorGradientDeltaIn0_05, const ImU32 &strokeColor, float rounding = 0.0f, int rounding_corners = 0, float strokeThickness = 1.0f);
// Add Dashed line or circle
IMGUI_API void AddLineDashed(ImDrawList *dl, const ImVec2& a, const ImVec2& b, ImU32 col, float thickness = 1.0f, unsigned int segments = 10, unsigned int on_segments = 1, unsigned int off_segments = 1);
IMGUI_API void AddCircleDashed(ImDrawList *dl, const ImVec2& centre, float radius, ImU32 col, int num_segments = 12, float thickness = 1.0f, int on_segments = 1, int off_segments = 1);
IMGUI_API void PathArcToDashedAndStroke(ImDrawList *dl, const ImVec2& centre, float radius, float a_min, float a_max, ImU32 col, float thickness = 1.0f, int num_segments = 10, int on_segments = 1, int off_segments = 1);
// Add Rotate Image
IMGUI_API void AddImageRotate(ImDrawList *dl, ImTextureID tex_id, ImVec2 pos, ImVec2 size, float angle, ImU32 board_col = IM_COL32(0, 0, 0, 255));
} // namespace ImGui

namespace ImGui
{
// Vertical Text Helper
IMGUI_API ImVec2    CalcVerticalTextSize(const char* text, const char* text_end = NULL, bool hide_text_after_double_hash = false, float wrap_width = -1.0f);
IMGUI_API void      RenderTextVertical(const ImFont* font,ImDrawList* draw_list, float size, ImVec2 pos, ImU32 col, const ImVec4& clip_rect, const char* text_begin,  const char* text_end = NULL, float wrap_width = 0.0f, bool cpu_fine_clip = false, bool rotateCCW = false, bool char_no_rotate = false);
IMGUI_API void      AddTextVertical(ImDrawList* drawList,const ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end = NULL, float wrap_width = 0.0f, const ImVec4* cpu_fine_clip_rect = NULL, bool rotateCCW = false, bool char_no_rotate = false);
IMGUI_API void      AddTextVertical(ImDrawList* drawList,const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end = NULL,bool rotateCCW = false, bool char_no_rotate = false);
IMGUI_API void      RenderTextVerticalClipped(const ImVec2& pos_min, const ImVec2& pos_max, const char* text, const char* text_end, const ImVec2* text_size_if_known, const ImVec2& align = ImVec2(0.0f,0.0f), const ImVec2* clip_min = NULL, const ImVec2* clip_max = NULL, bool rotateCCW = false, bool char_no_rotate = false);
} //namespace ImGui

namespace ImGui
{
// Roll Text Helper
IMGUI_API void      AddTextRolling(ImDrawList* drawList, const ImFont* font, float font_size, const ImVec2& pos, const ImVec2& size, ImU32 col, const int speed, const char* text_begin, const char* text_end = NULL);
IMGUI_API void      AddTextRolling(const char* text, const ImVec2& size, const ImVec2& pos, const int speed = 10);
IMGUI_API void      AddTextRolling(const char* text, const ImVec2& size, const int speed = 10);
} //namespace ImGui

namespace ImGui
{
// Posted by @alexsr here: https://github.com/ocornut/imgui/issues/1901
// Sligthly modified to provide default behaviour with default args
IMGUI_API void      LoadingIndicatorCircle(const char* label, float indicatorRadiusFactor=1.f,
                                        const ImVec4* pOptionalMainColor=NULL, const ImVec4* pOptionalBackdropColor=NULL,
                                        int circle_count=8, const float speed=1.f);
// Posted by @zfedoran here: https://github.com/ocornut/imgui/issues/1901
// Sligthly modified to provide default behaviour with default args
IMGUI_API void      LoadingIndicatorCircle2(const char* label, float indicatorRadiusFactor=1.f, float indicatorRadiusThicknessFactor=1.f, const ImVec4* pOptionalColor=NULL);
} // namespace ImGui

namespace ImGui
{
class IMGUI_API Piano {
public:
    int key_states[256] = {0};
    void up(int key);
    void draw_keyboard(ImVec2 size, bool input = false);
    void down(int key, int velocity);
    void reset();
};
} // namespace ImGui

namespace ImGui
{

IMGUI_API void  ImSpectrogram(const ImMat& in_mat, ImMat& out_mat, int window = 512, bool stft = false, int hope = 128);

} // namespace ImGui

// custom draw leader
namespace ImGui
{
IMGUI_API void Circle(bool filled, bool arrow = false);
IMGUI_API void Square(bool filled, bool arrow = false);
IMGUI_API void BracketSquare(bool filled, bool arrow = false);
IMGUI_API void RoundSquare(bool filled, bool arrow = false);
IMGUI_API void GridSquare(bool filled, bool arrow = false);
IMGUI_API void Diamond(bool filled, bool arrow = false);
} // namespace ImGui

// custom badge draw button
namespace ImGui
{
IMGUI_API bool CircleButton(const char* id_str, bool filled, bool arrow = false);
IMGUI_API bool SquareButton(const char* id_str, bool filled, bool arrow = false);
IMGUI_API bool BracketSquareButton(const char* id_str, bool filled, bool arrow = false);
IMGUI_API bool RoundSquareButton(const char* id_str, bool filled, bool arrow = false);
IMGUI_API bool GridSquareButton(const char* id_str, bool filled, bool arrow = false);
IMGUI_API bool DiamondButton(const char* id_str, bool filled, bool arrow = false);
} // namespace ImGui

namespace ImGui
{
// Set of nice spinners for imgui
// https://github.com/dalerank/imspinner

#define DECLPROP(name, type, def) \
    struct name { \
        type value = def; \
        operator type() { return value; } \
        name(const type& v) : value(v) {} \
    };

    enum SpinnerTypeT {
        e_st_rainbow = 0,
        e_st_angle,
        e_st_dots,
        e_st_ang,
		e_st_vdots,
		e_st_bounce_ball,
		e_st_eclipse,
		e_st_ingyang,
        e_st_barchartsine,

        e_st_count
    };

    using float_ptr = float *;
    constexpr float PI_DIV_4 = IM_PI / 4.f;
    constexpr float PI_DIV_2 = IM_PI / 2.f;
    constexpr float PI_2 = IM_PI * 2.f;
    template<class T> constexpr float PI_DIV(T d) { return IM_PI / (float)d; }
    template<class T> constexpr float PI_2_DIV(T d) { return PI_2 / (float)d; }

    DECLPROP (SpinnerType, SpinnerTypeT, e_st_rainbow)
    DECLPROP (Radius, float, 16.f)
    DECLPROP (Speed, float, 1.f)
    DECLPROP (Thickness, float, 1.f)
    DECLPROP (Color, ImColor, 0xffffffff)
    DECLPROP (BgColor, ImColor, 0xffffffff)
	DECLPROP (AltColor, ImColor, 0xffffffff)
    DECLPROP (Angle, float, IM_PI)
	DECLPROP (AngleMin, float, IM_PI)
    DECLPROP (AngleMax, float, IM_PI)
    DECLPROP (FloatPtr, float_ptr, nullptr)
    DECLPROP (Dots, int, 0)
    DECLPROP (MiddleDots, int, 0)
    DECLPROP (MinThickness, float, 0.f)
	DECLPROP (Reverse, bool, false)
    DECLPROP (Delta, float, 0.f)
    DECLPROP (Mode, int, 0)
#undef DECLPROP

#define IMPLRPOP(basetype,type) basetype m_##type; \
                                void set##type(const basetype& v) { m_##type = v;} \
                                void set(type h) { m_##type = h.value;} \
                                template<typename First, typename... Args> \
                                void set(const type& h, const Args&... args) { set##type(h.value); this->template set<Args...>(args...); }
    struct SpinnerConfig {
        SpinnerConfig() {}
        template<typename none = void> void set() {}

        template<typename... Args>
        SpinnerConfig(const Args&... args) { this->template set<Args...>(args...); }

        IMPLRPOP(SpinnerTypeT, SpinnerType)
        IMPLRPOP(float, Radius)
        IMPLRPOP(float, Speed)
        IMPLRPOP(float, Thickness)
        IMPLRPOP(ImColor, Color)
        IMPLRPOP(ImColor, BgColor)
		IMPLRPOP(ImColor, AltColor)
        IMPLRPOP(float, Angle)
		IMPLRPOP(float, AngleMin)
        IMPLRPOP(float, AngleMax)
        IMPLRPOP(float_ptr, FloatPtr)
        IMPLRPOP(int, Dots)
        IMPLRPOP(int, MiddleDots)
        IMPLRPOP(float, MinThickness)
		IMPLRPOP(bool, Reverse)
        IMPLRPOP(float, Delta)
        IMPLRPOP(int, Mode)
    };
#undef IMPLRPOP

using LeafColor = ImColor (int);
IMGUI_API void SpinnerRainbow(const char *label, float radius, float thickness, const ImColor &color, float speed, float ang_min = 0.f, float ang_max = PI_2, int arcs = 1, int mode = 0);
IMGUI_API void SpinnerRainbowMix(const char *label, float radius, float thickness, const ImColor &color, float speed, float ang_min = 0.f, float ang_max = PI_2, int arcs = 1, int mode = 0);
IMGUI_API void SpinnerRotatingHeart(const char *label, float radius, float thickness, const ImColor &color, float speed, float ang_min = 0.f);
IMGUI_API void SpinnerAng(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, const ImColor &bg = 0xffffff80, float speed = 2.8f, float angle = IM_PI, int mode = 0);
IMGUI_API void SpinnerAng8(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, const ImColor &bg = 0xffffffff, float speed = 2.8f, float angle = IM_PI, int mode = 0, float rkoef = 0.5f);
IMGUI_API void SpinnerAngMix(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, float angle = IM_PI, int arcs = 4, int mode = 0);
IMGUI_API void SpinnerLoadingRing(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, const ImColor &bg = 0xffffff80, float speed = 2.8f, int segments = 5);
IMGUI_API void SpinnerClock(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, const ImColor &bg = 0xffffff80, float speed = 2.8f);
IMGUI_API void SpinnerPulsar(const char *label, float radius, float thickness, const ImColor &bg = 0xffffff80, float speed = 2.8f, bool sequence = true, float angle = 0.f, int mode = 0);
IMGUI_API void SpinnerDoubleFadePulsar(const char *label, float radius, float /*thickness*/, const ImColor &bg = 0xffffff80, float speed = 2.8f);
IMGUI_API void SpinnerTwinPulsar(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int rings = 2, int mode = 0);
IMGUI_API void SpinnerFadePulsar(const char *label, float radius, const ImColor &color = 0xffffffff, float speed = 2.8f, int rings = 2, int mode = 0);
IMGUI_API void SpinnerCircularLines(const char *label, float radius, const ImColor &color = 0xffffffff, float speed = 1.8f, int lines = 8, int mode = 0);
IMGUI_API void SpinnerDots(const char *label, float *nextdot, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t dots = 12, float minth = -1.f, int mode = 0);
IMGUI_API void SpinnerVDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, const ImColor &bgcolor = 0xffffffff, float speed = 2.8f, size_t dots = 12, size_t mdots = 6, int mode = 0);
IMGUI_API void SpinnerBounceDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t dots = 3, int mode = 0);
IMGUI_API void SpinnerZipDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t dots = 5);
IMGUI_API void SpinnerDotsToPoints(const char *label, float radius, float thickness, float offset_k, const ImColor &color = 0xffffffff, float speed = 1.8f, size_t dots = 5);
IMGUI_API void SpinnerDotsToBar(const char *label, float radius, float thickness, float offset_k, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t dots = 5);
IMGUI_API void SpinnerWaveDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int lt = 8);
IMGUI_API void SpinnerFadeDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int lt = 8, int mode = 0);
IMGUI_API void SpinnerThreeDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int lt = 8);
IMGUI_API void SpinnerFiveDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int lt = 8);
IMGUI_API void Spinner4Caleidospcope(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int lt = 8);
IMGUI_API void SpinnerMultiFadeDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int lt = 8);
IMGUI_API void SpinnerScaleDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int lt = 8);
IMGUI_API void SpinnerSquareSpins(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f);
IMGUI_API void SpinnerMovingDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t dots = 3);
IMGUI_API void SpinnerRotateDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int dots = 2, int mode = 0);
IMGUI_API void SpinnerOrionDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int arcs = 4);
IMGUI_API void SpinnerGalaxyDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int arcs = 4);
IMGUI_API void SpinnerTwinAng(const char *label, float radius1, float radius2, float thickness, const ImColor &color1 = 0xffffffff, const ImColor &color2 = 0xff0000ff, float speed = 2.8f, float angle = IM_PI, int mode = 0);
IMGUI_API void SpinnerFilling(const char *label, float radius, float thickness, const ImColor &color1 = 0xffffffff, const ImColor &color2 = 0xff0000ff, float speed = 2.8f);
IMGUI_API void SpinnerFillingMem(const char *label, float radius, float thickness, const ImColor &color, ImColor &colorbg, float speed);
IMGUI_API void SpinnerTopup(const char *label, float radius1, float radius2, const ImColor &color = 0xff0000ff, const ImColor &fg = 0xffffffff, const ImColor &bg = 0xffffffff, float speed = 2.8f);
IMGUI_API void SpinnerTwinAng180(const char *label, float radius1, float radius2, float thickness, const ImColor &color1 = 0xffffffff, const ImColor &color2 = 0xff0000ff, float speed = 2.8f, float angle = PI_DIV_4, int mode = 0);
IMGUI_API void SpinnerTwinAng360(const char *label, float radius1, float radius2, float thickness, const ImColor &color1 = 0xffffffff, const ImColor &color2 = 0xff0000ff, float speed1 = 2.8f, float speed2 = 2.5f, int mode = 0);
IMGUI_API void SpinnerIncDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t dots = 6);
IMGUI_API void SpinnerIncFullDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t dots = 4);
IMGUI_API void SpinnerFadeBars(const char *label, float w, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t bars = 3, bool scale = false);
IMGUI_API void SpinnerFadeTris(const char *label, float radius, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t dim = 2, bool scale = false);
IMGUI_API void SpinnerBarsRotateFade(const char *label, float rmin, float rmax , float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t bars = 6);
IMGUI_API void SpinnerBarsScaleMiddle(const char *label, float w, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t bars = 3);
IMGUI_API void SpinnerAngTwin(const char *label, float radius1, float radius2, float thickness, const ImColor &color = 0xffffffff, const ImColor &bg = 0xffffff80, float speed = 2.8f, float angle = IM_PI, size_t arcs = 1, int mode = 0);
IMGUI_API void SpinnerPulsarBall(const char *label, float radius, float thickness, const ImColor &color = 0xfffffff, float speed = 2.8f, bool shadow = false, int mode = 0);
IMGUI_API void SpinnerIncScaleDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t dots = 6, float angle = 0.f, int mode = 0);
IMGUI_API void SpinnerSolarBalls(const char *label, float radius, float thickness, const ImColor &ball = 0xffffffff, const ImColor &bg = 0xffffff80, float speed = 2.8f, size_t balls = 4);
IMGUI_API void SpinnerSolarScaleBalls(const char *label, float radius, float thickness, const ImColor &ball = 0xffffffff, float speed = 2.8f, size_t balls = 4);
IMGUI_API void SpinnerSolarArcs(const char *label, float radius, float thickness, const ImColor &ball = 0xffffffff, const ImColor &bg = 0xffffff80, float speed = 2.8f, size_t balls = 4);
IMGUI_API void SpinnerMovingArcs(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t arcs = 4);
IMGUI_API void SpinnerRainbowCircle(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t arcs = 4, float mode = 1);
IMGUI_API void SpinnerBounceBall(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int dots = 1, bool shadow = false);
IMGUI_API void SpinnerArcRotation(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t arcs = 4, int mode = 0);
IMGUI_API void SpinnerArcFade(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t arcs = 4, int mode = 0);
IMGUI_API void SpinnerSimpleArcFade(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f);
IMGUI_API void SpinnerAsciiSymbolPoints(const char *label, const char* text, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f);
IMGUI_API void SpinnerTextFading(const char *label, const char* text, float radius, float fsize, const ImColor &color = 0xffffffff, float speed = 2.8f);
IMGUI_API void SpinnerSevenSegments(const char *label, const char* text, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f);
IMGUI_API void SpinnerSquareStrokeFill(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f);
IMGUI_API void SpinnerSquareStrokeLoading(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f);
IMGUI_API void SpinnerSquareStrokeFade(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f);
IMGUI_API void SpinnerSquareLoading(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f);
IMGUI_API void SpinnerFilledArcFade(const char *label, float radius, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t arcs = 4, int mode = 0);
IMGUI_API void SpinnerPointsArcBounce(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t points = 4, int circles = 2, float rspeed = 0.f);
IMGUI_API void SpinnerFilledArcColor(const char *label, float radius, const ImColor &color = 0xffff0000, const ImColor &bg = 0xffffffff, float speed = 2.8f, size_t arcs = 4);
IMGUI_API void SpinnerFilledArcRing(const char *label, float radius, float thickness, const ImColor &color = 0xffff0000, const ImColor &bg = 0xffffffff, float speed = 2.8f, size_t arcs = 4);
IMGUI_API void SpinnerArcWedges(const char *label, float radius, const ImColor &color = 0xffff0000, float speed = 2.8f, size_t arcs = 4);
IMGUI_API void SpinnerTwinBall(const char *label, float radius1, float radius2, float thickness, float b_thickness, const ImColor &ball = 0xffffffff, const ImColor &bg = 0xffffff80, float speed = 2.8f, size_t balls = 2);
IMGUI_API void SpinnerSomeScaleDots(const char *label, float radius, float thickness, const ImColor &color = 0xffff0000, float speed = 2.8f, size_t dots = 6, int mode = 0);
IMGUI_API void SpinnerAngTriple(const char *label, float radius1, float radius2, float radius3, float thickness, const ImColor &c1 = 0xffffffff, const ImColor &c2 = 0xffffff80, const ImColor &c3 = 0xffffffff, float speed = 2.8f, float angle = IM_PI);
IMGUI_API void SpinnerAngEclipse(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, float angle = IM_PI);
IMGUI_API void SpinnerIngYang(const char *label, float radius, float thickness, bool reverse, float yang_detlta_r, const ImColor &colorI = 0xffffffff, const ImColor &colorY = 0xffffffff, float speed = 2.8f, float angle = IM_PI * 0.7f);
IMGUI_API void SpinnerGooeyBalls(const char *label, float radius, const ImColor &color = 0xffffffff, float speed = 2.8f, int mode = 0);
IMGUI_API void SpinnerDotsLoading(const char *label, float radius, float thickness, const ImColor &color, const ImColor &bg, float speed);
IMGUI_API void SpinnerRotateGooeyBalls(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int balls = 1, int mode = 0);
IMGUI_API void SpinnerHerbertBalls(const char *label, float radius, float thickness, const ImColor &color, float speed, int balls);
IMGUI_API void SpinnerHerbertBalls3D(const char *label, float radius, float thickness, const ImColor &color, float speed);
IMGUI_API void SpinnerRotateTriangles(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int tris = 3);
IMGUI_API void SpinnerRotateShapes(const char *label, float radius, float thickness, const ImColor &color, float speed, int shapes, int pnt);
IMGUI_API void SpinnerSinSquares(const char *label, float radius, float thickness, const ImColor &color, float speed, int mode = 0);
IMGUI_API void SpinnerMoonLine(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, const ImColor &bg = 0xff000000, float speed = 2.8f, float angle = IM_PI);
IMGUI_API void SpinnerCircleDrop(const char *label, float radius, float thickness, float thickness_drop, const ImColor &color = 0xffffffff, const ImColor &bg = 0xffffff80, float speed = 2.8f, float angle = IM_PI);
IMGUI_API void SpinnerSurroundedIndicator(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, const ImColor &bg = 0xffffff80, float speed = 2.8f);
IMGUI_API void SpinnerWifiIndicator(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, const ImColor &bg = 0xffffff80, float speed = 2.8f, float cangle = 0.f, int dots = 3);
IMGUI_API void SpinnerTrianglesSelector(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, const ImColor &bg = 0xffffff80, float speed = 2.8f, size_t bars = 8);
IMGUI_API void SpinnerCamera(const char *label, float radius, float thickness, LeafColor *leaf_color, float speed = 2.8f, size_t bars = 8, int mode = 0);
IMGUI_API void SpinnerFlowingGradient(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, const ImColor &bg = 0xff000000, float speed = 2.8f, float angle = IM_PI);
IMGUI_API void SpinnerRotateSegments(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t arcs = 4, size_t layers = 1, int mode = 0);
IMGUI_API void SpinnerLemniscate(const char* label, float radius, float thickness, const ImColor& color = 0xffffffff, float speed = 2.8f, float angle = IM_PI / 2.0f);
IMGUI_API void SpinnerRotateGear(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t pins = 12);
IMGUI_API void SpinnerRotateWheel(const char *label, float radius, float thickness, const ImColor &bg_color = 0xffffffff, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t pins = 12);
IMGUI_API void SpinnerAtom(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int elipses = 3);
IMGUI_API void SpinnerPatternRings(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int elipses = 3);
IMGUI_API void SpinnerPatternEclipse(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int elipses = 3, float delta_a = 2.f, float delta_y = 0.f);
IMGUI_API void SpinnerPatternSphere(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int elipses = 3);
IMGUI_API void SpinnerRingSynchronous(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int elipses = 3);
IMGUI_API void SpinnerRingWatermarks(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int elipses = 3);
IMGUI_API void SpinnerRotatedAtom(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int elipses = 3, int mode = 0);
IMGUI_API void SpinnerRainbowBalls(const char *label, float radius, float thickness, const ImColor &color, float speed, int balls = 5, int mode = 0);
IMGUI_API void SpinnerRainbowShot(const char *label, float radius, float thickness, const ImColor &color, float speed, int balls = 5);
IMGUI_API void SpinnerSpiral(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t arcs = 4);
IMGUI_API void SpinnerSpiralEye(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f);
IMGUI_API void SpinnerBarChartSine(const char *label, float radius, float thickness, const ImColor &color, float speed, int bars = 5, int mode = 0);
IMGUI_API void SpinnerBarChartAdvSine(const char *label, float radius, float thickness, const ImColor &color, float speed, int mode = 0);
IMGUI_API void SpinnerBarChartAdvSineFade(const char *label, float radius, float thickness, const ImColor &color, float speed, int mode = 0);
IMGUI_API void SpinnerBarChartRainbow(const char *label, float radius, float thickness, const ImColor &color, float speed, int bars = 5, int mode = 0);
IMGUI_API void SpinnerBlocks(const char *label, float radius, float thickness, const ImColor &bg, const ImColor &color, float speed);
IMGUI_API void SpinnerTwinBlocks(const char *label, float radius, float thickness, const ImColor &bg, const ImColor &color, float speed);
IMGUI_API void SpinnerSquareRandomDots(const char *label, float radius, float thickness, const ImColor &bg, const ImColor &color, float speed);
IMGUI_API void SpinnerSquishSquare(const char *label, float radius, const ImColor &color, float speed);
IMGUI_API void SpinnerFluid(const char *label, float radius, const ImColor &color, float speed, int bars = 3);
IMGUI_API void SpinnerFluidPoints(const char *label, float radius, float thickness, const ImColor &color, float speed, size_t dots = 6, float delta = 0.35f);
IMGUI_API void SpinnerScaleBlocks(const char *label, float radius, float thickness, const ImColor &color, float speed, int mode = 0);
IMGUI_API void SpinnerScaleSquares(const char *label, float radius, float thikness, const ImColor &color, float speed);
IMGUI_API void SpinnerArcPolarFade(const char *label, float radius, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t arcs = 4, int mode = 0);
IMGUI_API void SpinnerArcPolarRadius(const char *label, float radius, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t arcs = 4);
IMGUI_API void SpinnerCaleidoscope(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t arcs = 6, int mode = 0);
IMGUI_API void SpinnerHboDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float minfade = 0.0f, float ryk = 0.f, float speed = 1.1f, size_t dots = 6);
IMGUI_API void SpinnerMoonDots(const char *label, float radius, float thickness, const ImColor &first, const ImColor &second, float speed = 1.1f);
IMGUI_API void SpinnerTwinHboDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float minfade = 0.0f, float ryk = 0.f, float speed = 1.1f, size_t dots = 6, float delta = 0.f);
IMGUI_API void SpinnerThreeDotsStar(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float minfade = 0.0f, float ryk = 0.f, float speed = 1.1f, float delta = 0.f);
IMGUI_API void SpinnerSineArcs(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f);
IMGUI_API void SpinnerTrianglesShift(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, const ImColor &bg = 0xffffff80, float speed = 2.8f, size_t bars = 8);
IMGUI_API void SpinnerPointsShift(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, const ImColor &bg = 0xffffff80, float speed = 2.8f, size_t bars = 8);
IMGUI_API void SpinnerSwingDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f);
IMGUI_API void SpinnerCircularPoints(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 1.8f, int lines = 8);
IMGUI_API void SpinnerCurvedCircle(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t circles = 1);
IMGUI_API void SpinnerModCircle(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float ang_min = 1.f, float ang_max = 1.f, float speed = 2.8f);
IMGUI_API void SpinnerDnaDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, int lt = 8, float delta = 0.5f, bool mode = 0);
IMGUI_API void Spinner3SmuggleDots(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 4.8f, int lt = 8, float delta = 0.5f, bool mode = 0);
IMGUI_API void SpinnerRotateSegmentsPulsar(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, float speed = 2.8f, size_t arcs = 4, size_t layers = 1);
IMGUI_API void SpinnerSplineAng(const char *label, float radius, float thickness, const ImColor &color = 0xffffffff, const ImColor &bg = 0xffffffff, float speed = 2.8f, float angle = IM_PI, int mode = 0);

template<SpinnerTypeT Type, typename... Args>
void Spinner(const char *label, const Args&... args)
{
    struct SpinnerDraw { SpinnerTypeT type; void (*func)(const char *, const SpinnerConfig &); }

    spinner_draw_funcs[e_st_count] = {
        { e_st_rainbow, [] (const char *label, const SpinnerConfig &c) { SpinnerRainbow(label, c.m_Radius, c.m_Thickness, c.m_Color, c.m_Speed, c.m_AngleMin, c.m_AngleMax, c.m_Dots, c.m_Mode); } },
        { e_st_angle,   [] (const char *label, const SpinnerConfig &c) { SpinnerAng(label, c.m_Radius, c.m_Thickness, c.m_Color, c.m_BgColor, c.m_Speed, c.m_Angle, c.m_Mode); } },
        { e_st_dots,    [] (const char *label, const SpinnerConfig &c) { SpinnerDots(label, c.m_FloatPtr, c.m_Radius, c.m_Thickness, c.m_Color, c.m_Speed, c.m_Dots, c.m_MinThickness, c.m_Mode); } },
        { e_st_ang,     [] (const char *label, const SpinnerConfig &c) { SpinnerAng(label, c.m_Radius, c.m_Thickness, c.m_Color, c.m_BgColor, c.m_Speed, c.m_Angle, c.m_Mode); } },
        { e_st_vdots,   [] (const char *label, const SpinnerConfig &c) { SpinnerVDots(label, c.m_Radius, c.m_Thickness, c.m_Color, c.m_BgColor, c.m_Speed, c.m_Dots, c.m_MiddleDots, c.m_Mode); } },
        { e_st_bounce_ball, [](const char *label,const SpinnerConfig &c) { SpinnerBounceBall(label, c.m_Radius, c.m_Thickness, c.m_Color, c.m_Speed, c.m_Dots); } },
        { e_st_eclipse, [] (const char *label, const SpinnerConfig &c) { SpinnerAngEclipse(label , c.m_Radius, c.m_Thickness, c.m_Color, c.m_Speed); } },
        { e_st_ingyang, [] (const char *label, const SpinnerConfig &c) { SpinnerIngYang(label, c.m_Radius, c.m_Thickness, c.m_Reverse, c.m_Delta, c.m_AltColor, c.m_Color, c.m_Speed, c.m_Angle); } },
        { e_st_barchartsine, [] (const char *label, const SpinnerConfig &c) { SpinnerBarChartSine(label, c.m_Radius, c.m_Thickness, c.m_Color, c.m_Speed, c.m_Dots, c.m_Mode); } }
    };

    SpinnerConfig config(SpinnerType{Type}, args...);
    if (config.m_SpinnerType < sizeof(spinner_draw_funcs) / sizeof(spinner_draw_funcs[0]))
    {
        spinner_draw_funcs[config.m_SpinnerType].func(label, config);
    }
}
} // namespace ImGui

namespace ImGui {
// PieMenu code based code posted by @thennequin here:
// https://gist.github.com/thennequin/64b4b996ec990c6ddc13a48c6a0ba68c
// Hope we can use it...
    namespace Pie   {
    /* Declaration */
    IMGUI_API bool BeginPopup( const char* pName, int iMouseButton = 0 );
    IMGUI_API void EndPopup();

    IMGUI_API bool MenuItem( const char* pName);    //, bool bEnabled = true );
    IMGUI_API bool BeginMenu( const char* pName);   //, bool bEnabled = true );
    IMGUI_API void EndMenu();

    }   // namespace Pie
} // namespace ImGui

namespace ImGui
{
class IMGUI_API MsgBox
{
public:
    inline MsgBox() {}
    virtual ~MsgBox() {};
    
    bool Init( const char* title, const char* icon, const char* text, const char** captions, bool show_checkbox = false );
    int  Draw(float wrap_width = 200.f);
    void Open();

    inline void AskAgain()
    {
        m_DontAskAgain = false;
        m_Selected = 0;
    }

protected:
    const char* m_Title;
    const char* m_Icon;
    const char* m_Text;
    const char** m_Captions;
    bool m_ShowCheckbox;
    bool m_DontAskAgain;
    int m_Selected;
};
}

namespace ImGui {
    // Virtual Keyboard
    // USAGE: to get started, just call VirtualKeyboard() in one of your ImGui windows
    enum KeyboardLogicalLayout {
      KLL_QWERTY = 0,
      KLL_QWERTZ,
      KLL_AZERTY,
      KLL_COUNT
    };
    IMGUI_API const char** GetKeyboardLogicalLayoutNames();
    enum KeyboardPhysicalLayout {
      KPL_ANSI = 0,
      KPL_ISO,
      KPL_JIS,
      KPL_COUNT
    };
    IMGUI_API const char** GetKeyboardPhysicalLayoutNames();
    enum VirtualKeyboardFlags_ {
        VirtualKeyboardFlags_ShowBaseBlock       =   1<<0,
        VirtualKeyboardFlags_ShowFunctionBlock   =   1<<1,
        VirtualKeyboardFlags_ShowArrowBlock      =   1<<2,  // This can't be excluded when both VirtualKeyboardFlags_BlockBase and VirtualKeyboardFlags_BlockKeypad are used
        VirtualKeyboardFlags_ShowKeypadBlock     =   1<<3,
        VirtualKeyboardFlags_ShowAllBlocks        =   VirtualKeyboardFlags_ShowBaseBlock|VirtualKeyboardFlags_ShowFunctionBlock|VirtualKeyboardFlags_ShowArrowBlock|VirtualKeyboardFlags_ShowKeypadBlock,
        VirtualKeyboardFlags_NoMouseInteraction = 1<<4,
        VirtualKeyboardFlags_NoKeyboardInteraction = 1<<5,
        VirtualKeyboardFlags_NoInteraction = VirtualKeyboardFlags_NoMouseInteraction | VirtualKeyboardFlags_NoKeyboardInteraction
    };
    typedef int VirtualKeyboardFlags;
    // Displays a virtual keyboard.
    // It always returns ImGuiKey_COUNT, unless:
    //     a) a mouse is clicked (clicked event) on a key AND flag VirtualKeyboardFlags_NoMouseInteraction is not used (DEFAULT)
    //     b) a key is typed (released event) AND VirtualKeyboardFlags_NoKeyboardInteraction is not used (DEFAULT). Note that multiple keys can be pressed together, but only one is returned.
    // In that case, it returns the clicked (or typed) key.
    IMGUI_API ImGuiKey VirtualKeyboard(VirtualKeyboardFlags flags=VirtualKeyboardFlags_ShowAllBlocks,KeyboardLogicalLayout logicalLayout=KLL_QWERTY,KeyboardPhysicalLayout physicalLayout=KPL_ISO);

    // Possible improvements (in random order):
    // 1) The L-shaped enter key is not displayed perfectly. Improve it.
    // 2) Add entries to the KeyboardLogicalLayout enum and implement keyboards for specific countries, riducing the number of 'empty' keys present in the general layout.
} // namespace ImGui

namespace ImGui
{
    IMGUI_API void TextVWithPadding(const ImVec2& padding, const char* fmt, va_list args) IM_FMTLIST(2);
    IMGUI_API void TextColoredWithPadding(const ImVec4& col, const ImVec2& padding, const char* fmt, ...) IM_FMTARGS(3);
    IMGUI_API void TextColoredVWithPadding(const ImVec4& col, const ImVec2& padding, const char* fmt, va_list args) IM_FMTLIST(3);
    IMGUI_API void TextExWithPadding(const ImVec2& padding, const char* text, const char* text_end = NULL, ImGuiTextFlags flags = 0);
} // namespace ImGui

// Multi-Context Compositor v0.10, for Dear ImGui
// Get latest version at http://www.github.com/ocornut/imgui_club
// Licensed under The MIT License (MIT)
namespace ImGui
{
    struct ImGuiMultiContextCompositor
    {
        // List of context + sorted front to back
        ImVector<ImGuiContext*> Contexts;
        ImVector<ImGuiContext*> ContextsFrontToBack;

        // [Internal]
        ImGuiContext*   CtxMouseFirst = NULL;       // When hovering a main/shared viewport, first context with io.WantCaptureMouse
        ImGuiContext*   CtxMouseExclusive = NULL;   // When hovering a secondary viewport
        ImGuiContext*   CtxMouseShape = NULL;       // Context owning mouse cursor shape
        ImGuiContext*   CtxKeyboardExclusive = NULL;// When focusing a secondary viewport
        ImGuiContext*   CtxDragDropSrc = NULL;      // Source context for drag and drop
        ImGuiContext*   CtxDragDropDst = NULL;      // When hovering a main/shared viewport, second context with io.WantCaptureMouse for Drag Drop target
        ImGuiPayload    DragDropPayload;            // Deep copy of drag and drop payload.
    };

    // Add/remove context.
    IMGUI_API void ImGuiMultiContextCompositor_AddContext(ImGuiMultiContextCompositor* mcc, ImGuiContext* ctx);
    IMGUI_API void ImGuiMultiContextCompositor_RemoveContext(ImGuiMultiContextCompositor* mcc, ImGuiContext* ctx);
    
    // Call at a shared sync point before calling NewFrame() on any context.
    IMGUI_API void ImGuiMultiContextCompositor_PreNewFrameUpdateAll(ImGuiMultiContextCompositor* mcc);
    
    // Call after caling NewFrame() on a given context.
    IMGUI_API void ImGuiMultiContextCompositor_PostNewFrameUpdateOne(ImGuiMultiContextCompositor* mcc, ImGuiContext* ctx);
    
    // Call at a shared sync point after calling EndFrame() on all contexts.
    IMGUI_API void ImGuiMultiContextCompositor_PostEndFrameUpdateAll(ImGuiMultiContextCompositor* mcc);
} // namespace ImGui
#endif // IMGUI_WIDGET_H
