#include <algorithm>
#include <chrono>
#include <string>
#include <regex>
#include <cmath>

#include "TextEditor.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h" // for imGui::GetCurrentWindow()
#include "imgui_internal.h"

#ifndef isascii
#define isascii(a) ((unsigned)(a) < 128)
#endif

// TODO
// - multiline comments vs single-line: latter is blocking start of a ML

template<class InputIt1, class InputIt2, class BinaryPredicate>
bool equals(InputIt1 first1, InputIt1 last1,
	InputIt2 first2, InputIt2 last2, BinaryPredicate p)
{
	for (; first1 != last1 && first2 != last2; ++first1, ++first2)
	{
		if (!p(*first1, *first2))
			return false;
	}
	return first1 == last1 && first2 == last2;
}

TextEditor::TextEditor()
	: mLineSpacing(1.0f)
	, mUndoIndex(0)
	, mTabSize(4)
	, mOverwrite(false)
	, mReadOnly(false)
	, mWithinRender(false)
	, mScrollToCursor(false)
	, mScrollToTop(false)
	, mTextChanged(false)
	, mColorizerEnabled(true)
	, mTextStart(20.0f)
	, mLeftMargin(10)
	, mCursorPositionChanged(false)
	, mColorRangeMin(0)
	, mColorRangeMax(0)
	, mSelectionMode(SelectionMode::Normal)
	, mCheckComments(true)
	, mHandleKeyboardInputs(true)
	, mHandleMouseInputs(true)
	, mIgnoreImGuiChild(false)
	, mShowWhitespaces(true)
	, mShowShortTabGlyphs(false)
	, mStartTime(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
	, mLastClick(-1.0f)
    , mSelecting(false)
{
	SetPalette(GetDarkPalette());
	SetLanguageDefinition(LanguageDefinition::HLSL());
	mLines.push_back(Line());
}

TextEditor::~TextEditor()
{
}

void TextEditor::SetLanguageDefinition(const LanguageDefinition & aLanguageDef)
{
	mLanguageDefinition = aLanguageDef;
	mRegexList.clear();

	for (auto& r : mLanguageDefinition.mTokenRegexStrings)
		mRegexList.push_back(std::make_pair(std::regex(r.first, std::regex_constants::optimize), r.second));

	Colorize();
}

void TextEditor::SetPalette(const Palette & aValue)
{
	mPaletteBase = aValue;
}

std::string TextEditor::GetText(const Coordinates & aStart, const Coordinates & aEnd) const
{
	std::string result;

	auto lstart = aStart.mLine;
	auto lend = aEnd.mLine;
	auto istart = GetCharacterIndex(aStart);
	auto iend = GetCharacterIndex(aEnd);
	size_t s = 0;

	for (size_t i = lstart; i < lend; i++)
		s += mLines[i].size();

	result.reserve(s + s / 8);

	while (istart < iend || lstart < lend)
	{
		if (lstart >= (int)mLines.size())
			break;

		auto& line = mLines[lstart];
		if (istart < (int)line.size())
		{
			result += line[istart].mChar;
			istart++;
		}
		else
		{
			istart = 0;
			++lstart;
			result += '\n';
		}
	}
    // NOTE: added this so it doesn't append an extra new line at the end
    //result.pop_back();
	return result;
}

TextEditor::Coordinates TextEditor::GetActualCursorCoordinates() const
{
	return SanitizeCoordinates(mState.mCursorPosition);
}

TextEditor::Coordinates TextEditor::SanitizeCoordinates(const Coordinates & aValue) const
{
	auto line = aValue.mLine;
	auto column = aValue.mColumn;
	if (line >= (int)mLines.size())
	{
		if (mLines.empty())
		{
			line = 0;
			column = 0;
		}
		else
		{
			line = (int)mLines.size() - 1;
			column = GetLineMaxColumn(line);
		}
		return Coordinates(line, column);
	}
	else
	{
		column = mLines.empty() ? 0 : ImMin(column, GetLineMaxColumn(line));
		return Coordinates(line, column);
	}
}

// https://en.wikipedia.org/wiki/UTF-8
// We assume that the char is a standalone character (<128) or a leading byte of an UTF-8 code sequence (non-10xxxxxx code)
static int UTF8CharLength(TextEditor::Char c)
{
	if ((c & 0xFE) == 0xFC)
		return 6;
	if ((c & 0xFC) == 0xF8)
		return 5;
	if ((c & 0xF8) == 0xF0)
		return 4;
	else if ((c & 0xF0) == 0xE0)
		return 3;
	else if ((c & 0xE0) == 0xC0)
		return 2;
	return 1;
}

// "Borrowed" from ImGui source
static inline int ImTextCharToUtf8(char* buf, int buf_size, unsigned int c)
{
	if (c < 0x80)
	{
		buf[0] = (char)c;
		return 1;
	}
	if (c < 0x800)
	{
		if (buf_size < 2) return 0;
		buf[0] = (char)(0xc0 + (c >> 6));
		buf[1] = (char)(0x80 + (c & 0x3f));
		return 2;
	}
	if (c >= 0xdc00 && c < 0xe000)
	{
		return 0;
	}
	if (c >= 0xd800 && c < 0xdc00)
	{
		if (buf_size < 4) return 0;
		buf[0] = (char)(0xf0 + (c >> 18));
		buf[1] = (char)(0x80 + ((c >> 12) & 0x3f));
		buf[2] = (char)(0x80 + ((c >> 6) & 0x3f));
		buf[3] = (char)(0x80 + ((c) & 0x3f));
		return 4;
	}
	//else if (c < 0x10000)
	{
		if (buf_size < 3) return 0;
		buf[0] = (char)(0xe0 + (c >> 12));
		buf[1] = (char)(0x80 + ((c >> 6) & 0x3f));
		buf[2] = (char)(0x80 + ((c) & 0x3f));
		return 3;
	}
}

void TextEditor::Advance(Coordinates & aCoordinates) const
{
	if (aCoordinates.mLine < (int)mLines.size())
	{
		auto& line = mLines[aCoordinates.mLine];
		auto cindex = GetCharacterIndex(aCoordinates);

		if (cindex + 1 < (int)line.size())
		{
			auto delta = UTF8CharLength(line[cindex].mChar);
			cindex = ImMin(cindex + delta, (int)line.size() - 1);
		}
		else
		{
			++aCoordinates.mLine;
			cindex = 0;
		}
		aCoordinates.mColumn = GetCharacterColumn(aCoordinates.mLine, cindex);
	}
}

void TextEditor::DeleteRange(const Coordinates & aStart, const Coordinates & aEnd)
{
	assert(aEnd >= aStart);
	assert(!mReadOnly);

	//printf("D(%d.%d)-(%d.%d)\n", aStart.mLine, aStart.mColumn, aEnd.mLine, aEnd.mColumn);

	if (aEnd == aStart)
		return;

	auto start = GetCharacterIndex(aStart);
	auto end = GetCharacterIndex(aEnd);

	if (aStart.mLine == aEnd.mLine)
	{
		auto& line = mLines[aStart.mLine];
		auto n = GetLineMaxColumn(aStart.mLine);
		if (aEnd.mColumn >= n)
			line.erase(line.begin() + start, line.end());
		else
			line.erase(line.begin() + start, line.begin() + end);
	}
	else
	{
		auto& firstLine = mLines[aStart.mLine];
		auto& lastLine = mLines[aEnd.mLine];

		firstLine.erase(firstLine.begin() + start, firstLine.end());
		lastLine.erase(lastLine.begin(), lastLine.begin() + end);

		if (aStart.mLine < aEnd.mLine)
			firstLine.insert(firstLine.end(), lastLine.begin(), lastLine.end());

		if (aStart.mLine < aEnd.mLine)
			RemoveLine(aStart.mLine + 1, aEnd.mLine + 1);
	}

	mTextChanged = true;
}

int TextEditor::InsertTextAt(Coordinates& /* inout */ aWhere, const char * aValue)
{
	assert(!mReadOnly);

	int cindex = GetCharacterIndex(aWhere);
	int totalLines = 0;
	while (*aValue != '\0')
	{
		assert(!mLines.empty());

		if (*aValue == '\r')
		{
			// skip
			++aValue;
		}
		else if (*aValue == '\n')
		{
			if (cindex < (int)mLines[aWhere.mLine].size())
			{
				auto& newLine = InsertLine(aWhere.mLine + 1);
				auto& line = mLines[aWhere.mLine];
				newLine.insert(newLine.begin(), line.begin() + cindex, line.end());
				line.erase(line.begin() + cindex, line.end());
			}
			else
			{
				InsertLine(aWhere.mLine + 1);
			}
			++aWhere.mLine;
			aWhere.mColumn = 0;
			cindex = 0;
			++totalLines;
			++aValue;
		}
		else
		{
			auto& line = mLines[aWhere.mLine];
			auto d = UTF8CharLength(*aValue);
			while (d-- > 0 && *aValue != '\0')
				line.insert(line.begin() + cindex++, Glyph(*aValue++, PaletteIndex::Default));
			aWhere.mColumn = GetCharacterColumn(aWhere.mLine, cindex);
		}

		mTextChanged = true;
	}

	return totalLines;
}

void TextEditor::AddUndo(UndoRecord& aValue)
{
	assert(!mReadOnly);
	//printf("AddUndo: (@%d.%d) +\'%s' [%d.%d .. %d.%d], -\'%s', [%d.%d .. %d.%d] (@%d.%d)\n",
	//	aValue.mBefore.mCursorPosition.mLine, aValue.mBefore.mCursorPosition.mColumn,
	//	aValue.mAdded.c_str(), aValue.mAddedStart.mLine, aValue.mAddedStart.mColumn, aValue.mAddedEnd.mLine, aValue.mAddedEnd.mColumn,
	//	aValue.mRemoved.c_str(), aValue.mRemovedStart.mLine, aValue.mRemovedStart.mColumn, aValue.mRemovedEnd.mLine, aValue.mRemovedEnd.mColumn,
	//	aValue.mAfter.mCursorPosition.mLine, aValue.mAfter.mCursorPosition.mColumn
	//	);

	mUndoBuffer.resize((size_t)(mUndoIndex + 1));
	mUndoBuffer.back() = aValue;
	++mUndoIndex;
}

TextEditor::Coordinates TextEditor::ScreenPosToCoordinates(const ImVec2& aPosition, bool aInsertionMode) const
{
	ImVec2 origin = ImGui::GetCursorScreenPos();
	ImVec2 local(aPosition.x - origin.x + 3.0f, aPosition.y - origin.y);
	float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ").x;

	int lineNo = ImMax(0, (int)floor(local.y / mCharAdvance.y));

	int columnCoord = 0;

	if (lineNo >= 0 && lineNo < (int)mLines.size())
	{
		auto& line = mLines.at(lineNo);
        //Fix for inability to click/go to the last column of a line, due to delta not being added when at the end of a line. Make sure to increment columnCoord with delta, before bailing of the while/for loop.
		//int columnIndex = 0;
		//std::string cumulatedString = "";
		//float columnWidth = 0.0f;
		float columnX = 0.0f;
		//int delta = 0;

		// First we find the hovered column coord.
		//while (mTextStart + columnX - (aInsertionMode ? 0.5f : 0.0f) * columnWidth < local.x && (size_t)columnIndex < line.size())
        for (size_t columnIndex = 0; columnIndex < line.size(); ++columnIndex)
		{
			//columnCoord += delta;
            float columnWidth = 0.0f;
			int delta = 0;
			if (line[columnIndex].mChar == '\t')
			{
				float oldX = columnX;
				columnX = (1.0f + std::floor((1.0f + columnX) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
				columnWidth = columnX - oldX;
				delta = columnCoord - (columnCoord / mTabSize) * mTabSize + mTabSize;
			}
			else
			{
				char buf[7];
				auto d = UTF8CharLength(line[columnIndex].mChar);
				int i = 0;
				while (i < 6 && d-- > 0)
					buf[i++] = line[columnIndex].mChar;
				buf[i] = '\0';
				columnWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf).x;
				columnX += columnWidth;
				delta = 1;
			}
			//++columnIndex;
            if (mTextStart + columnX - (aInsertionMode ? 0.5f : 0.0f) * columnWidth < local.x)
 				columnCoord += delta;
 			else
 				break;
		}

		// Then we reduce by 1 column coord if cursor is on the left side of the hovered column.
		//if (aInsertionMode && mTextStart + columnX - columnWidth * 2.0f < local.x)
		//	columnIndex = std::min((int)line.size() - 1, columnIndex + 1);
	}

	return SanitizeCoordinates(Coordinates(lineNo, columnCoord));
}

TextEditor::Coordinates TextEditor::FindWordStart(const Coordinates & aFrom) const
{
	Coordinates at = aFrom;
	if (at.mLine >= (int)mLines.size())
		return at;

	auto& line = mLines[at.mLine];
	auto cindex = GetCharacterIndex(at);

	if (cindex >= (int)line.size())
		return at;

	while (cindex > 0 && isspace(line[cindex].mChar))
		--cindex;

	auto cstart = (PaletteIndex)line[cindex].mColorIndex;
	while (cindex > 0)
	{
		auto c = line[cindex].mChar;
		if ((c & 0xC0) != 0x80)	// not UTF code sequence 10xxxxxx
		{
			if (c <= 32 && isspace(c))
			{
				cindex++;
				break;
			}
			if (cstart != (PaletteIndex)line[size_t(cindex - 1)].mColorIndex)
				break;
		}
		--cindex;
	}
	return Coordinates(at.mLine, GetCharacterColumn(at.mLine, cindex));
}

TextEditor::Coordinates TextEditor::FindWordEnd(const Coordinates & aFrom) const
{
	Coordinates at = aFrom;
	if (at.mLine >= (int)mLines.size())
		return at;

	auto& line = mLines[at.mLine];
	auto cindex = GetCharacterIndex(at);

	if (cindex >= (int)line.size())
		return at;

	bool prevspace = (bool)!!isspace(line[cindex].mChar);
	auto cstart = (PaletteIndex)line[cindex].mColorIndex;
	while (cindex < (int)line.size())
	{
		auto c = line[cindex].mChar;
		auto d = UTF8CharLength(c);
		if (cstart != (PaletteIndex)line[cindex].mColorIndex)
			break;

		if (prevspace != !!isspace(c))
		{
			if (isspace(c))
				while (cindex < (int)line.size() && isspace(line[cindex].mChar))
					++cindex;
			break;
		}
		cindex += d;
	}
	return Coordinates(aFrom.mLine, GetCharacterColumn(aFrom.mLine, cindex));
}

TextEditor::Coordinates TextEditor::FindNextWord(const Coordinates & aFrom) const
{
	Coordinates at = aFrom;
	if (at.mLine >= (int)mLines.size())
		return at;

	// skip to the next non-word character
	auto cindex = GetCharacterIndex(aFrom);
	bool isword = false;
	bool skip = false;
	if (cindex < (int)mLines[at.mLine].size())
	{
		auto& line = mLines[at.mLine];
		isword = !!isalnum(line[cindex].mChar);
		skip = isword;
	}

	while (!isword || skip)
	{
		if (at.mLine >= mLines.size())
		{
			auto l = ImMax(0, (int) mLines.size() - 1);
			return Coordinates(l, GetLineMaxColumn(l));
		}

		auto& line = mLines[at.mLine];
		if (cindex < (int)line.size())
		{
			isword = isalnum(line[cindex].mChar);

			if (isword && !skip)
				return Coordinates(at.mLine, GetCharacterColumn(at.mLine, cindex));

			if (!isword)
				skip = false;

			cindex++;
		}
		else
		{
			cindex = 0;
			++at.mLine;
			skip = false;
			isword = false;
		}
	}

	return at;
}

int TextEditor::GetCharacterIndex(const Coordinates& aCoordinates) const
{
	if (aCoordinates.mLine >= mLines.size())
		return -1;
	auto& line = mLines[aCoordinates.mLine];
	int c = 0;
	int i = 0;
	for (; i < line.size() && c < aCoordinates.mColumn;)
	{
		if (line[i].mChar == '\t')
			c = (c / mTabSize) * mTabSize + mTabSize;
		else
			++c;
		i += UTF8CharLength(line[i].mChar);
	}
	return i;
}

int TextEditor::GetCharacterColumn(int aLine, int aIndex) const
{
	if (aLine >= mLines.size())
		return 0;
	auto& line = mLines[aLine];
	int col = 0;
	int i = 0;
	while (i < aIndex && i < (int)line.size())
	{
		auto c = line[i].mChar;
		i += UTF8CharLength(c);
		if (c == '\t')
			col = (col / mTabSize) * mTabSize + mTabSize;
		else
			col++;
	}
	return col;
}

int TextEditor::GetLineCharacterCount(int aLine) const
{
	if (aLine >= mLines.size())
		return 0;
	auto& line = mLines[aLine];
	int c = 0;
	for (unsigned i = 0; i < line.size(); c++)
		i += UTF8CharLength(line[i].mChar);
	return c;
}

int TextEditor::GetLineMaxColumn(int aLine) const
{
	if (aLine >= mLines.size())
		return 0;
	auto& line = mLines[aLine];
	int col = 0;
	for (unsigned i = 0; i < line.size(); )
	{
		auto c = line[i].mChar;
		if (c == '\t')
			col = (col / mTabSize) * mTabSize + mTabSize;
		else
			col++;
		i += UTF8CharLength(c);
	}
	return col;
}

bool TextEditor::IsOnWordBoundary(const Coordinates & aAt) const
{
	if (aAt.mLine >= (int)mLines.size() || aAt.mColumn == 0)
		return true;

	auto& line = mLines[aAt.mLine];
	auto cindex = GetCharacterIndex(aAt);
	if (cindex >= (int)line.size())
		return true;

	if (mColorizerEnabled)
		return line[cindex].mColorIndex != line[size_t(cindex - 1)].mColorIndex;

	return isspace(line[cindex].mChar) != isspace(line[cindex - 1].mChar);
}

void TextEditor::RemoveLine(int aStart, int aEnd)
{
	assert(!mReadOnly);
	assert(aEnd >= aStart);
	assert(mLines.size() > (size_t)(aEnd - aStart));

	ErrorMarkers etmp;
	for (auto& i : mErrorMarkers)
	{
		ErrorMarkers::value_type e(i.first >= aStart ? i.first - 1 : i.first, i.second);
		if (e.first >= aStart && e.first <= aEnd)
			continue;
		etmp.insert(e);
	}
	mErrorMarkers = std::move(etmp);

	Breakpoints btmp;
	for (auto i : mBreakpoints)
	{
		if (i >= aStart && i <= aEnd)
			continue;
		btmp.insert(i >= aStart ? i - 1 : i);
	}
	mBreakpoints = std::move(btmp);

	mLines.erase(mLines.begin() + aStart, mLines.begin() + aEnd);
	assert(!mLines.empty());

	mTextChanged = true;
}

void TextEditor::RemoveLine(int aIndex)
{
	assert(!mReadOnly);
	assert(mLines.size() > 1);

	ErrorMarkers etmp;
	for (auto& i : mErrorMarkers)
	{
		ErrorMarkers::value_type e(i.first > aIndex ? i.first - 1 : i.first, i.second);
		if (e.first - 1 == aIndex)
			continue;
		etmp.insert(e);
	}
	mErrorMarkers = std::move(etmp);

	Breakpoints btmp;
	for (auto i : mBreakpoints)
	{
		if (i == aIndex)
			continue;
		btmp.insert(i >= aIndex ? i - 1 : i);
	}
	mBreakpoints = std::move(btmp);

	mLines.erase(mLines.begin() + aIndex);
	assert(!mLines.empty());

	mTextChanged = true;
}

TextEditor::Line& TextEditor::InsertLine(int aIndex)
{
	assert(!mReadOnly);

	auto& result = *mLines.insert(mLines.begin() + aIndex, Line());

	ErrorMarkers etmp;
	for (auto& i : mErrorMarkers)
		etmp.insert(ErrorMarkers::value_type(i.first >= aIndex ? i.first + 1 : i.first, i.second));
	mErrorMarkers = std::move(etmp);

	Breakpoints btmp;
	for (auto i : mBreakpoints)
		btmp.insert(i >= aIndex ? i + 1 : i);
	mBreakpoints = std::move(btmp);

	return result;
}

std::string TextEditor::GetWordUnderCursor() const
{
	auto c = GetCursorPosition();
	return GetWordAt(c);
}

std::string TextEditor::GetWordAt(const Coordinates & aCoords) const
{
	auto start = FindWordStart(aCoords);
	auto end = FindWordEnd(aCoords);

	std::string r;

	auto istart = GetCharacterIndex(start);
	auto iend = GetCharacterIndex(end);

	for (auto it = istart; it < iend; ++it)
		r.push_back(mLines[aCoords.mLine][it].mChar);

	return r;
}

ImRect TextEditor::GetWordRectAt(const ImVec2& mpos) const
{
	auto spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ");
	auto pos = ScreenPosToCoordinates(mpos);
	ImVec2 origin = ImGui::GetCursorScreenPos();
	auto start = FindWordStart(pos);
	auto end = FindWordEnd(pos);
	auto x_offset = pos.mColumn - start.mColumn;
	ImVec2 str_pos_min = ImVec2(floor((mpos.x - origin.x) / spaceSize.x) * spaceSize.x + origin.x + 1, floor((mpos.y - origin.y) / spaceSize.y) * spaceSize.y + origin.y);
	str_pos_min -= ImVec2(x_offset * spaceSize.x, 0);
	ImVec2 str_pos_max = str_pos_min + ImVec2((end.mColumn - start.mColumn) * spaceSize.x, spaceSize.y);
	str_pos_max += ImVec2(2, 0);
	return ImRect(str_pos_min, str_pos_max);
}

ImU32 TextEditor::GetGlyphColor(const Glyph & aGlyph) const
{
	if (!mColorizerEnabled)
		return mPalette[(int)PaletteIndex::Default];
	if (aGlyph.mComment)
		return mPalette[(int)PaletteIndex::Comment];
	if (aGlyph.mMultiLineComment)
		return mPalette[(int)PaletteIndex::MultiLineComment];
	auto const color = mPalette[(int)aGlyph.mColorIndex];
	if (aGlyph.mPreprocessor)
	{
		const auto ppcolor = mPalette[(int)PaletteIndex::Preprocessor];
		const int c0 = ((ppcolor & 0xff) + (color & 0xff)) / 2;
		const int c1 = (((ppcolor >> 8) & 0xff) + ((color >> 8) & 0xff)) / 2;
		const int c2 = (((ppcolor >> 16) & 0xff) + ((color >> 16) & 0xff)) / 2;
		const int c3 = (((ppcolor >> 24) & 0xff) + ((color >> 24) & 0xff)) / 2;
		return ImU32(c0 | (c1 << 8) | (c2 << 16) | (c3 << 24));
	}
	return color;
}

void TextEditor::HandleKeyboardInputs()
{
	if (ImGui::IsWindowFocused())
	{
		if (ImGui::IsWindowHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
		//ImGui::CaptureKeyboardFromApp(true);

		ImGuiIO& io = ImGui::GetIO();
		auto isOSX = false; //io.ConfigMacOSXBehaviors; // modify by Dicky since imgui 19070 already swap ctrl/cmd key
		auto alt = io.KeyAlt;
		auto ctrl = io.KeyCtrl;
		auto shift = io.KeyShift;
		auto super = io.KeySuper;

		auto isShortcut = (isOSX ? (super && !ctrl) : (ctrl && !super)) && !alt && !shift;
		auto isShiftShortcut = (isOSX ? (super && !ctrl) : (ctrl && !super)) && shift && !alt;
		auto isWordmoveKey = isOSX ? alt : ctrl;
		auto isAltOnly = alt && !ctrl && !shift && !super;
		auto isCtrlOnly = ctrl && !alt && !shift && !super;
		auto isShiftOnly = shift && !alt && !ctrl && !super;

		io.WantCaptureKeyboard = true;
		io.WantTextInput = true;

		if (!IsReadOnly() && isShortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Z)))
			Undo();
		else if (!IsReadOnly() && isAltOnly && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace)))
			Undo();
		else if (!IsReadOnly() && isShortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Y)))
			Redo();
		else if (!IsReadOnly() && isShiftShortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Z)))
			Redo();
		else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)))
			MoveUp(1, shift);
		else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)))
			MoveDown(1, shift);
		else if ((isOSX ? !ctrl : !alt) && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)))
			MoveLeft(1, shift, isWordmoveKey);
		else if ((isOSX ? !ctrl : !alt) && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)))
			MoveRight(1, shift, isWordmoveKey);
		else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageUp)))
			MoveUp(GetPageSize() - 4, shift);
		else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageDown)))
			MoveDown(GetPageSize() - 4, shift);
		else if (ctrl && !alt && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Home)))
			MoveTop(shift);
		else if (ctrl && !alt && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_End)))
			MoveBottom(shift);
		else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Home)))
			MoveHome(shift);
		else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_End)))
			MoveEnd(shift);
		else if (!IsReadOnly() && !alt && !ctrl && !shift && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete)))
			Delete();
		else if (!IsReadOnly() && !alt && !ctrl && !shift && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace)))
			Backspace();
		else if (!alt && !ctrl && !shift && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Insert)))
			mOverwrite ^= true;
		else if (isCtrlOnly && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Insert)))
			Copy();
		else if (isShortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_C)))
			Copy();
		else if (!IsReadOnly() && isShiftOnly && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Insert)))
			Paste();
		else if (!IsReadOnly() && isShortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_V)))
			Paste();
		else if (isShortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_X)))
			Cut();
		else if (isShiftOnly && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete)))
			Cut();
		else if (isShortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_A)))
			SelectAll();
		else if (!IsReadOnly() && !alt && !ctrl && !shift && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)))
			EnterCharacter('\n', false);
		else if (!IsReadOnly() && !alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Tab)))
			EnterCharacter('\t', shift);
		if (!IsReadOnly() && !io.InputQueueCharacters.empty() && !ctrl && !super)
		{
			for (int i = 0; i < io.InputQueueCharacters.Size; i++)
			{
				auto c = io.InputQueueCharacters[i];
				if (c != 0 && (c == '\n' || c >= 32))
					EnterCharacter(c, shift);
			}
			io.InputQueueCharacters.resize(0);
		}
	}
}

