#include <Foundation/Foundation.h>
#include <CoreText/CoreText.h>
#include "FontManager.h"

using namespace std;
using namespace FM;

// converts a CoreText weight (-1 to +1) to a standard weight (100 to 900)
static int convertWeight(float weight)
{
    if (weight <= -0.8f)
        return 100;
    else if (weight <= -0.6f)
        return 200;
    else if (weight <= -0.4f)
        return 300;
    else if (weight <= 0.0f)
        return 400;
    else if (weight <= 0.25f)
        return 500;
    else if (weight <= 0.35f)
        return 600;
    else if (weight <= 0.4f)
        return 700;
    else if (weight <= 0.6f)
        return 800;
    else
        return 900;
}

// converts a CoreText width (-1 to +1) to a standard width (1 to 9)
static int convertWidth(float unit)
{
    if (unit < 0)
    {
        return 1 + (1 + unit) * 4;
    }
    else
    {
        return 5 + unit * 4;
    }
}

static FontDescriptorHolder CreateFontDescriptor(CTFontDescriptorRef descriptor)
{
    NSURL *url = (NSURL *)CTFontDescriptorCopyAttribute(descriptor, kCTFontURLAttribute);
    NSString *psName = (NSString *)CTFontDescriptorCopyAttribute(descriptor, kCTFontNameAttribute);
    NSString *family = (NSString *)CTFontDescriptorCopyAttribute(descriptor, kCTFontFamilyNameAttribute);
    NSString *style = (NSString *)CTFontDescriptorCopyAttribute(descriptor, kCTFontStyleNameAttribute);

    NSDictionary *traits = (NSDictionary *)CTFontDescriptorCopyAttribute(descriptor, kCTFontTraitsAttribute);
    NSNumber *weightVal = traits[(id)kCTFontWeightTrait];
    FontWeight weight = (FontWeight)convertWeight([weightVal floatValue]);

    NSNumber *widthVal = traits[(id)kCTFontWidthTrait];
    FontWidth width = (FontWidth)convertWidth([widthVal floatValue]);

    NSNumber *symbolicTraitsVal = traits[(id)kCTFontSymbolicTrait];
    unsigned int symbolicTraits = [symbolicTraitsVal unsignedIntValue];

    FontDescriptorHolder res = NewFontDescriptor(
        [[url path] UTF8String],
        [psName UTF8String],
        [family UTF8String],
        [style UTF8String],
        weight,
        width,
        (symbolicTraits & kCTFontItalicTrait) != 0,
        (symbolicTraits & kCTFontMonoSpaceTrait) != 0);

    [url release];
    [psName release];
    [family release];
    [style release];
    [traits release];
    return res;
}

vector<FontDescriptorHolder> FM::GetAvailableFonts()
{
    // cache font collection for fast use in future calls
    static CTFontCollectionRef collection = NULL;
    if (collection == NULL)
        collection = CTFontCollectionCreateFromAvailableFonts(NULL);

    NSArray *matches = (NSArray *)CTFontCollectionCreateMatchingFontDescriptors(collection);

    vector<FontDescriptorHolder> results;
    results.reserve(matches->size());
    for (id m in matches)
    {
        CTFontDescriptorRef match = (CTFontDescriptorRef)m;
        results->push_back(CreateFontDescriptor(match));
    }

    [matches release];
    return results;
}

unordered_map<string, vector<FontDescriptorHolder>> FM::GroupFontsByFamily(const vector<FontDescriptorHolder>& fonts)
{
    unordered_map<string, vector<FontDescriptorHolder>> result;
    for (auto hFD : fonts)
    {
        auto iter = result.find(hFD->Family());
        if (iter == result.end())
        {
            vector<FontDescriptorHolder> group;
            group.push_back(hFD);
            result[hFD->Family()] = move(group);
        }
        else
        {
            iter->second.push_back(hFD);
        }
    }
    return result;
}
