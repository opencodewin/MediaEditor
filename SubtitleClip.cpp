#include "SubtitleClip.h"

using namespace std;
using namespace DataLayer;

SubtitleImage::SubtitleImage(ImGui::ImMat& image, const Rect& area)
    : m_image(image), m_area(area)
{}