void TextEditor::HandleMouseInputs()
{
	ImGuiIO& io = ImGui::GetIO();
	auto shift = io.KeyShift;
	// modify by Dicky since imgui 19070 already swap ctrl/cmd
	auto ctrl = io.KeyCtrl; //io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
	auto alt = io.KeyAlt; //io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;
	// modify by Dicky end
    if (ImGui::IsMouseReleased(0))
    {
        mSelecting = false;
    }
	if (ImGui::IsWindowHovered())
	{
		if (!shift && !alt)
		{
			auto click = ImGui::IsMouseClicked(0);
			auto doubleClick = ImGui::IsMouseDoubleClicked(0);
			auto t = ImGui::GetTime();
			auto tripleClick = click && !doubleClick && (mLastClick != -1.0f && (t - mLastClick) < io.MouseDoubleClickTime);

			/*
			Left mouse button triple click
			*/

			if (tripleClick)
			{
				if (!ctrl)
				{
					mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = SanitizeCoordinates(ScreenPosToCoordinates(ImGui::GetMousePos()));
					mSelectionMode = SelectionMode::Line;
					SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
				}

				mLastClick = -1.0f;
			}

			/*
			Left mouse button double click
			*/

			else if (doubleClick)
			{
				if (!ctrl)
				{
					mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = SanitizeCoordinates(ScreenPosToCoordinates(ImGui::GetMousePos()));
					if (mSelectionMode == SelectionMode::Line)
						mSelectionMode = SelectionMode::Normal;
					else
						mSelectionMode = SelectionMode::Word;
					SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
				}

				mLastClick = (float)ImGui::GetTime();
			}

			/*
			Left mouse button click
			*/
			else if (click)
			{
				mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = SanitizeCoordinates(ScreenPosToCoordinates(ImGui::GetMousePos() - ImVec2(6, 0), !mOverwrite));
				if (ctrl)
					mSelectionMode = SelectionMode::Word;
				else
					mSelectionMode = SelectionMode::Normal;
				SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);

				mLastClick = (float)ImGui::GetTime();
			}
			// Mouse left button dragging (=> update selection)
			else if (ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0))
			{
				io.WantCaptureMouse = true;
                mSelecting = true;
				mState.mCursorPosition = mInteractiveEnd = SanitizeCoordinates(ScreenPosToCoordinates(ImGui::GetMousePos(), !mOverwrite));
				SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
			}
		}
	}
    else if (mSelecting && ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0))
    {
        auto delta = ImGui::GetMouseDragDelta(0);
        auto scrollY = ImGui::GetScrollY();
        auto scrollMaxY = ImGui::GetScrollMaxY();
        auto scrollX = ImGui::GetScrollX();
        auto scrollMaxX = ImGui::GetScrollMaxX();
        if (delta.y > 0)
        {
            ImGui::SetScrollY(ImMin(scrollMaxY, scrollY + 16));
        }
        else if (delta.y < 0)
        {
            ImGui::SetScrollY(ImMax((float)0.0, scrollY - 16));
        }
        if (delta.x > 0)
        {
            ImGui::SetScrollX(ImMin(scrollMaxX, scrollX + 16));
        }
        else if (delta.x < 0)
        {
            ImGui::SetScrollX(ImMax((float)0.0, scrollX - 16));
        }
        mState.mCursorPosition = mInteractiveEnd = SanitizeCoordinates(ScreenPosToCoordinates(ImGui::GetMousePos(), !mOverwrite));
		SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
    }
}

void TextEditor::Render()
{
	/* Compute mCharAdvance regarding to scaled font size (Ctrl + mouse wheel)*/
	const float fontSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
	mCharAdvance = ImVec2(fontSize, ImGui::GetTextLineHeightWithSpacing() * mLineSpacing);

	/* Update palette with the current alpha from style */
	for (int i = 0; i < (int)PaletteIndex::Max; ++i)
	{
		auto color = ImGui::ColorConvertU32ToFloat4(mPaletteBase[i]);
		color.w *= ImGui::GetStyle().Alpha;
		mPalette[i] = ImGui::ColorConvertFloat4ToU32(color);
	}

	assert(mLineBuffer.empty());

	auto contentSize = ImGui::GetWindowContentRegionMax();
	auto drawList = ImGui::GetWindowDrawList();
	float longest(mTextStart);

	if (mScrollToTop)
	{
		mScrollToTop = false;
		ImGui::SetScrollY(0.f);
	}

	ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
	auto scrollX = ImGui::GetScrollX();
	auto scrollY = ImGui::GetScrollY();

	auto lineNo = (int)floor(scrollY / mCharAdvance.y);
	auto globalLineMax = (int)mLines.size();
	auto lineMax = ImMax(0, ImMin((int)mLines.size() - 1, lineNo + (int)floor((scrollY + contentSize.y) / mCharAdvance.y)));

	// Deduce mTextStart by evaluating mLines size (global lineMax) plus two spaces as text width
	char buf[16];
	snprintf(buf, 16, " %d ", globalLineMax);
	mTextStart = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x + mLeftMargin;

	if (!mLines.empty())
	{
		float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;

		while (lineNo <= lineMax)
		{
			ImVec2 lineStartScreenPos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + lineNo * mCharAdvance.y);
			ImVec2 textScreenPos = ImVec2(lineStartScreenPos.x + mTextStart, lineStartScreenPos.y);

			auto& line = mLines[lineNo];
			longest = ImMax(mTextStart + TextDistanceToLineStart(Coordinates(lineNo, GetLineMaxColumn(lineNo))), longest);
			auto columnNo = 0;
			Coordinates lineStartCoord(lineNo, 0);
			Coordinates lineEndCoord(lineNo, GetLineMaxColumn(lineNo));

			// Draw selection for the current line
			float sstart = -1.0f;
			float ssend = -1.0f;

			assert(mState.mSelectionStart <= mState.mSelectionEnd);
			if (mState.mSelectionStart <= lineEndCoord)
				sstart = mState.mSelectionStart > lineStartCoord ? TextDistanceToLineStart(mState.mSelectionStart) : 0.0f;
			if (mState.mSelectionEnd > lineStartCoord)
				ssend = TextDistanceToLineStart(mState.mSelectionEnd < lineEndCoord ? mState.mSelectionEnd : lineEndCoord);

			if (mState.mSelectionEnd.mLine > lineNo)
				ssend += mCharAdvance.x;

			if (sstart != -1 && ssend != -1 && sstart < ssend)
			{
				ImVec2 vstart(lineStartScreenPos.x + mTextStart + sstart, lineStartScreenPos.y);
				ImVec2 vend(lineStartScreenPos.x + mTextStart + ssend, lineStartScreenPos.y + mCharAdvance.y);
				drawList->AddRectFilled(vstart, vend, mPalette[(int)PaletteIndex::Selection]);
			}

			// Draw breakpoints
			auto start = ImVec2(lineStartScreenPos.x + scrollX, lineStartScreenPos.y);

			if (mBreakpoints.count(lineNo + 1) != 0)
			{
				auto end = ImVec2(lineStartScreenPos.x + contentSize.x + 2.0f * scrollX, lineStartScreenPos.y + mCharAdvance.y);
				drawList->AddRectFilled(start, end, mPalette[(int)PaletteIndex::Breakpoint]);
			}

			// Draw error markers
			auto errorIt = mErrorMarkers.find(lineNo + 1);
			if (errorIt != mErrorMarkers.end())
			{
				auto end = ImVec2(lineStartScreenPos.x + contentSize.x + 2.0f * scrollX, lineStartScreenPos.y + mCharAdvance.y);
				drawList->AddRectFilled(start, end, mPalette[(int)PaletteIndex::ErrorMarker]);

				if (ImGui::IsMouseHoveringRect(lineStartScreenPos, end) && ImGui::BeginTooltip())
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
					ImGui::Text("Error at line %d:", errorIt->first);
					ImGui::PopStyleColor();
					ImGui::Separator();
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.02f, 0.2f, 1.0f));
					ImGui::Text("%s", errorIt->second.c_str());
					ImGui::PopStyleColor();
					ImGui::EndTooltip();
				}
			}

			// Draw line number (right aligned)
			snprintf(buf, 16, "%d  ", lineNo + 1);

			auto lineNoWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x;
			drawList->AddText(ImVec2(lineStartScreenPos.x + mTextStart - lineNoWidth, lineStartScreenPos.y), mPalette[(int)PaletteIndex::LineNumber], buf);

			if (mState.mCursorPosition.mLine == lineNo)
			{
				auto focused = ImGui::IsWindowFocused();

				// Highlight the current line (where the cursor is)
				if (!HasSelection())
				{
					auto end = ImVec2(start.x + contentSize.x + scrollX, start.y + mCharAdvance.y);
					drawList->AddRectFilled(start, end, mPalette[(int)(focused ? PaletteIndex::CurrentLineFill : PaletteIndex::CurrentLineFillInactive)]);
					drawList->AddRect(start, end, mPalette[(int)PaletteIndex::CurrentLineEdge], 1.0f);
				}

				// Render the cursor
				if (focused)
				{
					auto timeEnd = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
					auto elapsed = timeEnd - mStartTime;
					if (elapsed > 400)
					{
						float width = 1.0f;
						auto cindex = GetCharacterIndex(mState.mCursorPosition);
						float cx = TextDistanceToLineStart(mState.mCursorPosition);

						if (mOverwrite && cindex < (int)line.size())
						{
							auto c = line[cindex].mChar;
							if (c == '\t')
							{
								auto x = (1.0f + std::floor((1.0f + cx) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
								width = x - cx;
							}
							else
							{
								char buf2[2];
								buf2[0] = line[cindex].mChar;
								buf2[1] = '\0';
								width = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf2).x;
							}
						}
						ImVec2 cstart(textScreenPos.x + cx, lineStartScreenPos.y);
						ImVec2 cend(textScreenPos.x + cx + width, lineStartScreenPos.y + mCharAdvance.y);
						drawList->AddRectFilled(cstart, cend, mPalette[(int)PaletteIndex::Cursor]);
						if (elapsed > 800)
							mStartTime = timeEnd;
					}
				}
			}

			// Render colorized text
			auto prevColor = line.empty() ? mPalette[(int)PaletteIndex::Default] : GetGlyphColor(line[0]);
			ImVec2 bufferOffset;

			for (int i = 0; i < line.size();)
			{
				auto& glyph = line[i];
				auto color = GetGlyphColor(glyph);

				if ((color != prevColor || glyph.mChar == '\t' || glyph.mChar == ' ') && !mLineBuffer.empty())
				{
					const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
					drawList->AddText(newOffset, prevColor, mLineBuffer.c_str());
					auto textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, mLineBuffer.c_str(), nullptr, nullptr);
					bufferOffset.x += textSize.x;
					mLineBuffer.clear();
				}
				prevColor = color;

				if (glyph.mChar == '\t')
				{
					auto oldX = bufferOffset.x;
					bufferOffset.x = (1.0f + std::floor((1.0f + bufferOffset.x) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
					++i;

					if (mShowWhitespaces)
					{
						ImVec2 p1, p2, p3, p4;

						if (mShowShortTabGlyphs)
						{
							const auto s = ImGui::GetFontSize();
							const auto x1 = textScreenPos.x + oldX + 1.0f;
							const auto x2 = textScreenPos.x + oldX + mCharAdvance.x - 1.0f;
							const auto y = textScreenPos.y + bufferOffset.y + s * 0.5f;

							p1 = ImVec2(x1, y);
							p2 = ImVec2(x2, y);
							p3 = ImVec2(x2 - s * 0.16f, y - s * 0.16f);
							p4 = ImVec2(x2 - s * 0.16f, y + s * 0.16f);
						}
						else
						{
							const auto s = ImGui::GetFontSize();
							const auto x1 = textScreenPos.x + oldX + 1.0f;
							const auto x2 = textScreenPos.x + bufferOffset.x - 1.0f;
							const auto y = textScreenPos.y + bufferOffset.y + s * 0.5f;

							p1 = ImVec2(x1, y);
							p2 = ImVec2(x2, y);
							p3 = ImVec2(x2 - s * 0.2f, y - s * 0.2f);
							p4 = ImVec2(x2 - s * 0.2f, y + s * 0.2f);
						}

						drawList->AddLine(p1, p2, 0x90909090);
						drawList->AddLine(p2, p3, 0x90909090);
						drawList->AddLine(p2, p4, 0x90909090);
					}
				}
				else if (glyph.mChar == ' ')
				{
					if (mShowWhitespaces)
					{
						const auto s = ImGui::GetFontSize();
						const auto x = textScreenPos.x + bufferOffset.x + spaceSize * 0.5f;
						const auto y = textScreenPos.y + bufferOffset.y + s * 0.5f;
						drawList->AddCircleFilled(ImVec2(x, y), 1.5f, 0x80808080, 4);
					}
					bufferOffset.x += spaceSize;
					i++;
				}
				else
				{
					auto l = UTF8CharLength(glyph.mChar);
					while (l-- > 0)
						mLineBuffer.push_back(line[i++].mChar);
				}
				++columnNo;
			}

			if (!mLineBuffer.empty())
			{
				const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
				drawList->AddText(newOffset, prevColor, mLineBuffer.c_str());
				mLineBuffer.clear();
			}

			++lineNo;
		}

		// Draw a tooltip on known identifiers/preprocessor symbols
		if (ImGui::IsMousePosValid() && ImGui::IsWindowHovered())
		{
			auto mpos = ImGui::GetMousePos();
			ImVec2 origin = ImGui::GetCursorScreenPos();
			ImVec2 local(mpos.x - origin.x, mpos.y - origin.y);
			//printf("Mouse: pos(%g, %g), origin(%g, %g), local(%g, %g)\n", mpos.x, mpos.y, origin.x, origin.y, local.x, local.y);
			if (local.x >= mTextStart)
			{
				auto pos = ScreenPosToCoordinates(mpos);
				//printf("Coord(%d, %d)\n", pos.mLine, pos.mColumn);
				auto id = GetWordAt(pos);
				if (!id.empty())
				{
					auto str_rect = GetWordRectAt(mpos);
					ImGui::ItemSize(str_rect.GetSize());
					ImGui::ItemAdd(str_rect, ImGui::GetID(id.c_str()));
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
					{
						auto it = mLanguageDefinition.mIdentifiers.find(id);
						if (it != mLanguageDefinition.mIdentifiers.end() && ImGui::BeginTooltip())
						{
							ImGui::TextUnformatted(it->second.mDeclaration.c_str());
							ImGui::EndTooltip();
						}
						else
						{
							auto pi = mLanguageDefinition.mPreprocIdentifiers.find(id);
							if (pi != mLanguageDefinition.mPreprocIdentifiers.end() && ImGui::BeginTooltip())
							{
								ImGui::TextUnformatted(pi->second.mDeclaration.c_str());
								ImGui::EndTooltip();
							}
						}
					}
				}
			}
		}
	}


	ImGui::Dummy(ImVec2((longest + 2), mLines.size() * mCharAdvance.y));

	if (mScrollToCursor)
	{
		EnsureCursorVisible();
		ImGui::SetWindowFocus();
		mScrollToCursor = false;
	}
}

void TextEditor::Render(const char* aTitle, const ImVec2& aSize, bool aBorder)
{
	mWithinRender = true;
	mTextChanged = false;
	mCursorPositionChanged = false;

	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(mPalette[(int)PaletteIndex::Background]));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
	if (!mIgnoreImGuiChild)
		ImGui::BeginChild(aTitle, aSize, aBorder, ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavInputs);

	if (mHandleKeyboardInputs)
	{
		HandleKeyboardInputs();
		ImGui::PushAllowKeyboardFocus(true);
	}

	if (mHandleMouseInputs)
		HandleMouseInputs();

	ColorizeInternal();
	Render();

	if (mHandleKeyboardInputs)
		ImGui::PopAllowKeyboardFocus();

	if (!mIgnoreImGuiChild)
		ImGui::EndChild();

	ImGui::PopStyleVar();
	ImGui::PopStyleColor();

	mWithinRender = false;
}

void TextEditor::SetText(const std::string & aText)
{
	mLines.clear();
	mLines.emplace_back(Line());
	for (auto chr : aText)
	{
		if (chr == '\r')
		{
			// ignore the carriage return character
		}
		else if (chr == '\n')
			mLines.emplace_back(Line());
		else
		{
			mLines.back().emplace_back(Glyph(chr, PaletteIndex::Default));
		}
	}

	mTextChanged = true;
	mScrollToTop = true;

	mUndoBuffer.clear();
	mUndoIndex = 0;

	Colorize();
}

void TextEditor::SetTextLines(const std::vector<std::string> & aLines)
{
	mLines.clear();

	if (aLines.empty())
	{
		mLines.emplace_back(Line());
	}
	else
	{
		mLines.resize(aLines.size());

		for (size_t i = 0; i < aLines.size(); ++i)
		{
			const std::string & aLine = aLines[i];

			mLines[i].reserve(aLine.size());
			for (size_t j = 0; j < aLine.size(); ++j)
				mLines[i].emplace_back(Glyph(aLine[j], PaletteIndex::Default));
		}
	}

	mTextChanged = true;
	mScrollToTop = true;

	mUndoBuffer.clear();
	mUndoIndex = 0;

	Colorize();
}

void TextEditor::EnterCharacter(ImWchar aChar, bool aShift)
{
	assert(!mReadOnly);

	UndoRecord u;

	u.mBefore = mState;

	if (HasSelection())
	{
		if (aChar == '\t' && mState.mSelectionStart.mLine != mState.mSelectionEnd.mLine)
		{

			auto start = mState.mSelectionStart;
			auto end = mState.mSelectionEnd;
			auto originalEnd = end;

			if (start > end)
				std::swap(start, end);
			start.mColumn = 0;
			//			end.mColumn = end.mLine < mLines.size() ? mLines[end.mLine].size() : 0;
			if (end.mColumn == 0 && end.mLine > 0)
				--end.mLine;
			if (end.mLine >= (int)mLines.size())
				end.mLine = mLines.empty() ? 0 : (int)mLines.size() - 1;
			end.mColumn = GetLineMaxColumn(end.mLine);

			//if (end.mColumn >= GetLineMaxColumn(end.mLine))
			//	end.mColumn = GetLineMaxColumn(end.mLine) - 1;

			u.mRemovedStart = start;
			u.mRemovedEnd = end;
			u.mRemoved = GetText(start, end);

			bool modified = false;

			for (int i = start.mLine; i <= end.mLine; i++)
			{
				auto& line = mLines[i];
				if (aShift)
				{
					if (!line.empty())
					{
						if (line.front().mChar == '\t')
						{
							line.erase(line.begin());
							modified = true;
						}
						else
						{
							for (int j = 0; j < mTabSize && !line.empty() && line.front().mChar == ' '; j++)
							{
								line.erase(line.begin());
								modified = true;
							}
						}
					}
				}
				else
				{
					line.insert(line.begin(), Glyph('\t', TextEditor::PaletteIndex::Background));
					modified = true;
				}
			}

			if (modified)
			{
				start = Coordinates(start.mLine, GetCharacterColumn(start.mLine, 0));
				Coordinates rangeEnd;
				if (originalEnd.mColumn != 0)
				{
					end = Coordinates(end.mLine, GetLineMaxColumn(end.mLine));
					rangeEnd = end;
					u.mAdded = GetText(start, end);
				}
				else
				{
					end = Coordinates(originalEnd.mLine, 0);
					rangeEnd = Coordinates(end.mLine - 1, GetLineMaxColumn(end.mLine - 1));
					u.mAdded = GetText(start, rangeEnd);
				}

				u.mAddedStart = start;
				u.mAddedEnd = rangeEnd;
				u.mAfter = mState;

				mState.mSelectionStart = start;
				mState.mSelectionEnd = end;
				AddUndo(u);

				mTextChanged = true;

				EnsureCursorVisible();
			}

			return;
		} // c == '\t'
		else
		{
			u.mRemoved = GetSelectedText();
			u.mRemovedStart = mState.mSelectionStart;
			u.mRemovedEnd = mState.mSelectionEnd;
			DeleteSelection();
		}
	} // HasSelection

	auto coord = GetActualCursorCoordinates();
	u.mAddedStart = coord;

	assert(!mLines.empty());

	if (aChar == '\n')
	{
		InsertLine(coord.mLine + 1);
		auto& line = mLines[coord.mLine];
		auto& newLine = mLines[coord.mLine + 1];

		if (mLanguageDefinition.mAutoIndentation)
			for (size_t it = 0; it < line.size() && isascii(line[it].mChar) && isblank(line[it].mChar); ++it)
				newLine.push_back(line[it]);

		const size_t whitespaceSize = newLine.size();
		auto cindex = GetCharacterIndex(coord);
		newLine.insert(newLine.end(), line.begin() + cindex, line.end());
		line.erase(line.begin() + cindex, line.begin() + line.size());
		SetCursorPosition(Coordinates(coord.mLine + 1, GetCharacterColumn(coord.mLine + 1, (int)whitespaceSize)));
		u.mAdded = (char)aChar;
	}
	else
	{
		char buf[7];
		int e = ImTextCharToUtf8(buf, 7, aChar);
		if (e > 0)
		{
			buf[e] = '\0';
			auto& line = mLines[coord.mLine];
			auto cindex = GetCharacterIndex(coord);

			if (mOverwrite && cindex < (int)line.size())
			{
				auto d = UTF8CharLength(line[cindex].mChar);

				u.mRemovedStart = mState.mCursorPosition;
				u.mRemovedEnd = Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex + d));

				while (d-- > 0 && cindex < (int)line.size())
				{
					u.mRemoved += line[cindex].mChar;
					line.erase(line.begin() + cindex);
				}
			}

			for (auto p = buf; *p != '\0'; p++, ++cindex)
				line.insert(line.begin() + cindex, Glyph(*p, PaletteIndex::Default));
			u.mAdded = buf;

			SetCursorPosition(Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex)));
		}
		else
			return;
	}

	mTextChanged = true;

	u.mAddedEnd = GetActualCursorCoordinates();
	u.mAfter = mState;

	AddUndo(u);

	Colorize(coord.mLine - 1, 3);
	EnsureCursorVisible();
}

void TextEditor::SetReadOnly(bool aValue)
{
	mReadOnly = aValue;
}

void TextEditor::SetTextChanged(bool changed)
{
	mTextChanged = changed;
}

void TextEditor::SetColorizerEnable(bool aValue)
{
	mColorizerEnabled = aValue;
}

void TextEditor::SetCursorPosition(const Coordinates & aPosition)
{
	if (mState.mCursorPosition != aPosition)
	{
		mState.mCursorPosition = aPosition;
		mCursorPositionChanged = true;
		EnsureCursorVisible();
	}
}

void TextEditor::SetSelectionStart(const Coordinates & aPosition)
{
	mState.mSelectionStart = SanitizeCoordinates(aPosition);
	if (mState.mSelectionStart > mState.mSelectionEnd)
		std::swap(mState.mSelectionStart, mState.mSelectionEnd);
}

void TextEditor::SetSelectionEnd(const Coordinates & aPosition)
{
	mState.mSelectionEnd = SanitizeCoordinates(aPosition);
	if (mState.mSelectionStart > mState.mSelectionEnd)
		std::swap(mState.mSelectionStart, mState.mSelectionEnd);
}

void TextEditor::SetSelection(const Coordinates & aStart, const Coordinates & aEnd, SelectionMode aMode)
{
	auto oldSelStart = mState.mSelectionStart;
	auto oldSelEnd = mState.mSelectionEnd;

	mState.mSelectionStart = SanitizeCoordinates(aStart);
	mState.mSelectionEnd = SanitizeCoordinates(aEnd);
	if (mState.mSelectionStart > mState.mSelectionEnd)
		std::swap(mState.mSelectionStart, mState.mSelectionEnd);

	switch (aMode)
	{
	case TextEditor::SelectionMode::Normal:
		break;
	case TextEditor::SelectionMode::Word:
	{
		mState.mSelectionStart = FindWordStart(mState.mSelectionStart);
		if (!IsOnWordBoundary(mState.mSelectionEnd))
			mState.mSelectionEnd = FindWordEnd(FindWordStart(mState.mSelectionEnd));
		break;
	}
	case TextEditor::SelectionMode::Line:
	{
		const auto lineNo = mState.mSelectionEnd.mLine;
		const auto lineSize = (size_t)lineNo < mLines.size() ? mLines[lineNo].size() : 0;
		mState.mSelectionStart = Coordinates(mState.mSelectionStart.mLine, 0);
		mState.mSelectionEnd = Coordinates(lineNo, GetLineMaxColumn(lineNo));
		break;
	}
	default:
		break;
	}

	if (mState.mSelectionStart != oldSelStart ||
		mState.mSelectionEnd != oldSelEnd)
		mCursorPositionChanged = true;
}

void TextEditor::SetTabSize(int aValue)
{
	mTabSize = ImMax(0, ImMin(32, aValue));
}

void TextEditor::InsertText(const std::string & aValue)
{
	InsertText(aValue.c_str());
}

void TextEditor::InsertText(const char * aValue)
{
	if (aValue == nullptr)
		return;

	auto pos = GetActualCursorCoordinates();
	auto start = ImMin(pos, mState.mSelectionStart);
	int totalLines = pos.mLine - start.mLine;

	totalLines += InsertTextAt(pos, aValue);

	SetSelection(pos, pos);
	SetCursorPosition(pos);
	Colorize(start.mLine - 1, totalLines + 2);
}

void TextEditor::DeleteSelection()
{
	assert(mState.mSelectionEnd >= mState.mSelectionStart);

	if (mState.mSelectionEnd == mState.mSelectionStart)
		return;

	DeleteRange(mState.mSelectionStart, mState.mSelectionEnd);

	SetSelection(mState.mSelectionStart, mState.mSelectionStart);
	SetCursorPosition(mState.mSelectionStart);
	Colorize(mState.mSelectionStart.mLine, 1);
}

void TextEditor::MoveUp(int aAmount, bool aSelect)
{
	auto oldPos = mState.mCursorPosition;
	mState.mCursorPosition.mLine = ImMax(0, mState.mCursorPosition.mLine - aAmount);
	if (oldPos != mState.mCursorPosition)
	{
		if (aSelect)
		{
			if (oldPos == mInteractiveStart)
				mInteractiveStart = mState.mCursorPosition;
			else if (oldPos == mInteractiveEnd)
				mInteractiveEnd = mState.mCursorPosition;
			else
			{
				mInteractiveStart = mState.mCursorPosition;
				mInteractiveEnd = oldPos;
			}
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);

		EnsureCursorVisible();
	}
}

void TextEditor::MoveDown(int aAmount, bool aSelect)
{
	assert(mState.mCursorPosition.mColumn >= 0);
	auto oldPos = mState.mCursorPosition;
	mState.mCursorPosition.mLine = ImMax(0, ImMin((int)mLines.size() - 1, mState.mCursorPosition.mLine + aAmount));

	if (mState.mCursorPosition != oldPos)
	{
		if (aSelect)
		{
			if (oldPos == mInteractiveEnd)
				mInteractiveEnd = mState.mCursorPosition;
			else if (oldPos == mInteractiveStart)
				mInteractiveStart = mState.mCursorPosition;
			else
			{
				mInteractiveStart = oldPos;
				mInteractiveEnd = mState.mCursorPosition;
			}
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);

		EnsureCursorVisible();
	}
}

static bool IsUTFSequence(char c)
{
	return (c & 0xC0) == 0x80;
}

void TextEditor::MoveLeft(int aAmount, bool aSelect, bool aWordMode)
{
	if (mLines.empty())
		return;

	auto oldPos = mState.mCursorPosition;
	mState.mCursorPosition = GetActualCursorCoordinates();
	auto line = mState.mCursorPosition.mLine;
	auto cindex = GetCharacterIndex(mState.mCursorPosition);

	while (aAmount-- > 0)
	{
		if (cindex == 0)
		{
			if (line > 0)
			{
				--line;
				if ((int)mLines.size() > line)
					cindex = (int)mLines[line].size();
				else
					cindex = 0;
			}
		}
		else
		{
			--cindex;
			if (cindex > 0)
			{
				if ((int)mLines.size() > line)
				{
					while (cindex > 0 && IsUTFSequence(mLines[line][cindex].mChar))
						--cindex;
				}
			}
		}

		mState.mCursorPosition = Coordinates(line, GetCharacterColumn(line, cindex));
		if (aWordMode)
		{
			mState.mCursorPosition = FindWordStart(mState.mCursorPosition);
			cindex = GetCharacterIndex(mState.mCursorPosition);
		}
	}

	mState.mCursorPosition = Coordinates(line, GetCharacterColumn(line, cindex));

	assert(mState.mCursorPosition.mColumn >= 0);
	if (aSelect)
	{
		if (oldPos == mInteractiveStart)
			mInteractiveStart = mState.mCursorPosition;
		else if (oldPos == mInteractiveEnd)
			mInteractiveEnd = mState.mCursorPosition;
		else
		{
			mInteractiveStart = mState.mCursorPosition;
			mInteractiveEnd = oldPos;
		}
	}
	else
		mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
	SetSelection(mInteractiveStart, mInteractiveEnd, aSelect && aWordMode ? SelectionMode::Word : SelectionMode::Normal);

	EnsureCursorVisible();
}

void TextEditor::MoveRight(int aAmount, bool aSelect, bool aWordMode)
{
	auto oldPos = mState.mCursorPosition;

	if (mLines.empty() || oldPos.mLine >= mLines.size())
		return;

	auto cindex = GetCharacterIndex(mState.mCursorPosition);
	while (aAmount-- > 0)
	{
		auto lindex = mState.mCursorPosition.mLine;
		auto& line = mLines[lindex];

		if (cindex >= line.size())
		{
			if (mState.mCursorPosition.mLine < mLines.size() - 1)
			{
				mState.mCursorPosition.mLine = ImMax(0, ImMin((int)mLines.size() - 1, mState.mCursorPosition.mLine + 1));
				mState.mCursorPosition.mColumn = 0;
			}
			else
				return;
		}
		else
		{
			cindex += UTF8CharLength(line[cindex].mChar);
			mState.mCursorPosition = Coordinates(lindex, GetCharacterColumn(lindex, cindex));
			if (aWordMode)
				mState.mCursorPosition = FindNextWord(mState.mCursorPosition);
		}
	}

	if (aSelect)
	{
		if (oldPos == mInteractiveEnd)
			mInteractiveEnd = SanitizeCoordinates(mState.mCursorPosition);
		else if (oldPos == mInteractiveStart)
			mInteractiveStart = mState.mCursorPosition;
		else
		{
			mInteractiveStart = oldPos;
			mInteractiveEnd = mState.mCursorPosition;
		}
	}
	else
		mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
	SetSelection(mInteractiveStart, mInteractiveEnd, aSelect && aWordMode ? SelectionMode::Word : SelectionMode::Normal);

	EnsureCursorVisible();
}

void TextEditor::MoveTop(bool aSelect)
{
	auto oldPos = mState.mCursorPosition;
	SetCursorPosition(Coordinates(0, 0));

	if (mState.mCursorPosition != oldPos)
	{
		if (aSelect)
		{
			mInteractiveEnd = oldPos;
			mInteractiveStart = mState.mCursorPosition;
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);
	}
}

void TextEditor::TextEditor::MoveBottom(bool aSelect)
{
	auto oldPos = GetCursorPosition();
	auto newPos = Coordinates((int)mLines.size() - 1, 0);
	SetCursorPosition(newPos);
	if (aSelect)
	{
		mInteractiveStart = oldPos;
		mInteractiveEnd = newPos;
	}
	else
		mInteractiveStart = mInteractiveEnd = newPos;
	SetSelection(mInteractiveStart, mInteractiveEnd);
}

void TextEditor::MoveHome(bool aSelect)
{
	auto oldPos = mState.mCursorPosition;
	SetCursorPosition(Coordinates(mState.mCursorPosition.mLine, 0));

	if (mState.mCursorPosition != oldPos)
	{
		if (aSelect)
		{
			if (oldPos == mInteractiveStart)
				mInteractiveStart = mState.mCursorPosition;
			else if (oldPos == mInteractiveEnd)
				mInteractiveEnd = mState.mCursorPosition;
			else
			{
				mInteractiveStart = mState.mCursorPosition;
				mInteractiveEnd = oldPos;
			}
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);
	}
}

void TextEditor::MoveEnd(bool aSelect)
{
	auto oldPos = mState.mCursorPosition;
	SetCursorPosition(Coordinates(mState.mCursorPosition.mLine, GetLineMaxColumn(oldPos.mLine)));

	if (mState.mCursorPosition != oldPos)
	{
		if (aSelect)
		{
			if (oldPos == mInteractiveEnd)
				mInteractiveEnd = mState.mCursorPosition;
			else if (oldPos == mInteractiveStart)
				mInteractiveStart = mState.mCursorPosition;
			else
			{
				mInteractiveStart = oldPos;
				mInteractiveEnd = mState.mCursorPosition;
			}
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);
	}
}

void TextEditor::Delete()
{
	assert(!mReadOnly);

	if (mLines.empty())
		return;

	UndoRecord u;
	u.mBefore = mState;

	if (HasSelection())
	{
		u.mRemoved = GetSelectedText();
		u.mRemovedStart = mState.mSelectionStart;
		u.mRemovedEnd = mState.mSelectionEnd;

		DeleteSelection();
	}
	else
	{
		auto pos = GetActualCursorCoordinates();
		SetCursorPosition(pos);
		auto& line = mLines[pos.mLine];

		if (pos.mColumn == GetLineMaxColumn(pos.mLine))
		{
			if (pos.mLine == (int)mLines.size() - 1)
				return;

			u.mRemoved = '\n';
			u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
			Advance(u.mRemovedEnd);

			auto& nextLine = mLines[pos.mLine + 1];
			line.insert(line.end(), nextLine.begin(), nextLine.end());
			RemoveLine(pos.mLine + 1);
		}
		else
		{
			auto cindex = GetCharacterIndex(pos);
			u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
			u.mRemovedEnd.mColumn++;
			u.mRemoved = GetText(u.mRemovedStart, u.mRemovedEnd);

			auto d = UTF8CharLength(line[cindex].mChar);
			while (d-- > 0 && cindex < (int)line.size())
				line.erase(line.begin() + cindex);
		}

		mTextChanged = true;

		Colorize(pos.mLine, 1);
	}

	u.mAfter = mState;
	AddUndo(u);
}

void TextEditor::Backspace()
{
	assert(!mReadOnly);

	if (mLines.empty())
		return;

	UndoRecord u;
	u.mBefore = mState;

	if (HasSelection())
	{
		u.mRemoved = GetSelectedText();
		u.mRemovedStart = mState.mSelectionStart;
		u.mRemovedEnd = mState.mSelectionEnd;

		DeleteSelection();
	}
	else
	{
		auto pos = GetActualCursorCoordinates();
		SetCursorPosition(pos);

		if (mState.mCursorPosition.mColumn == 0)
		{
			if (mState.mCursorPosition.mLine == 0)
				return;

			u.mRemoved = '\n';
			u.mRemovedStart = u.mRemovedEnd = Coordinates(pos.mLine - 1, GetLineMaxColumn(pos.mLine - 1));
			Advance(u.mRemovedEnd);

			auto& line = mLines[mState.mCursorPosition.mLine];
			auto& prevLine = mLines[mState.mCursorPosition.mLine - 1];
			auto prevSize = GetLineMaxColumn(mState.mCursorPosition.mLine - 1);
			prevLine.insert(prevLine.end(), line.begin(), line.end());

			ErrorMarkers etmp;
			for (auto& i : mErrorMarkers)
				etmp.insert(ErrorMarkers::value_type(i.first - 1 == mState.mCursorPosition.mLine ? i.first - 1 : i.first, i.second));
			mErrorMarkers = std::move(etmp);

			RemoveLine(mState.mCursorPosition.mLine);
			--mState.mCursorPosition.mLine;
			mState.mCursorPosition.mColumn = prevSize;
		}
		else
		{
			auto& line = mLines[mState.mCursorPosition.mLine];
			auto cindex = GetCharacterIndex(pos) - 1;
			auto cend = cindex + 1;
			while (cindex > 0 && IsUTFSequence(line[cindex].mChar))
				--cindex;

			//if (cindex > 0 && UTF8CharLength(line[cindex].mChar) > 1)
			//	--cindex;

			u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
			--u.mRemovedStart.mColumn;

			if (line[cindex].mChar == '\t')
				mState.mCursorPosition.mColumn -= mTabSize;
			else
				--mState.mCursorPosition.mColumn;

			while (cindex < line.size() && cend-- > cindex)
			{
				u.mRemoved += line[cindex].mChar;
				line.erase(line.begin() + cindex);
			}
		}

		mTextChanged = true;

		EnsureCursorVisible();
		Colorize(mState.mCursorPosition.mLine, 1);
	}

	u.mAfter = mState;
	AddUndo(u);
}

void TextEditor::SelectWordUnderCursor()
{
	auto c = GetCursorPosition();
	SetSelection(FindWordStart(c), FindWordEnd(c));
}

void TextEditor::SelectAll()
{
	SetSelection(Coordinates(0, 0), Coordinates((int)mLines.size(), 0));
}

bool TextEditor::HasSelection() const
{
	return mState.mSelectionEnd > mState.mSelectionStart;
}

void TextEditor::Copy()
{
	if (HasSelection())
	{
		ImGui::SetClipboardText(GetSelectedText().c_str());
	}
	else
	{
		if (!mLines.empty())
		{
			std::string str;
			auto& line = mLines[GetActualCursorCoordinates().mLine];
			for (auto& g : line)
				str.push_back(g.mChar);
			ImGui::SetClipboardText(str.c_str());
		}
	}
}

void TextEditor::Cut()
{
	if (IsReadOnly())
	{
		Copy();
	}
	else
	{
		if (HasSelection())
		{
			UndoRecord u;
			u.mBefore = mState;
			u.mRemoved = GetSelectedText();
			u.mRemovedStart = mState.mSelectionStart;
			u.mRemovedEnd = mState.mSelectionEnd;

			Copy();
			DeleteSelection();

			u.mAfter = mState;
			AddUndo(u);
		}
	}
}

void TextEditor::Paste()
{
	if (IsReadOnly())
		return;

	auto clipText = ImGui::GetClipboardText();
	if (clipText != nullptr && strlen(clipText) > 0)
	{
		UndoRecord u;
		u.mBefore = mState;

		if (HasSelection())
		{
			u.mRemoved = GetSelectedText();
			u.mRemovedStart = mState.mSelectionStart;
			u.mRemovedEnd = mState.mSelectionEnd;
			DeleteSelection();
		}

		u.mAdded = clipText;
		u.mAddedStart = GetActualCursorCoordinates();

		InsertText(clipText);

		u.mAddedEnd = GetActualCursorCoordinates();
		u.mAfter = mState;
		AddUndo(u);
	}
}

bool TextEditor::CanUndo() const
{
	return !mReadOnly && mUndoIndex > 0;
}

bool TextEditor::CanRedo() const
{
	return !mReadOnly && mUndoIndex < (int)mUndoBuffer.size();
}

void TextEditor::Undo(int aSteps)
{
	while (CanUndo() && aSteps-- > 0)
		mUndoBuffer[--mUndoIndex].Undo(this);
}

void TextEditor::Redo(int aSteps)
{
	while (CanRedo() && aSteps-- > 0)
		mUndoBuffer[mUndoIndex++].Redo(this);
}

const TextEditor::Palette & TextEditor::GetDarkPalette()
{
	const static Palette p = { {
			0xff7f7f7f,	// Default
			0xffd69c56,	// Keyword	
			0xff00ff00,	// Number
			0xff7070e0,	// String
			0xff70a0e0, // Char literal
			0xffffffff, // Punctuation
			0xff408080,	// Preprocessor
			0xffaaaaaa, // Identifier
			0xff9bc64d, // Known identifier
			0xffc040a0, // Preproc identifier
			0xff206020, // Comment (single line)
			0xff406020, // Comment (multi line)
			0xff101010, // Background
			0xffe0e0e0, // Cursor
			0x80a06020, // Selection
			0x800020ff, // ErrorMarker
			0x40f08000, // Breakpoint
			0xff707000, // Line number
			0x40000000, // Current line fill
			0x40808080, // Current line fill (inactive)
			0x40a0a0a0, // Current line edge
		} };
	return p;
}

const TextEditor::Palette & TextEditor::GetLightPalette()
{
	const static Palette p = { {
			0xff7f7f7f,	// None
			0xffff0c06,	// Keyword	
			0xff008000,	// Number
			0xff2020a0,	// String
			0xff304070, // Char literal
			0xff000000, // Punctuation
			0xff406060,	// Preprocessor
			0xff404040, // Identifier
			0xff606010, // Known identifier
			0xffc040a0, // Preproc identifier
			0xff205020, // Comment (single line)
			0xff405020, // Comment (multi line)
			0xffffffff, // Background
			0xff000000, // Cursor
			0x80600000, // Selection
			0xa00010ff, // ErrorMarker
			0x80f08000, // Breakpoint
			0xff505000, // Line number
			0x40000000, // Current line fill
			0x40808080, // Current line fill (inactive)
			0x40000000, // Current line edge
		} };
	return p;
}

const TextEditor::Palette & TextEditor::GetRetroBluePalette()
{
	const static Palette p = { {
			0xff00ffff,	// None
			0xffffff00,	// Keyword	
			0xff00ff00,	// Number
			0xff808000,	// String
			0xff808000, // Char literal
			0xffffffff, // Punctuation
			0xff008000,	// Preprocessor
			0xff00ffff, // Identifier
			0xffffffff, // Known identifier
			0xffff00ff, // Preproc identifier
			0xff808080, // Comment (single line)
			0xff404040, // Comment (multi line)
			0xff800000, // Background
			0xff0080ff, // Cursor
			0x80ffff00, // Selection
			0xa00000ff, // ErrorMarker
			0x80ff8000, // Breakpoint
			0xff808000, // Line number
			0x40000000, // Current line fill
			0x40808080, // Current line fill (inactive)
			0x40000000, // Current line edge
		} };
	return p;
}


std::string TextEditor::GetText() const
{
	auto lastLine = (int)mLines.size() - 1;
	auto lastLineLength = GetLineMaxColumn(lastLine);
	return GetText(Coordinates(), Coordinates(lastLine, lastLineLength));
}

std::vector<std::string> TextEditor::GetTextLines() const
{
	std::vector<std::string> result;

	result.reserve(mLines.size());

	for (auto & line : mLines)
	{
		std::string text;

		text.resize(line.size());

		for (size_t i = 0; i < line.size(); ++i)
			text[i] = line[i].mChar;

		result.emplace_back(std::move(text));
	}

	return result;
}

std::string TextEditor::GetSelectedText() const
{
	return GetText(mState.mSelectionStart, mState.mSelectionEnd);
}

std::string TextEditor::GetCurrentLineText()const
{
	auto lineLength = GetLineMaxColumn(mState.mCursorPosition.mLine);
	return GetText(
		Coordinates(mState.mCursorPosition.mLine, 0),
		Coordinates(mState.mCursorPosition.mLine, lineLength));
}

void TextEditor::ProcessInputs()
{
}

void TextEditor::Colorize(int aFromLine, int aLines)
{
	int toLine = aLines == -1 ? (int)mLines.size() : ImMin((int)mLines.size(), aFromLine + aLines);
	mColorRangeMin = ImMin(mColorRangeMin, aFromLine);
	mColorRangeMax = ImMax(mColorRangeMax, toLine);
	mColorRangeMin = ImMax(0, mColorRangeMin);
	mColorRangeMax = ImMax(mColorRangeMin, mColorRangeMax);
	mCheckComments = true;
}

void TextEditor::ColorizeRange(int aFromLine, int aToLine)
{
	if (mLines.empty() || aFromLine >= aToLine)
		return;

	std::string buffer;
	std::cmatch results;
	std::string id;

	int endLine = ImMax(0, ImMin((int)mLines.size(), aToLine));
	for (int i = aFromLine; i < endLine; ++i)
	{
		auto& line = mLines[i];

		if (line.empty())
			continue;

		buffer.resize(line.size());
		for (size_t j = 0; j < line.size(); ++j)
		{
			auto& col = line[j];
			buffer[j] = col.mChar;
			col.mColorIndex = PaletteIndex::Default;
		}

		const char * bufferBegin = &buffer.front();
		const char * bufferEnd = bufferBegin + buffer.size();

		auto last = bufferEnd;

		for (auto first = bufferBegin; first != last; )
		{
			const char * token_begin = nullptr;
			const char * token_end = nullptr;
			PaletteIndex token_color = PaletteIndex::Default;

			bool hasTokenizeResult = false;

			if (mLanguageDefinition.mTokenize != nullptr)
			{
				if (mLanguageDefinition.mTokenize(first, last, token_begin, token_end, token_color))
					hasTokenizeResult = true;
			}

			if (hasTokenizeResult == false)
			{
				// todo : remove
				//printf("using regex for %.*s\n", first + 10 < last ? 10 : int(last - first), first);

				for (auto& p : mRegexList)
				{
					if (std::regex_search(first, last, results, p.first, std::regex_constants::match_continuous))
					{
						hasTokenizeResult = true;

						auto& v = *results.begin();
						token_begin = v.first;
						token_end = v.second;
						token_color = p.second;
						break;
					}
				}
			}

			if (hasTokenizeResult == false)
			{
				first++;
			}
			else
			{
				const size_t token_length = token_end - token_begin;

				if (token_color == PaletteIndex::Identifier)
				{
					id.assign(token_begin, token_end);

					// todo : allmost all language definitions use lower case to specify keywords, so shouldn't this use ::tolower ?
					if (!mLanguageDefinition.mCaseSensitive)
						std::transform(id.begin(), id.end(), id.begin(), ::toupper);

					if (!line[first - bufferBegin].mPreprocessor)
					{
						if (mLanguageDefinition.mKeywords.count(id) != 0)
							token_color = PaletteIndex::Keyword;
						else if (mLanguageDefinition.mIdentifiers.count(id) != 0)
							token_color = PaletteIndex::KnownIdentifier;
						else if (mLanguageDefinition.mPreprocIdentifiers.count(id) != 0)
							token_color = PaletteIndex::PreprocIdentifier;
					}
					else
					{
						if (mLanguageDefinition.mPreprocIdentifiers.count(id) != 0)
							token_color = PaletteIndex::PreprocIdentifier;
					}
				}

				for (size_t j = 0; j < token_length; ++j)
					line[(token_begin - bufferBegin) + j].mColorIndex = token_color;

				first = token_end;
			}
		}
	}
}

void TextEditor::ColorizeInternal()
{
	if (mLines.empty() || !mColorizerEnabled)
		return;

	if (mCheckComments)
	{
		auto endLine = mLines.size();
		auto endIndex = 0;
		auto commentStartLine = endLine;
		auto commentStartIndex = endIndex;
		auto withinString = false;
		auto withinSingleLineComment = false;
		auto withinPreproc = false;
		auto firstChar = true;			// there is no other non-whitespace characters in the line before
		auto concatenate = false;		// '\' on the very end of the line
		auto currentLine = 0;
		auto currentIndex = 0;
		while (currentLine < endLine || currentIndex < endIndex)
		{
			auto& line = mLines[currentLine];

			if (currentIndex == 0 && !concatenate)
			{
				withinSingleLineComment = false;
				withinPreproc = false;
				firstChar = true;
			}

			concatenate = false;

			if (!line.empty())
			{
				auto& g = line[currentIndex];
				auto c = g.mChar;

				if (c != mLanguageDefinition.mPreprocChar && !isspace(c))
					firstChar = false;

				if (currentIndex == (int)line.size() - 1 && line[line.size() - 1].mChar == '\\')
					concatenate = true;

				bool inComment = (commentStartLine < currentLine || (commentStartLine == currentLine && commentStartIndex <= currentIndex));

				if (withinString)
				{
					line[currentIndex].mMultiLineComment = inComment;

					if (c == '\"')
					{
						if (currentIndex + 1 < (int)line.size() && line[currentIndex + 1].mChar == '\"')
						{
							currentIndex += 1;
							if (currentIndex < (int)line.size())
								line[currentIndex].mMultiLineComment = inComment;
						}
						else
							withinString = false;
					}
					else if (c == '\\')
					{
						currentIndex += 1;
						if (currentIndex < (int)line.size())
							line[currentIndex].mMultiLineComment = inComment;
					}
				}
				else
				{
					if (firstChar && c == mLanguageDefinition.mPreprocChar)
						withinPreproc = true;

					if (c == '\"')
					{
						withinString = true;
						line[currentIndex].mMultiLineComment = inComment;
					}
					else
					{
						auto pred = [](const char& a, const Glyph& b) { return a == b.mChar; };
						auto from = line.begin() + currentIndex;
						auto& startStr = mLanguageDefinition.mCommentStart;
						auto& singleStartStr = mLanguageDefinition.mSingleLineComment;

						if (!withinSingleLineComment && currentIndex + startStr.size() <= line.size() &&
							equals(startStr.begin(), startStr.end(), from, from + startStr.size(), pred))
						{
							commentStartLine = currentLine;
							commentStartIndex = currentIndex;
						}
						else if (singleStartStr.size() > 0 &&
							currentIndex + singleStartStr.size() <= line.size() &&
							equals(singleStartStr.begin(), singleStartStr.end(), from, from + singleStartStr.size(), pred))
						{
							withinSingleLineComment = true;
						}

						inComment = inComment = (commentStartLine < currentLine || (commentStartLine == currentLine && commentStartIndex <= currentIndex));

						line[currentIndex].mMultiLineComment = inComment;
						line[currentIndex].mComment = withinSingleLineComment;

						auto& endStr = mLanguageDefinition.mCommentEnd;
						if (currentIndex + 1 >= (int)endStr.size() &&
							equals(endStr.begin(), endStr.end(), from + 1 - endStr.size(), from + 1, pred))
						{
							commentStartIndex = endIndex;
							commentStartLine = endLine;
						}
					}
				}
				line[currentIndex].mPreprocessor = withinPreproc;
				currentIndex += UTF8CharLength(c);
				if (currentIndex >= (int)line.size())
				{
					currentIndex = 0;
					++currentLine;
				}
			}
			else
			{
				currentIndex = 0;
				++currentLine;
			}
		}
		mCheckComments = false;
	}

	if (mColorRangeMin < mColorRangeMax)
	{
		const int increment = (mLanguageDefinition.mTokenize == nullptr) ? 10 : 10000;
		const int to = ImMin(mColorRangeMin + increment, mColorRangeMax);
		ColorizeRange(mColorRangeMin, to);
		mColorRangeMin = to;

		if (mColorRangeMax == mColorRangeMin)
		{
			mColorRangeMin = INT_MAX;// std::numeric_limits<int>::max();
			mColorRangeMax = 0;
		}
		return;
	}
}

float TextEditor::TextDistanceToLineStart(const Coordinates& aFrom) const
{
	auto& line = mLines[aFrom.mLine];
	float distance = 0.0f;
	float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;
	int colIndex = GetCharacterIndex(aFrom);
	for (size_t it = 0u; it < line.size() && it < colIndex; )
	{
		if (line[it].mChar == '\t')
		{
			distance = (1.0f + std::floor((1.0f + distance) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
			++it;
		}
		else
		{
			auto d = UTF8CharLength(line[it].mChar);
			char tempCString[7];
			int i = 0;
			for (; i < 6 && d-- > 0 && it < (int)line.size(); i++, it++)
				tempCString[i] = line[it].mChar;

			tempCString[i] = '\0';
			distance += ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, tempCString, nullptr, nullptr).x;
		}
	}

	return distance;
}

void TextEditor::EnsureCursorVisible()
{
	if (!mWithinRender)
	{
		mScrollToCursor = true;
		return;
	}

	float scrollX = ImGui::GetScrollX();
	float scrollY = ImGui::GetScrollY();

	auto window_size = ImGui::GetContentRegionAvail();
	auto height = window_size.y;
	auto width = window_size.x;

	auto top = 1 + (int)ceil(scrollY / mCharAdvance.y);
	auto bottom = (int)ceil((scrollY + height) / mCharAdvance.y);

	auto left = (int)ceil(scrollX / mCharAdvance.x);
	auto right = (int)ceil((scrollX + width) / mCharAdvance.x);

	auto pos = GetActualCursorCoordinates();
	auto len = TextDistanceToLineStart(pos);

	if (pos.mLine < top)
		ImGui::SetScrollY(ImMax(0.0f, (pos.mLine - 1) * mCharAdvance.y));
	if (pos.mLine > bottom - 4)
		ImGui::SetScrollY(ImMax(0.0f, (pos.mLine + 4) * mCharAdvance.y - height));
	if (len + mTextStart < left * mCharAdvance.x + 4)
		ImGui::SetScrollX(ImMax(0.0f, len + mTextStart - 4));
	if (len + mTextStart > right * mCharAdvance.x - 4)
		ImGui::SetScrollX(ImMax(0.0f, len + mTextStart + 4 - width));
}

int TextEditor::GetPageSize() const
{
	auto height = ImGui::GetWindowHeight() - 20.0f;
	return (int)floor(height / mCharAdvance.y);
}

TextEditor::UndoRecord::UndoRecord(
	const std::string& aAdded,
	const TextEditor::Coordinates aAddedStart,
	const TextEditor::Coordinates aAddedEnd,
	const std::string& aRemoved,
	const TextEditor::Coordinates aRemovedStart,
	const TextEditor::Coordinates aRemovedEnd,
	TextEditor::EditorState& aBefore,
	TextEditor::EditorState& aAfter)
	: mAdded(aAdded)
	, mAddedStart(aAddedStart)
	, mAddedEnd(aAddedEnd)
	, mRemoved(aRemoved)
	, mRemovedStart(aRemovedStart)
	, mRemovedEnd(aRemovedEnd)
	, mBefore(aBefore)
	, mAfter(aAfter)
{
	assert(mAddedStart <= mAddedEnd);
	assert(mRemovedStart <= mRemovedEnd);
}

void TextEditor::UndoRecord::Undo(TextEditor * aEditor)
{
	if (!mAdded.empty())
	{
		aEditor->DeleteRange(mAddedStart, mAddedEnd);
		aEditor->Colorize(mAddedStart.mLine - 1, mAddedEnd.mLine - mAddedStart.mLine + 2);
	}

	if (!mRemoved.empty())
	{
		auto start = mRemovedStart;
		aEditor->InsertTextAt(start, mRemoved.c_str());
		aEditor->Colorize(mRemovedStart.mLine - 1, mRemovedEnd.mLine - mRemovedStart.mLine + 2);
	}

	aEditor->mState = mBefore;
	aEditor->EnsureCursorVisible();

}

void TextEditor::UndoRecord::Redo(TextEditor * aEditor)
{
	if (!mRemoved.empty())
	{
		aEditor->DeleteRange(mRemovedStart, mRemovedEnd);
		aEditor->Colorize(mRemovedStart.mLine - 1, mRemovedEnd.mLine - mRemovedStart.mLine + 1);
	}

	if (!mAdded.empty())
	{
		auto start = mAddedStart;
		aEditor->InsertTextAt(start, mAdded.c_str());
		aEditor->Colorize(mAddedStart.mLine - 1, mAddedEnd.mLine - mAddedStart.mLine + 1);
	}

	aEditor->mState = mAfter;
	aEditor->EnsureCursorVisible();
}

static bool TokenizeCStyleString(const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end)
{
	const char * p = in_begin;

	if (*p == '"')
	{
		p++;

		while (p < in_end)
		{
			// handle end of string
			if (*p == '"')
			{
				out_begin = in_begin;
				out_end = p + 1;
				return true;
			}

			// handle escape character for "
			if (*p == '\\' && p + 1 < in_end && p[1] == '"')
				p++;

			p++;
		}
	}

	return false;
}

static bool TokenizeCStyleCharacterLiteral(const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end)
{
	const char * p = in_begin;

	if (*p == '\'')
	{
		p++;

		// handle escape characters
		if (p < in_end && *p == '\\')
			p++;

		if (p < in_end)
			p++;

		// handle end of character literal
		if (p < in_end && *p == '\'')
		{
			out_begin = in_begin;
			out_end = p + 1;
			return true;
		}
	}

	return false;
}

static bool TokenizeCStyleIdentifier(const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end)
{
	const char * p = in_begin;

	if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_')
	{
		p++;

		while ((p < in_end) && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_'))
			p++;

		out_begin = in_begin;
		out_end = p;
		return true;
	}

	return false;
}

static bool TokenizeCStyleNumber(const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end)
{
	const char * p = in_begin;

	const bool startsWithNumber = *p >= '0' && *p <= '9';

	if (*p != '+' && *p != '-' && !startsWithNumber)
		return false;

	p++;

	bool hasNumber = startsWithNumber;

	while (p < in_end && (*p >= '0' && *p <= '9'))
	{
		hasNumber = true;

		p++;
	}

	if (hasNumber == false)
		return false;

	bool isFloat = false;
	bool isHex = false;
	bool isBinary = false;

	if (p < in_end)
	{
		if (*p == '.')
		{
			isFloat = true;

			p++;

			while (p < in_end && (*p >= '0' && *p <= '9'))
				p++;
		}
		else if (*p == 'x' || *p == 'X')
		{
			// hex formatted integer of the type 0xef80

			isHex = true;

			p++;

			while (p < in_end && ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')))
				p++;
		}
		else if (*p == 'b' || *p == 'B')
		{
			// binary formatted integer of the type 0b01011101

			isBinary = true;

			p++;

			while (p < in_end && (*p >= '0' && *p <= '1'))
				p++;
		}
	}

	if (isHex == false && isBinary == false)
	{
		// floating point exponent
		if (p < in_end && (*p == 'e' || *p == 'E'))
		{
			isFloat = true;

			p++;

			if (p < in_end && (*p == '+' || *p == '-'))
				p++;

			bool hasDigits = false;

			while (p < in_end && (*p >= '0' && *p <= '9'))
			{
				hasDigits = true;

				p++;
			}

			if (hasDigits == false)
				return false;
		}

		// single precision floating point type
		if (p < in_end && *p == 'f')
			p++;
	}

	if (isFloat == false)
	{
		// integer size type
		while (p < in_end && (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L'))
			p++;
	}

	out_begin = in_begin;
	out_end = p;
	return true;
}

static bool TokenizeCStylePunctuation(const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end)
{
	(void)in_end;

	switch (*in_begin)
	{
	case '[':
	case ']':
	case '{':
	case '}':
	case '!':
	case '%':
	case '^':
	case '&':
	case '*':
	case '(':
	case ')':
	case '-':
	case '+':
	case '=':
	case '~':
	case '|':
	case '<':
	case '>':
	case '?':
	case ':':
	case '/':
	case ';':
	case ',':
	case '.':
		out_begin = in_begin;
		out_end = in_begin + 1;
		return true;
	}

	return false;
}

static bool TokenizeLuaStyleString(const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end)
{
	const char * p = in_begin;

	bool is_single_quote = false;
	bool is_double_quotes = false;
	bool is_double_square_brackets = false;

	switch (*p)
	{
	case '\'':
		is_single_quote = true;
		break;
	case '"':
		is_double_quotes = true;
		break;
	case '[':
		p++;
		if (p < in_end && *(p) == '[')
			is_double_square_brackets = true;
		break;
	}

	if (is_single_quote || is_double_quotes || is_double_square_brackets)
	{
		p++;

		while (p < in_end)
		{
			// handle end of string
			if ((is_single_quote && *p == '\'') || (is_double_quotes && *p == '"') || (is_double_square_brackets && *p == ']' && p + 1 < in_end && *(p + 1) == ']'))
			{
				out_begin = in_begin;

				if (is_double_square_brackets)
					out_end = p + 2;
				else
					out_end = p + 1;

				return true;
			}

			// handle escape character for "
			if (*p == '\\' && p + 1 < in_end && (is_single_quote || is_double_quotes))
				p++;

			p++;
		}
	}

	return false;
}

static bool TokenizeLuaStyleIdentifier(const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end)
{
	const char * p = in_begin;

	if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_')
	{
		p++;

		while ((p < in_end) && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_'))
			p++;

		out_begin = in_begin;
		out_end = p;
		return true;
	}

	return false;
}

static bool TokenizeLuaStyleNumber(const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end)
{
	const char * p = in_begin;

	const bool startsWithNumber = *p >= '0' && *p <= '9';

	if (*p != '+' && *p != '-' && !startsWithNumber)
		return false;

	p++;

	bool hasNumber = startsWithNumber;

	while (p < in_end && (*p >= '0' && *p <= '9'))
	{
		hasNumber = true;

		p++;
	}

	if (hasNumber == false)
		return false;

	if (p < in_end)
	{
		if (*p == '.')
		{
			p++;

			while (p < in_end && (*p >= '0' && *p <= '9'))
				p++;
		}

		// floating point exponent
		if (p < in_end && (*p == 'e' || *p == 'E'))
		{
			p++;

			if (p < in_end && (*p == '+' || *p == '-'))
				p++;

			bool hasDigits = false;

			while (p < in_end && (*p >= '0' && *p <= '9'))
			{
				hasDigits = true;

				p++;
			}

			if (hasDigits == false)
				return false;
		}
	}

	out_begin = in_begin;
	out_end = p;
	return true;
}

static bool TokenizeLuaStylePunctuation(const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end)
{
	(void)in_end;

	switch (*in_begin)
	{
	case '[':
	case ']':
	case '{':
	case '}':
	case '!':
	case '%':
	case '#':
	case '^':
	case '&':
	case '*':
	case '(':
	case ')':
	case '-':
	case '+':
	case '=':
	case '~':
	case '|':
	case '<':
	case '>':
	case '?':
	case ':':
	case '/':
	case ';':
	case ',':
	case '.':
		out_begin = in_begin;
		out_end = in_begin + 1;
		return true;
	}

	return false;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::CPlusPlus()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const cppKeywords[] = {
			"alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit", "atomic_noexcept", "auto", "bitand", "bitor", "bool", "break", "case", "catch", "char", "char16_t", "char32_t", "class",
			"compl", "concept", "const", "constexpr", "const_cast", "continue", "decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false", "float",
			"for", "friend", "goto", "if", "import", "inline", "int", "long", "module", "mutable", "namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected", "public",
			"register", "reinterpret_cast", "requires", "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct", "switch", "synchronized", "template", "this", "thread_local",
			"throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
		};
		for (auto& k : cppKeywords)
			langDef.mKeywords.insert(k);

		static const char* const identifiers[] = {
			"abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph",
			"ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "printf", "sprintf", "snprintf", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper",
			"std", "string", "vector", "map", "unordered_map", "set", "unordered_set", "min", "max"
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.mTokenize = [](const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end, PaletteIndex & paletteIndex) -> bool
		{
			paletteIndex = PaletteIndex::Max;

			while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
				in_begin++;

			if (in_begin == in_end)
			{
				out_begin = in_end;
				out_end = in_end;
				paletteIndex = PaletteIndex::Default;
			}
			else if (TokenizeCStyleString(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::String;
			else if (TokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::CharLiteral;
			else if (TokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::Identifier;
			else if (TokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::Number;
			else if (TokenizeCStylePunctuation(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::Punctuation;

			return paletteIndex != PaletteIndex::Max;
		};

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";
		langDef.mSingleLineComment = "//";

		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = true;

		langDef.mName = "C++";

		inited = true;
	}
	return langDef;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::HLSL()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"AppendStructuredBuffer", "asm", "asm_fragment", "BlendState", "bool", "break", "Buffer", "ByteAddressBuffer", "case", "cbuffer", "centroid", "class", "column_major", "compile", "compile_fragment",
			"CompileShader", "const", "continue", "ComputeShader", "ConsumeStructuredBuffer", "default", "DepthStencilState", "DepthStencilView", "discard", "do", "double", "DomainShader", "dword", "else",
			"export", "extern", "false", "float", "for", "fxgroup", "GeometryShader", "groupshared", "half", "Hullshader", "if", "in", "inline", "inout", "InputPatch", "int", "interface", "line", "lineadj",
			"linear", "LineStream", "matrix", "min16float", "min10float", "min16int", "min12int", "min16uint", "namespace", "nointerpolation", "noperspective", "NULL", "out", "OutputPatch", "packoffset",
			"pass", "pixelfragment", "PixelShader", "point", "PointStream", "precise", "RasterizerState", "RenderTargetView", "return", "register", "row_major", "RWBuffer", "RWByteAddressBuffer", "RWStructuredBuffer",
			"RWTexture1D", "RWTexture1DArray", "RWTexture2D", "RWTexture2DArray", "RWTexture3D", "sample", "sampler", "SamplerState", "SamplerComparisonState", "shared", "snorm", "stateblock", "stateblock_state",
			"static", "string", "struct", "switch", "StructuredBuffer", "tbuffer", "technique", "technique10", "technique11", "texture", "Texture1D", "Texture1DArray", "Texture2D", "Texture2DArray", "Texture2DMS",
			"Texture2DMSArray", "Texture3D", "TextureCube", "TextureCubeArray", "true", "typedef", "triangle", "triangleadj", "TriangleStream", "uint", "uniform", "unorm", "unsigned", "vector", "vertexfragment",
			"VertexShader", "void", "volatile", "while",
			"bool1","bool2","bool3","bool4","double1","double2","double3","double4", "float1", "float2", "float3", "float4", "int1", "int2", "int3", "int4", "in", "out", "inout",
			"uint1", "uint2", "uint3", "uint4", "dword1", "dword2", "dword3", "dword4", "half1", "half2", "half3", "half4",
			"float1x1","float2x1","float3x1","float4x1","float1x2","float2x2","float3x2","float4x2",
			"float1x3","float2x3","float3x3","float4x3","float1x4","float2x4","float3x4","float4x4",
			"half1x1","half2x1","half3x1","half4x1","half1x2","half2x2","half3x2","half4x2",
			"half1x3","half2x3","half3x3","half4x3","half1x4","half2x4","half3x4","half4x4",
		};
		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static const char* const identifiers[] = {
			"abort", "abs", "acos", "all", "AllMemoryBarrier", "AllMemoryBarrierWithGroupSync", "any", "asdouble", "asfloat", "asin", "asint", "asint", "asuint",
			"asuint", "atan", "atan2", "ceil", "CheckAccessFullyMapped", "clamp", "clip", "cos", "cosh", "countbits", "cross", "D3DCOLORtoUBYTE4", "ddx",
			"ddx_coarse", "ddx_fine", "ddy", "ddy_coarse", "ddy_fine", "degrees", "determinant", "DeviceMemoryBarrier", "DeviceMemoryBarrierWithGroupSync",
			"distance", "dot", "dst", "errorf", "EvaluateAttributeAtCentroid", "EvaluateAttributeAtSample", "EvaluateAttributeSnapped", "exp", "exp2",
			"f16tof32", "f32tof16", "faceforward", "firstbithigh", "firstbitlow", "floor", "fma", "fmod", "frac", "frexp", "fwidth", "GetRenderTargetSampleCount",
			"GetRenderTargetSamplePosition", "GroupMemoryBarrier", "GroupMemoryBarrierWithGroupSync", "InterlockedAdd", "InterlockedAnd", "InterlockedCompareExchange",
			"InterlockedCompareStore", "InterlockedExchange", "InterlockedMax", "InterlockedMin", "InterlockedOr", "InterlockedXor", "isfinite", "isinf", "isnan",
			"ldexp", "length", "lerp", "lit", "log", "log10", "log2", "mad", "max", "min", "modf", "msad4", "mul", "noise", "normalize", "pow", "printf",
			"Process2DQuadTessFactorsAvg", "Process2DQuadTessFactorsMax", "Process2DQuadTessFactorsMin", "ProcessIsolineTessFactors", "ProcessQuadTessFactorsAvg",
			"ProcessQuadTessFactorsMax", "ProcessQuadTessFactorsMin", "ProcessTriTessFactorsAvg", "ProcessTriTessFactorsMax", "ProcessTriTessFactorsMin",
			"radians", "rcp", "reflect", "refract", "reversebits", "round", "rsqrt", "saturate", "sign", "sin", "sincos", "sinh", "smoothstep", "sqrt", "step",
			"tan", "tanh", "tex1D", "tex1D", "tex1Dbias", "tex1Dgrad", "tex1Dlod", "tex1Dproj", "tex2D", "tex2D", "tex2Dbias", "tex2Dgrad", "tex2Dlod", "tex2Dproj",
			"tex3D", "tex3D", "tex3Dbias", "tex3Dgrad", "tex3Dlod", "tex3Dproj", "texCUBE", "texCUBE", "texCUBEbias", "texCUBEgrad", "texCUBElod", "texCUBEproj", "transpose", "trunc"
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Preprocessor));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";
		langDef.mSingleLineComment = "//";

		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = true;

		langDef.mName = "HLSL";

		inited = true;
	}
	return langDef;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::GLSL()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		auto functions_descriptions = [](const std::vector<std::string>& Syntax, const std::vector<std::string>& Description)
		{
			std::string result = "Syntax:\n\t";
			for (auto syn : Syntax)
			{
				result += syn + "\n\t";
			}
			result += "\nDescription:\n\t";
			for (auto desc : Description)
			{
				result += desc + "\n\t";
			}
			result += "\n";
			return result;
		};
		static const char* const keywords[] = {
			"auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short",
			"signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary",
			"_Noreturn", "_Static_assert", "_Thread_local"
		};
		for (auto& k : keywords)
			langDef.mKeywords.insert(k);
	
		{
			static std::map<std::string, std::string> buildin_functions_identifiers;
			// Angle and Trigonometry Functions
			buildin_functions_identifiers.insert(std::make_pair("radians", functions_descriptions({"genType radians(genType degrees)"}, {"Converts degrees to radians, i.e., π/180 * degrees"})));
			buildin_functions_identifiers.insert(std::make_pair("degrees", functions_descriptions({"genType degrees(genType radians)"}, {"Converts radians to degrees, i.e., 180/π * radians"})));
			buildin_functions_identifiers.insert(std::make_pair("sin", functions_descriptions({"genType sin(genType angle)"}, {"The standard trigonometric sine function."})));
			buildin_functions_identifiers.insert(std::make_pair("cos", functions_descriptions({"genType cos(genType angle)"}, {"The standard trigonometric cosine function."})));
			buildin_functions_identifiers.insert(std::make_pair("tan", functions_descriptions({"genType tan(genType angle)"}, {"The standard trigonometric tangent."})));
			buildin_functions_identifiers.insert(std::make_pair("asin", functions_descriptions({"genType asin(genType x)"}, {"Arc sine. Returns an angle whose sine is x.\n\tThe range of values returned by this function is [ -π/2,π/2],\n\tResults are undefined if |x|>1."})));
			buildin_functions_identifiers.insert(std::make_pair("acos", functions_descriptions({"genType acos(genType x)"}, {"Arc cosine. Returns an angle whose cosine is x.\n\tThe range of values returned by this function is [0, π].\n\tResults are undefined if |x|>1."})));
			buildin_functions_identifiers.insert(std::make_pair("atan", functions_descriptions({"genType atan(genType y, genType x)", "genType atan(genType y_over_x)"}, {"Arc tangent. Returns an angle whose tangent is y/x.\n\tThe signs of x and y are used to determine what quadrant the angle is in The range of values returned by this function is [−π,π].\n\tResults are undefined if x and y are both 0.", "Arc tangent. Returns an angle whose tangent is y_over_x.\n\tThe range of values returned by this function is [-π/2,π/2]."})));
			buildin_functions_identifiers.insert(std::make_pair("sinh", functions_descriptions({"genType sinh(genType x)"}, {"Returns the hyperbolic sine function (e^x - e^-x)/2"})));
			buildin_functions_identifiers.insert(std::make_pair("cosh", functions_descriptions({"genType cosh(genType x)"}, {"Returns the hyperbolic cosine function (e^x + e^-x)/2"})));
			buildin_functions_identifiers.insert(std::make_pair("tanh", functions_descriptions({"genType tanh(genType x)"}, {"Returns the hyperbolic tangent function sinh(x)/cosh(x)"})));
			buildin_functions_identifiers.insert(std::make_pair("asinh", functions_descriptions({"genType asinh(genType x)"}, {"Arc hyperbolic sine; returns the inverse of sinh."})));
			buildin_functions_identifiers.insert(std::make_pair("acosh", functions_descriptions({"genType acosh(genType x)"}, {"Arc hyperbolic cosine; returns the non-negative inverse\n\tof cosh. Results are undefined if x < 1."})));
			buildin_functions_identifiers.insert(std::make_pair("atanh", functions_descriptions({"genType atanh(genType x)"}, {"Arc hyperbolic tangent; returns the inverse of tanh.\n\tResults are undefined if |x|≥1."})));
			// Exponential Functions
			buildin_functions_identifiers.insert(std::make_pair("pow", functions_descriptions({"genType pow(genType x, genType y)"}, {"Returns x raised to the y power, i.e., x^y\n\tResults are undefined if x < 0.\n\tResults are undefined if x = 0 and y <= 0."})));
			buildin_functions_identifiers.insert(std::make_pair("exp", functions_descriptions({"genType exp(genType x)"}, {"Returns the natural exponentiation of x, i.e., e^x."})));
			buildin_functions_identifiers.insert(std::make_pair("log", functions_descriptions({"genType log(genType x)"}, {"Returns the natural logarithm of x, i.e., returns the value\n\ty which satisfies the equation x = e^y.\n\tResults are undefined if x <= 0."})));
			buildin_functions_identifiers.insert(std::make_pair("exp2", functions_descriptions({"genType exp2(genType x)"}, {"Returns 2 raised to the x power, i.e., 2^x"})));
			buildin_functions_identifiers.insert(std::make_pair("log2", functions_descriptions({"genType log2(genType x)"}, {"Returns the base 2 logarithm of x, i.e., returns the value\n\ty which satisfies the equation x=2^y\n\tResults are undefined if x <= 0."})));
			buildin_functions_identifiers.insert(std::make_pair("sqrt", functions_descriptions({"genType sqrt(genType x)", "genDType sqrt(genDType x)"}, {"Returns √x .\n\tResults are undefined if x < 0."})));
			buildin_functions_identifiers.insert(std::make_pair("inversesqrt", functions_descriptions({"genType inversesqrt(genType x)", "genDType inversesqrt(genDType x)"}, {"Returns 1 / √x .\n\tResults are undefined if x <= 0."})));
			// Common Functions
			buildin_functions_identifiers.insert(std::make_pair("abs", functions_descriptions({"genType abs(genType x)", "genIType abs(genIType x)", "genDType abs(genDType x)"}, {"Returns x if x >= 0; otherwise it returns –x."})));
			buildin_functions_identifiers.insert(std::make_pair("sign", functions_descriptions({"genType sign(genType x)", "genIType sign(genIType x)", "genDType sign(genDType x)"}, {"Returns 1.0 if x > 0, 0.0 if x = 0, or –1.0 if x < 0."})));
			buildin_functions_identifiers.insert(std::make_pair("floor", functions_descriptions({"genType floor(genType x)", "genDType floor(genDType x)"}, {"Returns a value equal to the nearest integer that is less than or equal to x."})));
			buildin_functions_identifiers.insert(std::make_pair("trunc", functions_descriptions({"genType trunc(genType x)", "genDType trunc(genDType x)"}, {"Returns a value equal to the nearest integer to x whose\n\tabsolute value is not larger than the absolute value of x."})));
			buildin_functions_identifiers.insert(std::make_pair("round", functions_descriptions({"genType round(genType x)", "genDType round(genDType x)"}, {"Returns a value equal to the nearest integer to x.\n\tThe fraction 0.5 will round in a direction chosen by the implementation, presumably the direction that is fastest.\n\tThis includes the possibility that round(x) returns the same value as roundEven(x) for all values of x."})));
			buildin_functions_identifiers.insert(std::make_pair("roundEven", functions_descriptions({"genType roundEven(genType x)", "genDType roundEven(genDType x)"}, {"Returns a value equal to the nearest integer to x.\n\tA fractional part of 0.5 will round toward the nearest even integer.\n\t(Both 3.5 and 4.5 for x will return 4.0.)"})));
			buildin_functions_identifiers.insert(std::make_pair("ceil", functions_descriptions({"genType ceil(genType x)", "genDType ceil(genDType x)"}, {"Returns a value equal to the nearest integer that is greater than or equal to x."})));
			buildin_functions_identifiers.insert(std::make_pair("fract", functions_descriptions({"genType fract(genType x)", "genDType fract(genDType x)"}, {"Returns x - floor (x)."})));
			buildin_functions_identifiers.insert(std::make_pair("mod", functions_descriptions({"genType mod(genType x, float y)", "genType mod(genType x, genType y)", "genDType mod(genDType x, double y)", "genDType mod(genDType x, genDType y)"}, {"Modulus. Returns x - y * floor (x/y)."})));
			buildin_functions_identifiers.insert(std::make_pair("modf", functions_descriptions({"genType modf(genType x, out genType i)", "genDType modf (genDType x, out genDType i)"}, {"Returns the fractional part of x and sets i to the integer part (as a whole number floating-point value).\n\tBoth the return value and the output parameter will have the same sign as x."})));
			buildin_functions_identifiers.insert(std::make_pair("min", functions_descriptions({"genType min(genType x, genType y)", "genType min(genType x, float y)", "genDType min(genDType x, genDType y)", "genDType min(genDType x, double y)", "genIType min(genIType x, genIType y)", "genIType min(genIType x, int y)", "genUType min(genUType x, genUType y)", "genUType min(genUType x, uint y)"}, {"Returns y if y < x; otherwise it returns x."})));
			buildin_functions_identifiers.insert(std::make_pair("max", functions_descriptions({"genType max(genType x, genType y)", "genType max(genType x, float y)", "genDType max(genDType x, genDType y)", "genDType max(genDType x, double y)", "genIType max(genIType x, genIType y)", "genIType max(genIType x, int y)", "genUType max(genUType x, genUType y)", "genUType max(genUType x, uint y)"}, {"Returns y if x < y; otherwise it returns x."})));
			buildin_functions_identifiers.insert(std::make_pair("clamp", functions_descriptions({"genType clamp(genType x, genType minVal, genType maxVal)", "genType clamp(genType x, float minVal, float maxVal)", "genDType clamp(genDType x, genDType minVal, genDType maxVal)", "genDType clamp(genDType x, double minVal, double maxVal)", "genIType clamp(genIType x, genIType minVal, genIType maxVal)", "genIType clamp(genIType x, int minVal, int maxVal)", "genUType clamp(genUType x, genUType minVal, genUType maxVal)", "genUType clamp (genUType x, uint minVal, uint maxVal)"}, {"Returns min (max (x, minVal), maxVal).\n\tResults are undefined if minVal > maxVal."})));
			buildin_functions_identifiers.insert(std::make_pair("mix", functions_descriptions({"genType mix(genType x, genType y, genType a)", "genType mix(genType x, genType y, float a)", "genDType mix(genDType x, genDType y, genDType a)", "genDType mix(genDType x, genDType y, double a)", "genType mix(genType x, genType y, genBType a)", "genDType mix(genDType x, genDType y, genBType a)"}, {"Returns the linear blend of x and y, i.e., x⋅(1-a)+ y⋅a\n\tSelects which vector each returned component comes from.\n\tFor a component of a that is false, the corresponding component of x is returned.\n\tFor a component of a that is true, the corresponding component of y is returned.\n\tComponents of x and y that are not selected are allowed to be invalid floating-point values and will have no effect on the results.\n\tThus, this provides different functionality than, for example,\n\tgenType mix(genType x, genType y, genType(a)) where a is a Boolean vector."})));
			buildin_functions_identifiers.insert(std::make_pair("step", functions_descriptions({"genType step(genType edge, genType x)", "genType step(float edge, genType x)", "genDType step(genDType edge, genDType x)", "genDType step(double edge, genDType x)"}, {"Returns 0.0 if x < edge; otherwise it returns 1.0."})));
			buildin_functions_identifiers.insert(std::make_pair("smoothstep", functions_descriptions({"genType smoothstep(genType edge0, genType edge1, genType x)", "genType smoothstep(float edge0, float edge1, genType x)", "genDType smoothstep(genDType edge0, genDType edge1, genDType x)", "genDType smoothstep(double edge0, double edge1, genDType x)"}, {"Returns 0.0 if x <= edge0 and 1.0 if x >= edge1 and performs smooth Hermite interpolation between 0 and 1 when edge0 < x < edge1.\n\tThis is useful in cases where you would want a threshold function with a smooth transition. This is equivalent to: \n\t\tgenType t;\n\t\tt = clamp ((x - edge0) / (edge1 - edge0), 0, 1);\n\t\treturn t * t * (3 - 2 * t);\n\t(And similarly for doubles.) Results are undefined if edge0 >= edge1."})));
			buildin_functions_identifiers.insert(std::make_pair("isnan", functions_descriptions({"genBType isnan(genType x)", "genBType isnan(genDType x)"}, {"Returns true if x holds a NaN.\n\tReturns false otherwise.\n\tAlways returns false if NaNs are not implemented."})));
			buildin_functions_identifiers.insert(std::make_pair("isinf", functions_descriptions({"genBType isinf(genType x)", "genBType isinf(genDType x)"}, {"Returns true if x holds a positive infinity or negative infinity.\n\tReturns false otherwise."})));
			buildin_functions_identifiers.insert(std::make_pair("floatBitsToInt", functions_descriptions({"genIType floatBitsToInt(genType value)"}, {"Returns a signed or unsigned integer value representing the encoding of a float.\n\tThe float value's bit-level representation is preserved."})));
			buildin_functions_identifiers.insert(std::make_pair("floatBitsToUint", functions_descriptions({"genUType floatBitsToUint(genType value)"}, {"Returns a signed or unsigned integer value representing the encoding of a float.\n\tThe float value's bit-level representation is preserved."})));
			buildin_functions_identifiers.insert(std::make_pair("intBitsToFloat", functions_descriptions({"genType intBitsToFloat(genIType value)"}, {"Returns a float value corresponding to a signed or unsigned integer encoding of a float.\n\tIf a NaN is passed in, it will not signal, and the resulting value is unspecified.\n\tIf an Inf is passed in, the resulting value is the corresponding Inf."})));
			buildin_functions_identifiers.insert(std::make_pair("uintBitsToFloat", functions_descriptions({"genType uintBitsToFloat(genUType value)"}, {"Returns a float value corresponding to a signed or unsigned integer encoding of a float.\n\tIf a NaN is passed in, it will not signal, and the resulting value is unspecified.\n\tIf an Inf is passed in, the resulting value is the corresponding Inf."})));
			buildin_functions_identifiers.insert(std::make_pair("fma", functions_descriptions({"genType fma(genType a, genType b, genType c)", "genDType fma(genDType a, genDType b, genDType c)"}, {"Computes and returns a*b + c.\n\tIn uses where the return value is eventually consumed by a variable declared as precise:\n\t\tfma() is considered a single operation, whereas the expression “a*b + c” consumed by a variable declared precise is considered two operations.\n\t\tThe precision of fma() can differ from the precision of the expression “a*b + c”.\n\t\tfma() will be computed with the same precision as any other fma() consumed by a precise variable, giving invariant results for the same input values of a, b, and c.\n\tOtherwise, in the absence of precise consumption, there are no special constraints on the number of operations or difference in precision between fma() and the expression “a*b + c”."})));
			buildin_functions_identifiers.insert(std::make_pair("frexp", functions_descriptions({"genType frexp(genType x, out genIType exp)", "genDType frexp(genDType x, out genIType exp)"}, {"Splits x into a floating-point significand in the range [0.5, 1.0) and an integral exponent of two, such that:\n\t\tx= significand⋅2^exponent\n\tThe significand is returned by the function and the exponent is returned in the parameter exp.\n\tFor a floating-point value of zero, the significand and exponent are both zero.\n\tFor a floating-point value that is an infinity or is not a number, the results are undefined.\n\tIf an implementation supports negative 0, frexp(-0) should return -0; otherwise it will return 0."})));
			buildin_functions_identifiers.insert(std::make_pair("ldexp", functions_descriptions({"genType ldexp(genType x, in genIType exp)", "genDType ldexp(genDType x, in genIType exp)"}, {"Builds a floating-point number from x and the corresponding integral exponent of two in exp, returning:\n\t\tsignificand⋅2^exponent\n\tIf this product is too large to be represented in the floating-point type, the result is undefined.\n\tIf exp is greater than +128 (single-precision) or +1024 (double-precision), the value returned is undefined.\n\tIf exp is less than -126 (single-precision) or -1022 (double- precision), the value returned may be flushed to zero.\n\tAdditionally, splitting the value into a significand and exponent using frexp() and then reconstructing a floating-point\n\tvalue using ldexp() should yield the original input for zero and all finite non-denormized values."})));
			// Floating-Point Pack and Unpack Functions (no Description)
			buildin_functions_identifiers.insert(std::make_pair("packUnorm2x16", functions_descriptions({"uint packUnorm2x16(vec2 v)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("packSnorm2x16", functions_descriptions({"uint packSnorm2x16(vec2 v)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("packUnorm4x8", functions_descriptions({"uint packUnorm4x8(vec4 v)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("packSnorm4x8", functions_descriptions({"uint packSnorm4x8(vec4 v)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("unpackUnorm2x16", functions_descriptions({"vec2 unpackUnorm2x16(uint p)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("unpackSnorm2x16", functions_descriptions({"vec2 unpackSnorm2x16(uint p)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("unpackUnorm4x8", functions_descriptions({"vec4 unpackUnorm4x8(uint p)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("unpackSnorm4x8", functions_descriptions({"vec4 unpackSnorm4x8(uint p)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("packDouble2x32", functions_descriptions({"double packDouble2x32(uvec2 v)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("unpackDouble2x32", functions_descriptions({"uvec2 unpackDouble2x32(double v)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("packHalf2x16", functions_descriptions({"uint packHalf2x16(vec2 v)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("unpackHalf2x16", functions_descriptions({"vec2 unpackHalf2x16(uint v)"}, {"Built-in Function"})));
			// Geometric Functions
			buildin_functions_identifiers.insert(std::make_pair("length", functions_descriptions({"float length(genType x)", "double length(genDType x)"}, {"Returns the length of vector x, i.e.,\n\t\t√x[0]2+x[1]2+..."})));
			buildin_functions_identifiers.insert(std::make_pair("distance", functions_descriptions({"float distance(genType p0, genType p1)", "double distance(genDType p0, genDType p1)"}, {"Returns the distance between p0 and p1, i.e.,\n\t\tlength (p0 - p1)"})));
			buildin_functions_identifiers.insert(std::make_pair("dot", functions_descriptions({"float dot(genType x, genType y)", "double dot(genDType x, genDType y)"}, {"Returns the dot product of x and y, i.e.,\n\t\tx[0]⋅y[0]+x[1]⋅y[1]+..."})));
			buildin_functions_identifiers.insert(std::make_pair("cross", functions_descriptions({"vec3 cross(vec3 x, vec3 y)", "dvec3 cross(dvec3 x, dvec3 y)"}, {"Returns the cross product of x and y, i.e.,\n\t\t[x[1]⋅y[2]-y[1]⋅x[2]]\n\t\t[x[2]⋅y[0]-y[2]⋅x[0]]\n\t\t[x[0]⋅y[1]-y[0]⋅x[1]]"})));
			buildin_functions_identifiers.insert(std::make_pair("normalize", functions_descriptions({"genType normalize(genType x)", "genDType normalize(genDType x)"}, {"Returns a vector in the same direction as x but with a length of 1."})));
			buildin_functions_identifiers.insert(std::make_pair("ftransform", functions_descriptions({"vec4 ftransform()"}, {"Available only when using the compatibility profile. For core OpenGL, use invariant.\n\tFor vertex shaders only. This function will ensure that the incoming vertex value will be transformed in a way that produces exactly the same result as would be\n\tproduced by OpenGL’s fixed functionality transform. It is intended to be used to compute gl_Position, e.g.,\n\t\tgl_Position = ftransform()\n\tThis function should be used, for example, when an application is rendering the same geometry in separate passes,\n\tand one pass uses the fixed functionality path to render and another pass uses programmable shaders."})));
			buildin_functions_identifiers.insert(std::make_pair("faceforward", functions_descriptions({"genType faceforward(genType N, genType I, genType Nref)", "genDType faceforward(genDType N, genDType I, genDType Nref)"}, {"If dot(Nref, I) < 0 return N, otherwise return –N."})));
			buildin_functions_identifiers.insert(std::make_pair("reflect", functions_descriptions({"genType reflect(genType I, genType N)", "genDType reflect(genDType I, genDType N)"}, {"For the incident vector I and surface orientation N, returns the reflection direction:\n\t\tI - 2 * dot(N, I) * N\n\tN must already be normalized in order to achieve the desired result."})));
			buildin_functions_identifiers.insert(std::make_pair("refract", functions_descriptions({"genType refract(genType I, genType N, float eta)", "genDType refract(genDType I, genDType N, float eta)"}, {"For the incident vector I and surface normal N, and the ratio of indices of refraction eta, return the refraction vector. The result is computed by \n\t\tk = 1.0 - eta * eta * (1.0 - dot(N, I) * dot(N, I))\n\t\tif (k < 0.0)\n\t\t\treturn genType(0.0) // or genDType(0.0)\n\t\telse\n\t\t\treturn eta * I - (eta * dot(N, I) + sqrt(k)) * N\n\tThe input parameters for the incident vector I and the surface normal N must already be normalized to get the desired results."})));
			// Matrix Functions
			buildin_functions_identifiers.insert(std::make_pair("matrixCompMult", functions_descriptions({"mat matrixCompMult(mat x, mat y)"}, {"Multiply matrix x by matrix y component-wise, i.e., result[i][j] is the scalar product of x[i][j] and y[i][j].\n\tNote: to get linear algebraic matrix multiplication, use the multiply operator (*)."})));
			buildin_functions_identifiers.insert(std::make_pair("outerProduct", functions_descriptions({"mat2 outerProduct(vec2 c, vec2 r)", "mat3 outerProduct(vec3 c, vec3 r)", "mat4 outerProduct(vec4 c, vec4 r)", "mat2x3 outerProduct(vec3 c, vec2 r)", "mat3x2 outerProduct(vec2 c, vec3 r)", "mat2x4 outerProduct(vec4 c, vec2 r)", "mat4x2 outerProduct(vec2 c, vec4 r)", "mat3x4 outerProduct(vec4 c, vec3 r)", "mat4x3 outerProduct(vec3 c, vec4 r)"}, {"Treats the first parameter c as a column vector (matrix with one column) and the second parameter r as a row vector (matrix with one row)\n\tand does a linear algebraic matrix multiply c * r, yielding a matrix whose number of rows is the number of components in c and whose number of columns is the number of components in r."})));
			buildin_functions_identifiers.insert(std::make_pair("transpose", functions_descriptions({"mat2 transpose(mat2 m)", "mat3 transpose(mat3 m)", "mat4 transpose(mat4 m)", "mat2x3 transpose(mat3x2 m)", "mat3x2 transpose(mat2x3 m)", "mat2x4 transpose(mat4x2 m)", "mat4x2 transpose(mat2x4 m)", "mat3x4 transpose(mat4x3 m)", "mat4x3 transpose(mat3x4 m)"}, {"Returns a matrix that is the transpose of m. The input matrix m is not modified."})));
			buildin_functions_identifiers.insert(std::make_pair("determinant", functions_descriptions({"float determinant(mat2 m)", "float determinant(mat3 m)", "float determinant(mat4 m)"}, {"Returns the determinant of m."})));
			buildin_functions_identifiers.insert(std::make_pair("inverse", functions_descriptions({"mat2 inverse(mat2 m)", "mat3 inverse(mat3 m)", "mat4 inverse(mat4 m)"}, {"Returns a matrix that is the inverse of m. The input matrix m is not modified.\n\tThe values in the returned matrix are undefined if m is singular or poorly- conditioned (nearly singular)."})));
			// Vector Relational Functions
			buildin_functions_identifiers.insert(std::make_pair("lessThan", functions_descriptions({"bvec lessThan(vec x, vec y)", "bvec lessThan(ivec x, ivec y)", "bvec lessThan(uvec x, uvec y)"}, {"Returns the component-wise compare of x < y."})));
			buildin_functions_identifiers.insert(std::make_pair("lessThanEqual", functions_descriptions({"bvec lessThanEqual(vec x, vec y)", "bvec lessThanEqual(ivec x, ivec y)", "bvec lessThanEqual(uvec x, uvec y)"}, {"Returns the component-wise compare of x <= y."})));
			buildin_functions_identifiers.insert(std::make_pair("greaterThan", functions_descriptions({"bvec greaterThan(vec x, vec y)", "bvec greaterThan(ivec x, ivec y)", "bvec greaterThan(uvec x, uvec y)"}, {"Returns the component-wise compare of x > y."})));
			buildin_functions_identifiers.insert(std::make_pair("greaterThanEqual", functions_descriptions({"bvec greaterThanEqual(vec x, vec y)", "bvec greaterThanEqual(ivec x, ivec y)", "bvec greaterThanEqual(uvec x, uvec y)"}, {"Returns the component-wise compare of x >= y."})));
			buildin_functions_identifiers.insert(std::make_pair("equal", functions_descriptions({"bvec equal(vec x, vec y)", "bvec equal(ivec x, ivec y)", "bvec equal(uvec x, uvec y)", "bvec equal(bvec x, bvec y)"}, {"Returns the component-wise compare of x == y."})));
			buildin_functions_identifiers.insert(std::make_pair("notEqual", functions_descriptions({"bvec notEqual(vec x, vec y)", "bvec notEqual(ivec x, ivec y)", "bvec notEqual(uvec x, uvec y)", "bvec notEqual(bvec x, bvec y)"}, {"Returns the component-wise compare of x != y."})));
			buildin_functions_identifiers.insert(std::make_pair("any", functions_descriptions({"bool any(bvec x)"}, {"Returns true if any component of x is true."})));
			buildin_functions_identifiers.insert(std::make_pair("all", functions_descriptions({"bool all(bvec x)"}, {"Returns true only if all components of x are true."})));
			buildin_functions_identifiers.insert(std::make_pair("not", functions_descriptions({"bvec not(bvec x)"}, {"Returns the component-wise logical complement of x."})));
			// Integer Functions (no Description)
			buildin_functions_identifiers.insert(std::make_pair("uaddCarry", functions_descriptions({"genUType uaddCarry(genUType x, genUType y, out genUType carry)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("usubBorrow", functions_descriptions({"genUType usubBorrow(genUType x, genUType y, out genUType borrow)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("umulExtended", functions_descriptions({"void umulExtended(genUType x, genUType y, out genUType msb, out genUType lsb)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("imulExtended", functions_descriptions({"void imulExtended(genIType x, genIType y, out genIType msb, out genIType lsb)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("bitfieldExtract", functions_descriptions({"genIType bitfieldExtract(genIType value, int offset, int bits)", "genUType bitfieldExtract(genUType value, int offset, int bits)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("bitfieldInsert", functions_descriptions({"genIType bitfieldInsert(genIType base, genIType insert, int offset, int bits)", "genUType bitfieldInsert(genUType base, genUType insert, int offset, int bits)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("bitfieldReverse", functions_descriptions({"genIType bitfieldReverse(genIType value)", "genUType bitfieldReverse(genUType value)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("bitCount", functions_descriptions({"genIType bitCount(genIType value)", "genIType bitCount(genUType value)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("findLSB", functions_descriptions({"genIType findLSB(genIType value)", "genIType findLSB(genUType value)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("findMSB", functions_descriptions({"genIType findMSB(genIType value)", "genIType findMSB(genUType value)"}, {"Built-in Function"})));
			// Texture Query Functions (no Description)
			buildin_functions_identifiers.insert(std::make_pair("textureSize", functions_descriptions({"int textureSize(gsampler1D sampler, int lod)", "ivec2 textureSize(gsampler2D sampler, int lod)", "ivec3 textureSize(gsampler3D sampler, int lod)", "ivec2 textureSize(gsamplerCube sampler, int lod)",
																										"int textureSize(sampler1DShadow sampler, int lod)", "ivec2 textureSize(sampler2DShadow sampler, int lod)", "ivec2 textureSize(samplerCubeShadow sampler, int lod)", "ivec3 textureSize(gsamplerCubeArray sampler, int lod)",
																										"ivec3 textureSize(samplerCubeArrayShadow sampler, int lod)", "ivec2 textureSize(gsampler2DRect sampler)", "ivec2 textureSize(sampler2DRectShadow sampler)", "ivec2 textureSize(gsampler1DArray sampler, int lod)",
																										"ivec3 textureSize(gsampler2DArray sampler, int lod)", "ivec2 textureSize(sampler1DArrayShadow sampler, int lod)", "ivec3 textureSize(sampler2DArrayShadow sampler, int lod)", "int textureSize(gsamplerBuffer sampler)", "ivec2 textureSize(gsampler2DMS sampler)", "ivec3 textureSize(gsampler2DMSArray sampler)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("textureQueryLod", functions_descriptions({"vec2 textureQueryLod(gsampler1D sampler, float P)", "vec2 textureQueryLod(gsampler2D sampler, vec2 P)", "vec2 textureQueryLod(gsampler3D sampler, vec3 P)", "vec2 textureQueryLod(gsamplerCube sampler, vec3 P)",
																											"vec2 textureQueryLod(gsampler1DArray sampler, float P)", "vec2 textureQueryLod(gsampler2DArray sampler, vec2 P)", "vec2 textureQueryLod(gsamplerCubeArray sampler, vec3 P)", "vec2 textureQueryLod(sampler1DShadow sampler, float P)",
																											"vec2 textureQueryLod(sampler2DShadow sampler, vec2 P)", "vec2 textureQueryLod(samplerCubeShadow sampler, vec3 P)", "vec2 textureQueryLod(sampler1DArrayShadow sampler, float P)", "vec2 textureQueryLod(sampler2DArrayShadow sampler, vec2 P)", "vec2 textureQueryLod(samplerCubeArrayShadow sampler, vec3 P)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("textureQueryLevels", functions_descriptions({"int textureQueryLevels(gsampler1D sampler)", "int textureQueryLevels(gsampler2D sampler)", "int textureQueryLevels(gsampler3D sampler)", "int textureQueryLevels(gsamplerCube sampler)",
																												"int textureQueryLevels(gsampler1DArray sampler)", "int textureQueryLevels(gsampler2DArray sampler)", "int textureQueryLevels(gsamplerCubeArray sampler)", "int textureQueryLevels(sampler1DShadow sampler)",
																												"int textureQueryLevels(sampler2DShadow sampler)", "int textureQueryLevels(samplerCubeShadow sampler)", "int textureQueryLevels(sampler1DArrayShadow sampler)",  "int textureQueryLevels(sampler2DArrayShadow sampler) ", "int textureQueryLevels(samplerCubeArrayShadow sampler)"}, {"Built-in Function"})));
			// Texel Lookup Functions (no Description)
			buildin_functions_identifiers.insert(std::make_pair("texture", functions_descriptions({"gvec4 texture(gsampler1D sampler, float P[, float bias])", "gvec4 texture(gsampler2D sampler, vec2 P[, float bias])", "gvec4 texture(gsampler3D sampler, vec3 P[, float bias])", "gvec4 texture(gsamplerCube sampler, vec3 P[, float bias])", "float texture(sampler1DShadow sampler, vec3 P[, float bias])",
																									"float texture(sampler2DShadow sampler, vec3 P[, float bias])", "float texture(samplerCubeShadow sampler, vec4 P[, float bias])", "gvec4 texture(gsampler1DArray sampler, vec2 P[, float bias])", "gvec4 texture(gsampler2DArray sampler, vec3 P[, float bias])", "gvec4 texture(gsamplerCubeArray sampler, vec4 P[, float bias])",
																									"float texture(sampler1DArrayShadow sampler, vec3 P[, float bias])", "float texture(sampler2DArrayShadow sampler, vec4 P)", "gvec4 texture(gsampler2DRect sampler, vec2 P)", "float texture(sampler2DRectShadow sampler, vec3 P)", "float texture(gsamplerCubeArrayShadow sampler, vec4 P, float compare)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("textureProj", functions_descriptions({"gvec4 textureProj(gsampler1D sampler, vec2 P[, float bias])", "gvec4 textureProj(gsampler1D sampler, vec4 P[, float bias])", "gvec4 textureProj(gsampler2D sampler, vec3 P[, float bias])", "gvec4 textureProj(gsampler2D sampler, vec4 P[, float bias])", "gvec4 textureProj(gsampler3D sampler, vec4 P[, float bias])",
																										"float textureProj(sampler1DShadow sampler, vec4 P[, float bias])", "float textureProj(sampler2DShadow sampler, vec4 P[, float bias])", "gvec4 textureProj(gsampler2DRect sampler, vec3 P)", "gvec4 textureProj(gsampler2DRect sampler, vec4 P)", "float textureProj(sampler2DRectShadow sampler, vec4 P)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("textureLod", functions_descriptions({"gvec4 textureLod(gsampler1D sampler, float P, float lod)", "gvec4 textureLod(gsampler2D sampler, vec2 P, float lod)", "gvec4 textureLod(gsampler3D sampler, vec3 P, float lod)", "gvec4 textureLod(gsamplerCube sampler, vec3 P, float lod)", "float textureLod(sampler1DShadow sampler, vec3 P, float lod)",
																										"float textureLod(sampler2DShadow sampler, vec3 P, float lod)", "gvec4 textureLod(gsampler1DArray sampler, vec2 P, float lod)", "gvec4 textureLod(gsampler2DArray sampler, vec3 P, float lod)", "float textureLod(sampler1DArrayShadow sampler, vec3 P, float lod)", "gvec4 textureLod(gsamplerCubeArray sampler, vec4 P, float lod)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("textureOffset", functions_descriptions({"gvec4 textureOffset(gsampler1D sampler, float P, int offset[, float bias])", "gvec4 textureOffset(gsampler2D sampler, vec2 P, ivec2 offset[, float bias])", "gvec4 textureOffset(gsampler3D sampler, vec3 P, ivec3 offset[, float bias])", "gvec4 textureOffset(gsampler2DRect sampler, vec2 P, ivec2 offset)", "float textureOffset(sampler2DRectShadow sampler, vec3 P, ivec2 offset)",
																										"float textureOffset(sampler1DShadow sampler, vec3 P, int offset[, float bias])", "float textureOffset(sampler2DShadow sampler, vec3 P, ivec2 offset[, float bias])", "gvec4 textureOffset(gsampler1DArray sampler, vec2 P, int offset[, float bias])", "gvec4 textureOffset(gsampler2DArray sampler, vec3 P, ivec2 offset[, float bias])", "float textureOffset(sampler1DArrayShadow sampler, vec3 P, int offset[, float bias])", "float textureOffset(sampler2DArrayShadow sampler, vec4 P, ivec2 offset)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("texelFetch", functions_descriptions({"gvec4 texelFetch(gsampler1D sampler, int P, int lod)", "gvec4 texelFetch(gsampler2D sampler, ivec2 P, int lod)", "gvec4 texelFetch(gsampler3D sampler, ivec3 P, int lod)", "gvec4 texelFetch(gsampler2DRect sampler, ivec2 P)", "gvec4 texelFetch(gsampler1DArray sampler, ivec2 P, int lod)",
																										"gvec4 texelFetch(gsampler2DArray sampler, ivec3 P, int lod)", "gvec4 texelFetch(gsamplerBuffer sampler, int P)", "gvec4 texelFetch(gsampler2DMS sampler, ivec2 P, int sample)", "gvec4 texelFetch(gsampler2DMSArray sampler, ivec3 P, int sample)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("texelFetchOffset", functions_descriptions({"gvec4 texelFetchOffset(gsampler1D sampler, int P, int lod, int offset)", "gvec4 texelFetchOffset(gsampler2D sampler, ivec2 P, int lod, ivec2 offset)", "gvec4 texelFetchOffset(gsampler3D sampler, ivec3 P, int lod, ivec3 offset)",
																											"gvec4 texelFetchOffset(gsampler2DRect sampler, ivec2 P, ivec2 offset)", "gvec4 texelFetchOffset(gsampler1DArray sampler, ivec2 P, int lod, int offset)", "gvec4 texelFetchOffset(gsampler2DArray sampler, ivec3 P, int lod, ivec2 offset)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("textureProjOffset", functions_descriptions({"gvec4 textureProjOffset(gsampler1D sampler, vec2 P, int offset[, float bias])", "gvec4 textureProjOffset(gsampler1D sampler, vec4 P, int offset[, float bias])", "gvec4 textureProjOffset(gsampler2D sampler, vec3 P, ivec2 offset[, float bias])", "gvec4 textureProjOffset(gsampler2D sampler, vec4 P, ivec2 offset[, float bias])", "gvec4 textureProjOffset(gsampler3D sampler, vec4 P, ivec3 offset[, float bias])",
																											"gvec4 textureProjOffset(gsampler2DRect sampler, vec3 P, ivec2 offset)", "gvec4 textureProjOffset(gsampler2DRect sampler, vec4 P, ivec2 offset)", "float textureProjOffset(sampler2DRectShadow sampler, vec4 P, ivec2 offset)", "float textureProjOffset(sampler1DShadow sampler, vec4 P, int offset[, float bias])", "float textureProjOffset(sampler2DShadow sampler, vec4 P, ivec2 offset[, float bias])"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("textureLodOffset", functions_descriptions({"gvec4 textureLodOffset(gsampler1D sampler, float P, float lod, int offset)", "gvec4 textureLodOffset(gsampler2D sampler, vec2 P, float lod, ivec2 offset)", "gvec4 textureLodOffset(gsampler3D sampler, vec3 P, float lod, ivec3 offset)", "float textureLodOffset(sampler1DShadow sampler, vec3 P, float lod, int offset)",
																											"float textureLodOffset(sampler2DShadow sampler, vec3 P, float lod, ivec2 offset)", "gvec4 textureLodOffset(gsampler1DArray sampler, vec2 P, float lod, int offset)", "gvec4 textureLodOffset(gsampler2DArray sampler, vec3 P, float lod, ivec2 offset)", "float textureLodOffset(sampler1DArrayShadow sampler, vec3 P, float lod, int offset)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("textureProjLod", functions_descriptions({"gvec4 textureProjLod(gsampler1D sampler, vec2 P, float lod)", "gvec4 textureProjLod(gsampler1D sampler, vec4 P, float lod)", "gvec4 textureProjLod(gsampler2D sampler, vec3 P, float lod)", "gvec4 textureProjLod(gsampler2D sampler, vec4 P, float lod)",
																											"gvec4 textureProjLod(gsampler3D sampler, vec4 P, float lod)", "float textureProjLod(sampler1DShadow sampler, vec4 P, float lod)", "float textureProjLod(sampler2DShadow sampler, vec4 P, float lod)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("textureProjLodOffset", functions_descriptions({"gvec4 textureProjLodOffset(gsampler1D sampler, vec2 P, float lod, int offset)", "gvec4 textureProjLodOffset(gsampler1D sampler, vec4 P, float lod, int offset)", "gvec4 textureProjLodOffset(gsampler2D sampler, vec3 P, float lod, ivec2 offset)", "gvec4 textureProjLodOffset(gsampler2D sampler, vec4 P, float lod, ivec2 offset)",
																												"gvec4 textureProjLodOffset(gsampler3D sampler, vec4 P, float lod, ivec3 offset)", "float textureProjLodOffset(sampler1DShadow sampler, vec4 P, float lod, int offset)", "float textureProjLodOffset(sampler2DShadow sampler, vec4 P, float lod, ivec2 offset)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("textureGrad", functions_descriptions({"gvec4 textureGrad(gsampler1D sampler, float P, float dPdx, float dPdy)", "gvec4 textureGrad(gsampler2D sampler, vec2 P, vec2 dPdx, vec2 dPdy)", "gvec4 textureGrad(gsampler3D sampler, vec3 P, vec3 dPdx, vec3 dPdy)", "gvec4 textureGrad(gsamplerCube sampler, vec3 P, vec3 dPdx, vec3 dPdy)", "gvec4 textureGrad(gsampler2DRect sampler, vec2 P, vec2 dPdx, vec2 dPdy)",
																										"float textureGrad(sampler2DRectShadow sampler, vec3 P, vec2 dPdx, vec2 dPdy)", "float textureGrad(sampler1DShadow sampler, vec3 P, float dPdx, float dPdy)", "float textureGrad(sampler2DShadow sampler, vec3 P, vec2 dPdx, vec2 dPdy)", "float textureGrad(samplerCubeShadow sampler, vec4 P, vec3 dPdx, vec3 dPdy)", "gvec4 textureGrad(gsampler1DArray sampler, vec2 P, float dPdx, float dPdy)",
																										"gvec4 textureGrad(gsampler2DArray sampler, vec3 P, vec2 dPdx, vec2 dPdy)", "float textureGrad(sampler1DArrayShadow sampler, vec3 P, float dPdx, float dPdy)", "float textureGrad(sampler2DArrayShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy)", "gvec4 textureGrad(gsamplerCubeArray sampler, vec4 P, vec3 dPdx, vec3 dPdy)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("textureGradOffset", functions_descriptions({"gvec4 textureGradOffset(gsampler1D sampler, float P, float dPdx, float dPdy, int offset)", "gvec4 textureGradOffset(gsampler2D sampler, vec2 P, vec2 Pdx, vec2 dPdy, ivec2 offset)", "gvec4 textureGradOffset(gsampler3D sampler, vec3 P, vec3 dPdx, vec3 dPdy, ivec3 offset)", "gvec4 textureGradOffset(gsampler2DRect sampler, vec2 P, vec2 dPdx, vec2 dPdy, ivec2 offset)",
																											"float textureGradOffset(sampler2DRectShadow sampler, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset)", "float textureGradOffset(sampler1DShadow sampler, vec3 P, float dPdx, float dPdy, int offset )", "float textureGradOffset(sampler2DShadow sampler, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset)", "gvec4 textureGradOffset(gsampler1DArray sampler, vec2 P, float dPdx, float dPdy, int offset)",
																											"gvec4 textureGradOffset(gsampler2DArray sampler, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset)", "float textureGradOffset(sampler1DArrayShadow sampler, vec3 P, float dPdx, float dPdy, int offset)", "float textureGradOffset(sampler2DArrayShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("textureProjGrad", functions_descriptions({"gvec4 textureProjGrad(gsampler1D sampler, vec2 P, float dPdx, float dPdy)", "gvec4 textureProjGrad(gsampler1D sampler, vec4 P, float dPdx, float dPdy)", "gvec4 textureProjGrad(gsampler2D sampler, vec3 P, vec2 dPdx, vec2 dPdy)", "gvec4 textureProjGrad(gsampler2D sampler, vec4 P, vec2 dPdx, vec2 dPdy)", "gvec4 textureProjGrad(gsampler3D sampler, vec4 P, vec3 dPdx, vec3 dPdy)",
																											"gvec4 textureProjGrad(gsampler2DRect sampler, vec3 P, vec2 dPdx, vec2 dPdy)", "gvec4 textureProjGrad(gsampler2DRect sampler, vec4 P, vec2 dPdx, vec2 dPdy)", "float textureProjGrad(sampler2DRectShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy)", "float textureProjGrad(sampler1DShadow sampler, vec4 P, float dPdx, float dPdy)", "float textureProjGrad(sampler2DShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("textureProjGradOffset", functions_descriptions({"gvec4 textureProjGradOffset(gsampler1D sampler, vec2 P, float dPdx, float dPdy, int offset)", "gvec4 textureProjGradOffset(gsampler1D sampler, vec4 P, float dPdx, float dPdy, int offset)", "gvec4 textureProjGradOffset(gsampler2D sampler, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset)", "gvec4 textureProjGradOffset(gsampler2D sampler, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset)", "gvec4 textureProjGradOffset(gsampler2DRect sampler, vec3 P, vec2 dPdx, vec2 dPdy, ivec2 offset)",
																												"gvec4 textureProjGradOffset(gsampler2DRect sampler, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset)", "float textureProjGradOffset(sampler2DRectShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset)", "gvec4 textureProjGradOffset(gsampler3D sampler, vec4 P, vec3 dPdx, vec3 dPdy, ivec3 offset)", "float textureProjGradOffset(sampler1DShadow sampler, vec4 P, float dPdx, float dPdy, int offset)", "float textureProjGradOffset(sampler2DShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy, ivec2 offset)"}, {"Built-in Function"})));
			// Texture Gather Functions (no Description)
			buildin_functions_identifiers.insert(std::make_pair("textureGather", functions_descriptions({"gvec4 textureGather(gsampler2D sampler, vec2 P [, int comp])", "gvec4 textureGather(gsampler2DArray sampler, vec3 P [, int comp])", "gvec4 textureGather(gsamplerCube sampler, vec3 P [, int comp])", "gvec4 textureGather(gsamplerCubeArray sampler, vec4 P[, int comp])", "gvec4 textureGather(gsampler2DRect sampler, vec2 P[, int comp])",
																										"vec4 textureGather(sampler2DShadow sampler, vec2 P, float refZ)", "vec4 textureGather(sampler2DArrayShadow sampler, vec3 P, float refZ)", "vec4 textureGather(samplerCubeShadow sampler, vec3 P, float refZ)", "vec4 textureGather(samplerCubeArrayShadow sampler, vec4 P, float refZ)", "vec4 textureGather(sampler2DRectShadow sampler, vec2 P, float refZ)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("textureGatherOffset", functions_descriptions({"gvec4 textureGatherOffset(gsampler2D sampler, vec2 P, ivec2 offset[, int comp])", "gvec4 textureGatherOffset(gsampler2DArray sampler, vec3 P, ivec2 offset[, int comp])", "gvec4 textureGatherOffset(gsampler2DRect sampler, vec2 P, ivec2 offset[, int comp])",
																												"vec4 textureGatherOffset(sampler2DShadow sampler, vec2 P, float refZ, ivec2 offset)", "vec4 textureGatherOffset(sampler2DArrayShadow sampler, vec3 P, float refZ, ivec2 offset)", "vec4 textureGatherOffset(sampler2DRectShadow sampler, vec2 P, float refZ, ivec2 offset)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("textureGatherOffsets", functions_descriptions({"gvec4 textureGatherOffsets(gsampler2D sampler, vec2 P, ivec2 offsets[4][, int comp])", "gvec4 textureGatherOffsets(gsampler2DArray sampler, vec3 P, ivec2 offsets[4][, int comp])", "gvec4 textureGatherOffsets(gsampler2DRect sampler, vec2 P, ivec2 offsets[4][, int comp])",
																												"vec4 textureGatherOffsets(sampler2DShadow sampler, vec2 P, float refZ, ivec2 offsets[4])", "vec4 textureGatherOffsets(sampler2DArrayShadow sampler, vec3 P, float refZ, ivec2 offsets[4])", "vec4 textureGatherOffsets(sampler2DRectShadow sampler, vec2 P, float refZ, ivec2 offsets[4])"}, {"Built-in Function"})));
			// Compatibility Profile Texture Functions (no Description)
			buildin_functions_identifiers.insert(std::make_pair("texture1D", functions_descriptions({"vec4 texture1D(sampler1D sampler, float coord [, float bias] )"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("texture1DProj", functions_descriptions({"vec4 texture1DProj(sampler1D sampler, vec2 coord[, float bias])", "vec4 texture1DProj(sampler1D sampler, vec4 coord[, float bias])"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("texture1DLod", functions_descriptions({"vec4 texture1DLod(sampler1D sampler, float coord, float lod)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("texture1DProjLod", functions_descriptions({"vec4 texture1DProjLod(sampler1D sampler, vec2 coord, float lod)", "vec4 texture1DProjLod(sampler1D sampler, vec4 coord, float lod)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("texture2D", functions_descriptions({"vec4 texture2D(sampler2D sampler, vec2 coord[, float bias])"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("texture2DProj", functions_descriptions({"vec4 texture2DProj(sampler2D sampler, vec3 coord[, float bias])", "vec4 texture2DProj(sampler2D sampler, vec4 coord[, float bias])"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("texture2DLod", functions_descriptions({"vec4 texture2DLod(sampler2D sampler, vec2 coord, float lod)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("texture2DProjLod", functions_descriptions({"vec4 texture2DProjLod(sampler2D sampler, vec3 coord, float lod)", "vec4 texture2DProjLod(sampler2D sampler, vec4 coord, float lod)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("texture3D", functions_descriptions({"vec4 texture3D(sampler3D sampler, vec3 coord[, float bias])"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("texture3DProj", functions_descriptions({"vec4 texture3DProj(sampler3D sampler, vec4 coord[, float bias])"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("texture3DLod", functions_descriptions({"vec4 texture3DLod(sampler3D sampler, vec3 coord, float lod)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("texture3DProjLod", functions_descriptions({"vec4 texture3DProjLod(sampler3D sampler, vec4 coord, float lod)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("textureCube", functions_descriptions({"vec4 textureCube(samplerCube sampler, vec3 coord[, float bias])"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("textureCubeLod", functions_descriptions({"vec4 textureCubeLod(samplerCube sampler, vec3 coord, float lod)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("shadow1D", functions_descriptions({"vec4 shadow1D(sampler1DShadow sampler, vec3 coord[, float bias])"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("shadow2D", functions_descriptions({"vec4 shadow2D(sampler2DShadow sampler, vec3 coord[, float bias])"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("shadow1DProj", functions_descriptions({"vec4 shadow1DProj(sampler1DShadow sampler, vec4 coord[, float bias])"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("shadow2DProj", functions_descriptions({"vec4 shadow2DProj(sampler2DShadow sampler, vec4 coord[, float bias])"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("shadow1DLod", functions_descriptions({"vec4 shadow1DLod(sampler1DShadow sampler, vec3 coord, float lod)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("shadow2DLod", functions_descriptions({"vec4 shadow2DLod(sampler2DShadow sampler, vec3 coord, float lod)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("shadow1DProjLod", functions_descriptions({"vec4 shadow1DProjLod(sampler1DShadow sampler, vec4 coord, float lod)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("shadow2DProjLod", functions_descriptions({"vec4 shadow2DProjLod(sampler2DShadow sampler, vec4 coord, float lod)"}, {"Built-in Function"})));
			// Atomic-Counter Functions
			buildin_functions_identifiers.insert(std::make_pair("atomicCounterIncrement", functions_descriptions({"uint atomicCounterIncrement(atomic_uint c)"}, {"Atomically\n\t\t1. increments the counter for c, and\n\t\t2. returns its value prior to the increment operation.\n\tThese two steps are done atomically with respect to the atomic counter functions in this table."})));
			buildin_functions_identifiers.insert(std::make_pair("atomicCounterDecrement", functions_descriptions({"uint atomicCounterDecrement(atomic_uint c)"}, {"Atomically\n\t\t1. decrements the counter for c, and\n\t\t2. returns the value resulting from the decrement operation.\n\tThese two steps are done atomically with respect to the atomic counter functions in this table."})));
			buildin_functions_identifiers.insert(std::make_pair("atomicCounter", functions_descriptions({"uint atomicCounter(atomic_uint c)"}, {"Returns the counter value for c."})));
			// Atomic Memory Functions
			buildin_functions_identifiers.insert(std::make_pair("atomicAdd", functions_descriptions({"uint atomicAdd(inout uint mem, uint data)", "int atomicAdd(inout int mem, int data)"}, {"Computes a new value by adding the value of data to the contents mem."})));
			buildin_functions_identifiers.insert(std::make_pair("atomicMin", functions_descriptions({"uint atomicMin(inout uint mem, uint data)", "int atomicMin(inout int mem, int data)"}, {"Computes a new value by taking the minimum of the value of data and the contents of mem."})));
			buildin_functions_identifiers.insert(std::make_pair("atomicMax", functions_descriptions({"uint atomicMax(inout uint mem, uint data)", "int atomicMax(inout int mem, int data)"}, {"Computes a new value by taking the maximum of the value of data and the contents of mem."})));
			buildin_functions_identifiers.insert(std::make_pair("atomicAnd", functions_descriptions({"uint atomicAnd(inout uint mem, uint data)", "int atomicAnd(inout int mem, int data)"}, {"Computes a new value by performing a bit-wise AND of the value of data and the contents of mem."})));
			buildin_functions_identifiers.insert(std::make_pair("atomicOr", functions_descriptions({"uint atomicOr(inout uint mem, uint data)", "int atomicOr(inout int mem, int data)"}, {"Computes a new value by performing a bit-wise OR of the value of data and the contents of mem."})));
			buildin_functions_identifiers.insert(std::make_pair("atomicXor", functions_descriptions({"uint atomicXor(inout uint mem, uint data)", "int atomicXor(inout int mem, int data)"}, {"Computes a new value by performing a bit-wise EXCLUSIVE OR of the value of data and the contents of mem."})));
			buildin_functions_identifiers.insert(std::make_pair("atomicExchange", functions_descriptions({"uint atomicExchange(inout uint mem, uint data)", "int atomicExchange(inout int mem, int data)"}, {"Computes a new value by simply copying the value of data."})));
			buildin_functions_identifiers.insert(std::make_pair("atomicCompSwap", functions_descriptions({"uint atomicCompSwap(inout uint mem, uint compare, uint data)", "int atomicCompSwap(inout int mem, int compare, int data)"}, {"Compares the value of compare and the contents of mem.\n\tIf the values are equal, the new value is given by data; otherwise, it is taken from the original contents of mem."})));
			// Image Functions (no Description)
			buildin_functions_identifiers.insert(std::make_pair("imageSize", functions_descriptions({"int imageSize(readonly writeonly gimage1D image)", "ivec2 imageSize(readonly writeonly gimage2D image)", "ivec3 imageSize(readonly writeonly gimage3D image)", "ivec2 imageSize(readonly writeonly gimageCube image)", "ivec3 imageSize(readonly writeonly gimageCubeArray image)",
																									"ivec2 imageSize(readonly writeonly gimageRect image)", "ivec2 imageSize(readonly writeonly gimage1DArray image)", "ivec3 imageSize(readonly writeonly gimage2DArray image)", "int imageSize(readonly writeonly gimageBuffer image)", "ivec2 imageSize(readonly writeonly gimage2DMS image)", "ivec3 imageSize(readonly writeonly gimage2DMSArray image)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("imageLoad", functions_descriptions({"gvec4 imageLoad(readonly IMAGE_PARAMS)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("imageStore", functions_descriptions({"void imageStore(writeonly IMAGE_PARAMS, gvec4 data)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("imageAtomicAdd", functions_descriptions({"uint imageAtomicAdd(IMAGE_PARAMS, uint data)", "int imageAtomicAdd(IMAGE_PARAMS, int data)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("imageAtomicMin", functions_descriptions({"uint imageAtomicMin(IMAGE_PARAMS, uint data)", "int imageAtomicMin(IMAGE_PARAMS, int data)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("imageAtomicMax", functions_descriptions({"uint imageAtomicMax(IMAGE_PARAMS, uint data)", "int imageAtomicMax(IMAGE_PARAMS, int data)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("imageAtomicAnd", functions_descriptions({"uint imageAtomicAnd(IMAGE_PARAMS, uint data)", "int imageAtomicAnd(IMAGE_PARAMS, int data)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("imageAtomicOr", functions_descriptions({"uint imageAtomicOr(IMAGE_PARAMS, uint data)", "int imageAtomicOr(IMAGE_PARAMS, int data)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("imageAtomicXor", functions_descriptions({"uint imageAtomicXor(IMAGE_PARAMS, uint data)", "int imageAtomicXor(IMAGE_PARAMS, int data)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("imageAtomicExchange", functions_descriptions({"uint imageAtomicExchange(IMAGE_PARAMS, uint data)", "int imageAtomicExchange(IMAGE_PARAMS, int data)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("imageAtomicCompSwap", functions_descriptions({"uint imageAtomicCompSwap(IMAGE_PARAMS, uint compare, uint data)", "int imageAtomicCompSwap(IMAGE_PARAMS, int compare, int data)"}, {"Built-in Function"})));
			// Fragment Processing Functions
			buildin_functions_identifiers.insert(std::make_pair("dFdx", functions_descriptions({"genType dFdx(genType p)"}, {"Returns the derivative in x using local differencing for the input argument p."})));
			buildin_functions_identifiers.insert(std::make_pair("dFdy", functions_descriptions({"genType dFdy(genType p)"}, {"Returns the derivative in y using local differencing for the input argument p.\n\tThese two functions are commonly used to estimate the filter width used to anti-alias procedural textures.\n\tWe are assuming that the expression is being evaluated in parallel on a SIMD array so that at any given point in time the value of the function is known at the grid points represented by the SIMD array.\n\tLocal differencing between SIMD array elements can therefore be used to derive dFdx, dFdy, etc."})));
			buildin_functions_identifiers.insert(std::make_pair("fwidth", functions_descriptions({"genType fwidth(genType p)"}, {"Returns the sum of the absolute derivative in x and y using local differencing for the input argument p, i.e., abs (dFdx (p)) + abs (dFdy (p));"})));
			// Interpolation Functions (no Description)
			buildin_functions_identifiers.insert(std::make_pair("interpolateAtCentroid", functions_descriptions({"float interpolateAtCentroid(float interpolant)", "vec2 interpolateAtCentroid(vec2 interpolant)", "vec3 interpolateAtCentroid(vec3 interpolant)", "vec4 interpolateAtCentroid(vec4 interpolant)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("interpolateAtSample", functions_descriptions({"float interpolateAtSample(float interpolant, int sample)", "vec2 interpolateAtSample(vec2 interpolant, int sample)", "vec3 interpolateAtSample(vec3 interpolant, int sample)", "vec4 interpolateAtSample(vec4 interpolant, int sample)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("interpolateAtOffset", functions_descriptions({"float interpolateAtOffset(float interpolant, vec2 offset)", "vec2 interpolateAtOffset(vec2 interpolant, vec2 offset)", "vec3 interpolateAtOffset(vec3 interpolant, vec2 offset)", "vec4 interpolateAtOffset(vec4 interpolant, vec2 offset)"}, {"Built-in Function"})));
			// Noise Functions
			buildin_functions_identifiers.insert(std::make_pair("noise1", functions_descriptions({"float noise1(genType x)"}, {"Returns a 1D noise value based on the input value x."})));
			buildin_functions_identifiers.insert(std::make_pair("noise2", functions_descriptions({"vec2 noise2(genType x)"}, {"Returns a 2D noise value based on the input value x."})));
			buildin_functions_identifiers.insert(std::make_pair("noise3", functions_descriptions({"vec3 noise3(genType x)"}, {"Returns a 3D noise value based on the input value x."})));
			buildin_functions_identifiers.insert(std::make_pair("noise4", functions_descriptions({"vec4 noise4(genType x)"}, {"Returns a 4D noise value based on the input value x."})));
			// Geometry Shader Functions (no Description)
			buildin_functions_identifiers.insert(std::make_pair("EmitStreamVertex", functions_descriptions({"void EmitStreamVertex(int stream)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("EndStreamPrimitive", functions_descriptions({"void EndStreamPrimitive(int stream)"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("EmitVertex", functions_descriptions({"void EmitVertex()"}, {"Built-in Function"})));
			buildin_functions_identifiers.insert(std::make_pair("EndPrimitive", functions_descriptions({"void EndPrimitive()"}, {"Built-in Function"})));
			// Shader Invocation Control Functions
			buildin_functions_identifiers.insert(std::make_pair("barrier", functions_descriptions({"void barrier()"}, {"For any given static instance of barrier(),\n\tall tessellation control shader invocations for a single input patch must enter it before any will be allowed to continue beyond it,\n\tor all invocations for a single work group must enter it before any will continue beyond it."})));
			// Shader Memory Control Functions
			buildin_functions_identifiers.insert(std::make_pair("memoryBarrier", functions_descriptions({"void memoryBarrier()"}, {"Control the ordering of memory transactions issued by a single shader invocation."})));
			buildin_functions_identifiers.insert(std::make_pair("memoryBarrierAtomicCounter", functions_descriptions({"void memoryBarrierAtomicCounter()"}, {"Control the ordering of accesses to atomic-counter variables issued by a single shader invocation."})));
			buildin_functions_identifiers.insert(std::make_pair("memoryBarrierBuffer", functions_descriptions({"void memoryBarrierBuffer()"}, {"Control the ordering of memory transactions to buffer variables issued within a single shader invocation."})));
			buildin_functions_identifiers.insert(std::make_pair("memoryBarrierShared", functions_descriptions({"void memoryBarrierShared()"}, {"Control the ordering of memory transactions to shared variables issued within a single shader invocation. Only available in compute shaders."})));
			buildin_functions_identifiers.insert(std::make_pair("memoryBarrierImage", functions_descriptions({"void memoryBarrierImage()"}, {"Control the ordering of memory transactions to images issued within a single shader invocation."})));
			buildin_functions_identifiers.insert(std::make_pair("groupMemoryBarrier", functions_descriptions({"void groupMemoryBarrier()"}, {"Control the ordering of all memory transactions issued within a single shader invocation, as viewed by other invocations in the same work group.\n\tOnly available in compute shaders."})));
			for (const auto& k : buildin_functions_identifiers)
			{
				Identifier id;
				id.mDeclaration = k.second;
				langDef.mIdentifiers.insert(std::make_pair(k.first, id));
			}
		}

		static const char* const buildin_variables[] = {
			"gl_NumWorkGroups", "gl_WorkGroupSize", "gl_WorkGroupID", "gl_LocalInvocationID", "gl_GlobalInvocationID", "gl_LocalInvocationIndex", "gl_VertexID", "gl_InstanceID", "gl_PerVertex",
			"gl_PrimitiveIDIn", "gl_InvocationID", "gl_PrimitiveID", "gl_Layer", "gl_ViewportIndex", "gl_PatchVerticesIn", "gl_TessLevelOuter", "gl_TessLevelInner", "gl_TessCoord", 
			"gl_FragCoord", "gl_FrontFacing", "gl_ClipDistance", "gl_PointCoord", "gl_SampleID", "gl_SamplePosition", "gl_SampleMaskIn", "gl_FragDepth", "gl_SampleMask",
			"gl_Color", "gl_SecondaryColor", "gl_Normal", "gl_Vertex", "gl_MultiTexCoord0", "gl_MultiTexCoord1", "gl_MultiTexCoord2", "gl_MultiTexCoord3", "gl_MultiTexCoord4",
			"gl_MultiTexCoord5", "gl_MultiTexCoord6", "gl_MultiTexCoord7", "gl_FogCoord"
		};

		for (auto& k : buildin_variables)
		{
			Identifier id;
			id.mDeclaration = "Built-in Variables";
			langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
		}

		static const char* const buildin_constants[] = {
			"gl_MaxComputeWorkGroupCount", "gl_MaxComputeWorkGroupSize", "gl_MaxComputeUniformComponents", "gl_MaxComputeTextureImageUnits", "gl_MaxComputeImageUniforms", "gl_MaxComputeAtomicCounters", "gl_MaxComputeAtomicCounterBuffers",
			"gl_MaxVertexAttribs", "gl_MaxVertexUniformComponents", "gl_MaxVaryingComponents", "gl_MaxVertexOutputComponents", "gl_MaxGeometryInputComponents", "gl_MaxGeometryOutputComponents", "gl_MaxFragmentInputComponents",
			"gl_MaxVertexTextureImageUnits", "gl_MaxCombinedTextureImageUnits", "gl_MaxTextureImageUnits", "gl_MaxImageUnits", "gl_MaxCombinedImageUnitsAndFragmentOutputs", "gl_MaxCombinedShaderOutputResources",
			"gl_MaxImageSamples", "gl_MaxVertexImageUniforms", "gl_MaxTessControlImageUniforms", "gl_MaxTessEvaluationImageUniforms", "gl_MaxGeometryImageUniforms", "gl_MaxFragmentImageUniforms", "gl_MaxCombinedImageUniforms",
			"gl_MaxFragmentUniformComponents", "gl_MaxDrawBuffers", "gl_MaxClipDistances", "gl_MaxGeometryTextureImageUnits", "gl_MaxGeometryOutputVertices", "gl_MaxGeometryTotalOutputComponents", "gl_MaxGeometryUniformComponents", "gl_MaxGeometryVaryingComponents",
			"gl_MaxTessControlInputComponents", "gl_MaxTessControlOutputComponents", "gl_MaxTessControlTextureImageUnits", "gl_MaxTessControlUniformComponents", "gl_MaxTessControlTotalOutputComponents", 
			"gl_MaxTessEvaluationInputComponents", "gl_MaxTessEvaluationOutputComponents", "gl_MaxTessEvaluationTextureImageUnits", "gl_MaxTessEvaluationUniformComponents", "gl_MaxTessPatchComponents", "gl_MaxPatchVertices", "gl_MaxTessGenLevel",
			"gl_MaxViewports", "gl_MaxVertexUniformVectors", "gl_MaxFragmentUniformVectors", "gl_MaxVaryingVectors", "gl_MaxVertexAtomicCounters", "gl_MaxTessControlAtomicCounters", "gl_MaxTessEvaluationAtomicCounters", "gl_MaxGeometryAtomicCounters",
			"gl_MaxFragmentAtomicCounters", "gl_MaxCombinedAtomicCounters", "gl_MaxAtomicCounterBindings", "gl_MaxVertexAtomicCounterBuffers", "gl_MaxTessControlAtomicCounterBuffers", "gl_MaxTessEvaluationAtomicCounterBuffers",
			"gl_MaxGeometryAtomicCounterBuffers", "gl_MaxFragmentAtomicCounterBuffers", "gl_MaxCombinedAtomicCounterBuffers", "gl_MaxAtomicCounterBufferSize", "gl_MinProgramTexelOffset", "gl_MaxProgramTexelOffset",
			"gl_MaxTransformFeedbackBuffers", "gl_MaxTransformFeedbackInterleavedComponents", "gl_MaxTextureUnits", "gl_MaxTextureCoords", "gl_MaxClipPlanes", "gl_MaxVaryingFloats"
		};

		for (auto& k : buildin_constants)
		{
			Identifier id;
			id.mDeclaration = "Built-in Constants";
			langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Preprocessor));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";
		langDef.mSingleLineComment = "//";

		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = true;

		langDef.mName = "GLSL";

		inited = true;
	}
	return langDef;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::C()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short",
			"signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary",
			"_Noreturn", "_Static_assert", "_Thread_local"
		};
		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static const char* const identifiers[] = {
			"abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph",
			"ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper"
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.mTokenize = [](const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end, PaletteIndex & paletteIndex) -> bool
		{
			paletteIndex = PaletteIndex::Max;

			while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
				in_begin++;

			if (in_begin == in_end)
			{
				out_begin = in_end;
				out_end = in_end;
				paletteIndex = PaletteIndex::Default;
			}
			else if (TokenizeCStyleString(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::String;
			else if (TokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::CharLiteral;
			else if (TokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::Identifier;
			else if (TokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::Number;
			else if (TokenizeCStylePunctuation(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::Punctuation;

			return paletteIndex != PaletteIndex::Max;
		};

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";
		langDef.mSingleLineComment = "//";

		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = true;

		langDef.mName = "C";

		inited = true;
	}
	return langDef;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::SQL()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"ADD", "EXCEPT", "PERCENT", "ALL", "EXEC", "PLAN", "ALTER", "EXECUTE", "PRECISION", "AND", "EXISTS", "PRIMARY", "ANY", "EXIT", "PRINT", "AS", "FETCH", "PROC", "ASC", "FILE", "PROCEDURE",
			"AUTHORIZATION", "FILLFACTOR", "PUBLIC", "BACKUP", "FOR", "RAISERROR", "BEGIN", "FOREIGN", "READ", "BETWEEN", "FREETEXT", "READTEXT", "BREAK", "FREETEXTTABLE", "RECONFIGURE",
			"BROWSE", "FROM", "REFERENCES", "BULK", "FULL", "REPLICATION", "BY", "FUNCTION", "RESTORE", "CASCADE", "GOTO", "RESTRICT", "CASE", "GRANT", "RETURN", "CHECK", "GROUP", "REVOKE",
			"CHECKPOINT", "HAVING", "RIGHT", "CLOSE", "HOLDLOCK", "ROLLBACK", "CLUSTERED", "IDENTITY", "ROWCOUNT", "COALESCE", "IDENTITY_INSERT", "ROWGUIDCOL", "COLLATE", "IDENTITYCOL", "RULE",
			"COLUMN", "IF", "SAVE", "COMMIT", "IN", "SCHEMA", "COMPUTE", "INDEX", "SELECT", "CONSTRAINT", "INNER", "SESSION_USER", "CONTAINS", "INSERT", "SET", "CONTAINSTABLE", "INTERSECT", "SETUSER",
			"CONTINUE", "INTO", "SHUTDOWN", "CONVERT", "IS", "SOME", "CREATE", "JOIN", "STATISTICS", "CROSS", "KEY", "SYSTEM_USER", "CURRENT", "KILL", "TABLE", "CURRENT_DATE", "LEFT", "TEXTSIZE",
			"CURRENT_TIME", "LIKE", "THEN", "CURRENT_TIMESTAMP", "LINENO", "TO", "CURRENT_USER", "LOAD", "TOP", "CURSOR", "NATIONAL", "TRAN", "DATABASE", "NOCHECK", "TRANSACTION",
			"DBCC", "NONCLUSTERED", "TRIGGER", "DEALLOCATE", "NOT", "TRUNCATE", "DECLARE", "NULL", "TSEQUAL", "DEFAULT", "NULLIF", "UNION", "DELETE", "OF", "UNIQUE", "DENY", "OFF", "UPDATE",
			"DESC", "OFFSETS", "UPDATETEXT", "DISK", "ON", "USE", "DISTINCT", "OPEN", "USER", "DISTRIBUTED", "OPENDATASOURCE", "VALUES", "DOUBLE", "OPENQUERY", "VARYING","DROP", "OPENROWSET", "VIEW",
			"DUMMY", "OPENXML", "WAITFOR", "DUMP", "OPTION", "WHEN", "ELSE", "OR", "WHERE", "END", "ORDER", "WHILE", "ERRLVL", "OUTER", "WITH", "ESCAPE", "OVER", "WRITETEXT"
		};

		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static const char* const identifiers[] = {
			"ABS",  "ACOS",  "ADD_MONTHS",  "ASCII",  "ASCIISTR",  "ASIN",  "ATAN",  "ATAN2",  "AVG",  "BFILENAME",  "BIN_TO_NUM",  "BITAND",  "CARDINALITY",  "CASE",  "CAST",  "CEIL",
			"CHARTOROWID",  "CHR",  "COALESCE",  "COMPOSE",  "CONCAT",  "CONVERT",  "CORR",  "COS",  "COSH",  "COUNT",  "COVAR_POP",  "COVAR_SAMP",  "CUME_DIST",  "CURRENT_DATE",
			"CURRENT_TIMESTAMP",  "DBTIMEZONE",  "DECODE",  "DECOMPOSE",  "DENSE_RANK",  "DUMP",  "EMPTY_BLOB",  "EMPTY_CLOB",  "EXP",  "EXTRACT",  "FIRST_VALUE",  "FLOOR",  "FROM_TZ",  "GREATEST",
			"GROUP_ID",  "HEXTORAW",  "INITCAP",  "INSTR",  "INSTR2",  "INSTR4",  "INSTRB",  "INSTRC",  "LAG",  "LAST_DAY",  "LAST_VALUE",  "LEAD",  "LEAST",  "LENGTH",  "LENGTH2",  "LENGTH4",
			"LENGTHB",  "LENGTHC",  "LISTAGG",  "LN",  "LNNVL",  "LOCALTIMESTAMP",  "LOG",  "LOWER",  "LPAD",  "LTRIM",  "MAX",  "MEDIAN",  "MIN",  "MOD",  "MONTHS_BETWEEN",  "NANVL",  "NCHR",
			"NEW_TIME",  "NEXT_DAY",  "NTH_VALUE",  "NULLIF",  "NUMTODSINTERVAL",  "NUMTOYMINTERVAL",  "NVL",  "NVL2",  "POWER",  "RANK",  "RAWTOHEX",  "REGEXP_COUNT",  "REGEXP_INSTR",
			"REGEXP_REPLACE",  "REGEXP_SUBSTR",  "REMAINDER",  "REPLACE",  "ROUND",  "ROWNUM",  "RPAD",  "RTRIM",  "SESSIONTIMEZONE",  "SIGN",  "SIN",  "SINH",
			"SOUNDEX",  "SQRT",  "STDDEV",  "SUBSTR",  "SUM",  "SYS_CONTEXT",  "SYSDATE",  "SYSTIMESTAMP",  "TAN",  "TANH",  "TO_CHAR",  "TO_CLOB",  "TO_DATE",  "TO_DSINTERVAL",  "TO_LOB",
			"TO_MULTI_BYTE",  "TO_NCLOB",  "TO_NUMBER",  "TO_SINGLE_BYTE",  "TO_TIMESTAMP",  "TO_TIMESTAMP_TZ",  "TO_YMINTERVAL",  "TRANSLATE",  "TRIM",  "TRUNC", "TZ_OFFSET",  "UID",  "UPPER",
			"USER",  "USERENV",  "VAR_POP",  "VAR_SAMP",  "VARIANCE",  "VSIZE "
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\\'[^\\\']*\\\'", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";
		langDef.mSingleLineComment = "//";

		langDef.mCaseSensitive = false;
		langDef.mAutoIndentation = false;

		langDef.mName = "SQL";

		inited = true;
	}
	return langDef;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::AngelScript()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"and", "abstract", "auto", "bool", "break", "case", "cast", "class", "const", "continue", "default", "do", "double", "else", "enum", "false", "final", "float", "for",
			"from", "funcdef", "function", "get", "if", "import", "in", "inout", "int", "interface", "int8", "int16", "int32", "int64", "is", "mixin", "namespace", "not",
			"null", "or", "out", "override", "private", "protected", "return", "set", "shared", "super", "switch", "this ", "true", "typedef", "uint", "uint8", "uint16", "uint32",
			"uint64", "void", "while", "xor"
		};

		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static const char* const identifiers[] = {
			"cos", "sin", "tab", "acos", "asin", "atan", "atan2", "cosh", "sinh", "tanh", "log", "log10", "pow", "sqrt", "abs", "ceil", "floor", "fraction", "closeTo", "fpFromIEEE", "fpToIEEE",
			"complex", "opEquals", "opAddAssign", "opSubAssign", "opMulAssign", "opDivAssign", "opAdd", "opSub", "opMul", "opDiv"
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";
		langDef.mSingleLineComment = "//";

		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = true;

		langDef.mName = "AngelScript";

		inited = true;
	}
	return langDef;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::Lua()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"and", "break", "do", "else", "elseif", "end", "false", "for", "function", "goto", "if", "in", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while"
		};

		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static const char* const identifiers[] = {
			"assert", "collectgarbage", "dofile", "error", "getmetatable", "ipairs", "loadfile", "load", "loadstring",  "next",  "pairs",  "pcall",  "print",  "rawequal",  "rawlen",  "rawget",  "rawset",
			"select",  "setmetatable",  "tonumber",  "tostring",  "type",  "xpcall",  "_G",  "_VERSION","arshift", "band", "bnot", "bor", "bxor", "btest", "extract", "lrotate", "lshift", "replace",
			"rrotate", "rshift", "create", "resume", "running", "status", "wrap", "yield", "isyieldable", "debug","getuservalue", "gethook", "getinfo", "getlocal", "getregistry", "getmetatable",
			"getupvalue", "upvaluejoin", "upvalueid", "setuservalue", "sethook", "setlocal", "setmetatable", "setupvalue", "traceback", "close", "flush", "input", "lines", "open", "output", "popen",
			"read", "tmpfile", "type", "write", "close", "flush", "lines", "read", "seek", "setvbuf", "write", "__gc", "__tostring", "abs", "acos", "asin", "atan", "ceil", "cos", "deg", "exp", "tointeger",
			"floor", "fmod", "ult", "log", "max", "min", "modf", "rad", "random", "randomseed", "sin", "sqrt", "string", "tan", "type", "atan2", "cosh", "sinh", "tanh",
			"pow", "frexp", "ldexp", "log10", "pi", "huge", "maxinteger", "mininteger", "loadlib", "searchpath", "seeall", "preload", "cpath", "path", "searchers", "loaded", "module", "require", "clock",
			"date", "difftime", "execute", "exit", "getenv", "remove", "rename", "setlocale", "time", "tmpname", "byte", "char", "dump", "find", "format", "gmatch", "gsub", "len", "lower", "match", "rep",
			"reverse", "sub", "upper", "pack", "packsize", "unpack", "concat", "maxn", "insert", "pack", "unpack", "remove", "move", "sort", "offset", "codepoint", "char", "len", "codes", "charpattern",
			"coroutine", "table", "io", "os", "string", "utf8", "bit32", "math", "debug", "package"
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.mTokenize = [](const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end, PaletteIndex & paletteIndex) -> bool
		{
			paletteIndex = PaletteIndex::Max;

			while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
				in_begin++;

			if (in_begin == in_end)
			{
				out_begin = in_end;
				out_end = in_end;
				paletteIndex = PaletteIndex::Default;
			}
			else if (TokenizeLuaStyleString(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::String;
			else if (TokenizeLuaStyleIdentifier(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::Identifier;
			else if (TokenizeLuaStyleNumber(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::Number;
			else if (TokenizeLuaStylePunctuation(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::Punctuation;

			return paletteIndex != PaletteIndex::Max;
		};

		langDef.mCommentStart = "--[[";
		langDef.mCommentEnd = "]]";
		langDef.mSingleLineComment = "--";

		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = false;

		langDef.mName = "Lua";

		inited = true;
	}
	return langDef;
}

#if IMGUI_BUILD_EXAMPLE 
void TextEditor::text_edit_demo(bool * open)
{
	auto lang = TextEditor::LanguageDefinition::CPlusPlus();
	// set your own known preprocessor symbols...
	static const char* ppnames[] = { "NULL", "PM_REMOVE",
		"ZeroMemory", "DXGI_SWAP_EFFECT_DISCARD", "D3D_FEATURE_LEVEL", "D3D_DRIVER_TYPE_HARDWARE", "WINAPI","D3D11_SDK_VERSION", "assert" };
	// ... and their corresponding values
	static const char* ppvalues[] = { 
		"#define NULL ((void*)0)", 
		"#define PM_REMOVE (0x0001)",
		"Microsoft's own memory zapper function\n(which is a macro actually)\nvoid ZeroMemory(\n\t[in] PVOID  Destination,\n\t[in] SIZE_T Length\n); ", 
		"enum DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_DISCARD = 0", 
		"enum D3D_FEATURE_LEVEL", 
		"enum D3D_DRIVER_TYPE::D3D_DRIVER_TYPE_HARDWARE  = ( D3D_DRIVER_TYPE_UNKNOWN + 1 )",
		"#define WINAPI __stdcall",
		"#define D3D11_SDK_VERSION (7)",
		" #define assert(expression) (void)(                                                  \n"
        "    (!!(expression)) ||                                                              \n"
        "    (_wassert(_CRT_WIDE(#expression), _CRT_WIDE(__FILE__), (unsigned)(__LINE__)), 0) \n"
        " )"
		};
    // ... test program
    static const std::string program = " \n\
// dear imgui: \"null\" example application \n\
// (compile and link imgui, create context, run headless with NO INPUTS, NO GRAPHICS OUTPUT) \n\
// This is useful to test building, but you cannot interact with anything here! \n\
#include \"imgui.h\" \n\
#include <stdio.h> \n\
\n\
int main(int, char**) \n\
{ \n\
    IMGUI_CHECKVERSION(); \n\
    ImGui::CreateContext(); \n\
    ImGuiIO& io = ImGui::GetIO(); \n\
\n\
    // Build atlas \n\
    unsigned char* tex_pixels = NULL; \n\
    int tex_w, tex_h; \n\
    io.Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h); \n\
\n\
    for (int n = 0; n < 20; n++) \n\
    { \n\
        io.DisplaySize = ImVec2(1920, 1080); \n\
        io.DeltaTime = 1.0f / 60.0f; \n\
        ImGui::NewFrame(); \n\
\n\
        static float f = 0.0f; \n\
        ImGui::Text(\"Hello, world!\"); \n\
        ImGui::SliderFloat(\"float\", &f, 0.0f, 1.0f); \n\
        ImGui::Text(\"Application average %.3f ms/frame (%.1f FPS)\", 1000.0f / io.Framerate, io.Framerate); \n\
        ImGui::ShowDemoWindow(NULL); \n\
\n\
        ImGui::Render(); \n\
    } \n\
\n\
    ImGui::DestroyContext(); \n\
    return 0; \n\
} \n\
";

	for (int i = 0; i < sizeof(ppnames) / sizeof(ppnames[0]); ++i)
	{
		TextEditor::Identifier id;
		id.mDeclaration = ppvalues[i];
		lang.mPreprocIdentifiers.insert(std::make_pair(std::string(ppnames[i]), id));
	}

	// set your own identifiers
	static const char* identifiers[] = {
		"HWND", "HRESULT", "LPRESULT","D3D11_RENDER_TARGET_VIEW_DESC", "DXGI_SWAP_CHAIN_DESC","MSG","LRESULT","WPARAM", "LPARAM","UINT","LPVOID",
		"ID3D11Device", "ID3D11DeviceContext", "ID3D11Buffer", "ID3D11Buffer", "ID3D10Blob", "ID3D11VertexShader", "ID3D11InputLayout", "ID3D11Buffer",
		"ID3D10Blob", "ID3D11PixelShader", "ID3D11SamplerState", "ID3D11ShaderResourceView", "ID3D11RasterizerState", "ID3D11BlendState", "ID3D11DepthStencilState",
		"IDXGISwapChain", "ID3D11RenderTargetView", "ID3D11Texture2D", "TextEditor" };
	static const char* idecls[] = 
	{
		"typedef HWND_* HWND", "typedef long HRESULT", "typedef long* LPRESULT", "struct D3D11_RENDER_TARGET_VIEW_DESC", "struct DXGI_SWAP_CHAIN_DESC",
		"typedef tagMSG MSG\n * Message structure","typedef LONG_PTR LRESULT","WPARAM", "LPARAM","UINT","LPVOID",
		"ID3D11Device", "ID3D11DeviceContext", "ID3D11Buffer", "ID3D11Buffer", "ID3D10Blob", "ID3D11VertexShader", "ID3D11InputLayout", "ID3D11Buffer",
		"ID3D10Blob", "ID3D11PixelShader", "ID3D11SamplerState", "ID3D11ShaderResourceView", "ID3D11RasterizerState", "ID3D11BlendState", "ID3D11DepthStencilState",
		"IDXGISwapChain", "ID3D11RenderTargetView", "ID3D11Texture2D", "class TextEditor" };
	for (int i = 0; i < sizeof(identifiers) / sizeof(identifiers[0]); ++i)
	{
		TextEditor::Identifier id;
		id.mDeclaration = std::string(idecls[i]);
		lang.mIdentifiers.insert(std::make_pair(std::string(identifiers[i]), id));
	}
	SetLanguageDefinition(lang);

	TextEditor::ErrorMarkers markers;
	markers.insert(std::make_pair<int, std::string>(5, "Example error here:\nInclude file not found: \"imgui.h\""));
	markers.insert(std::make_pair<int, std::string>(41, "Another example error"));
	SetErrorMarkers(markers);

	auto cpos = GetCursorPosition();
	ImGui::Begin("Text Editor Demo", open, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar);
	ImGui::SetWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
            if (ImGui::MenuItem("Load"))
            {
                SetText(program);
            }
			if (ImGui::MenuItem("Save"))
			{
				auto textToSave = GetText();
				/// save text....
			}
			if (ImGui::MenuItem("Quit", "Alt-F4"))
			{
				*open = false;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			bool ro = IsReadOnly();
			if (ImGui::MenuItem("Read-only mode", nullptr, &ro))
				SetReadOnly(ro);
			ImGui::Separator();

			if (ImGui::MenuItem("Undo", "ALT-Backspace", nullptr, !ro && CanUndo()))
				Undo();
			if (ImGui::MenuItem("Redo", "Ctrl-Y", nullptr, !ro && CanRedo()))
				Redo();

			ImGui::Separator();

			if (ImGui::MenuItem("Copy", "Ctrl-C", nullptr, HasSelection()))
				Copy();
			if (ImGui::MenuItem("Cut", "Ctrl-X", nullptr, !ro && HasSelection()))
				Cut();
			if (ImGui::MenuItem("Delete", "Del", nullptr, !ro && HasSelection()))
				Delete();
			if (ImGui::MenuItem("Paste", "Ctrl-V", nullptr, !ro && ImGui::GetClipboardText() != nullptr))
				Paste();

			ImGui::Separator();

			if (ImGui::MenuItem("Select all", nullptr, nullptr))
				SetSelection(TextEditor::Coordinates(), TextEditor::Coordinates(GetTotalLines(), 0));

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("Dark palette"))
				SetPalette(TextEditor::GetDarkPalette());
			if (ImGui::MenuItem("Light palette"))
				SetPalette(TextEditor::GetLightPalette());
			if (ImGui::MenuItem("Retro blue palette"))
				SetPalette(TextEditor::GetRetroBluePalette());
            ImGui::Separator();

            bool show_space = IsShowingWhitespaces();
            if (ImGui::MenuItem("Show White Spaces", nullptr, &show_space))
            {
                SetShowWhitespaces(show_space);
            }
            bool show_short_tab = IsShowingShortTabGlyphs();
            if (ImGui::MenuItem("Show Short TAB", nullptr, &show_short_tab))
            {
                SetShowShortTabGlyphs(show_short_tab);
            }
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s", cpos.mLine + 1, cpos.mColumn + 1, GetTotalLines(),
		IsOverwrite() ? "Ovr" : "Ins",
		CanUndo() ? "*" : " ",
		GetLanguageDefinition().mName.c_str());

	Render("TextEditor");
	ImGui::End();
}
#endif
