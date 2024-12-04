#include <immat.h>

namespace ImGui 
{
// color space matrix
const float r2y_601_full[3][3] = {
    { 0.299000f,  0.587000f,  0.114000f},
    {-0.168736f, -0.331264f,  0.500000f},
    { 0.500000f, -0.418688f, -0.081312f}
};

const float r2y_601_narrow[3][3] = {
    { 0.256788f,  0.515639f,  0.100141f},
    {-0.144914f, -0.290993f,  0.439216f},
    { 0.429412f, -0.367788f, -0.071427f}
};

const float r2y_709_full[3][3] = {
    { 0.212600f,  0.715200f,  0.072200f},
    {-0.114572f, -0.385428f,  0.500000f},
    { 0.500000f, -0.454153f, -0.045847f}
};

const float r2y_709_narrow[3][3] = {
    { 0.182586f,  0.628254f,  0.063423f},
    {-0.098397f, -0.338572f,  0.439216f},
    { 0.429412f, -0.398942f, -0.040274f}
};

const float r2y_2020_full[3][3] = {
    { 0.262700f,  0.678000f,  0.059300f},
    {-0.139630f, -0.360370f,  0.500000f},
    { 0.500000f, -0.459786f, -0.040214f}
};

const float r2y_2020_narrow[3][3] = {
    { 0.225613f,  0.595576f,  0.052091f},
    {-0.119918f, -0.316560f,  0.439216f},
    { 0.429412f, -0.403890f, -0.035325f}
};

const float y2r_601_full[3][3] = {
    {1.000000f,  0.000000f,  1.402000f},
    {1.000000f, -0.344136f, -0.714136f},
    {1.000000f,  1.772000f,  0.000000f}
};

const float y2r_601_narrow[3][3] = {
    {1.164384f,  0.000000f,  1.596027f},
    {1.164384f, -0.391762f, -0.812968f},
    {1.164384f,  2.017232f,  0.000000f}
};

const float y2r_709_full[3][3] = {
    {1.000000f,  0.000000f,  1.574800f},
    {1.000000f, -0.187324f, -0.468124f},
    {1.000000f,  1.855600f,  0.000000f}
};

const float y2r_709_narrow[3][3] = {
    {1.164384f,  0.000000f,  1.792741f},
    {1.164384f, -0.213249f, -0.532909f},
    {1.164384f,  2.112402f,  0.000000f}
};

const float y2r_2020_full[3][3] = {
    {1.000000f,  0.000000f,  1.474600f},
    {1.000000f, -0.164553f, -0.571353f},
    {1.000000f,  1.881400f,  0.000000f}
};

const float y2r_2020_narrow[3][3] = {
    {1.164384f,  0.000000f,  1.678674f},
    {1.164384f, -0.187326f, -0.650424f},
    {1.164384f,  2.141772f,  0.000000f}
};

const float srgb[3][3] = {
    {1.000000f, 1.000000f, 1.000000f},
    {1.000000f, 1.000000f, 1.000000f},
    {1.000000f, 1.000000f, 1.000000f}
};

const ImMat matrix_yr_601_full   (3, 3, (void *)y2r_601_full,   sizeof(float));
const ImMat matrix_yr_601_narrow (3, 3, (void *)y2r_601_narrow, sizeof(float));
const ImMat matrix_yr_709_full   (3, 3, (void *)y2r_709_full,   sizeof(float));
const ImMat matrix_yr_709_narrow (3, 3, (void *)y2r_709_narrow, sizeof(float));
const ImMat matrix_yr_2020_full  (3, 3, (void *)y2r_2020_full,  sizeof(float));
const ImMat matrix_yr_2020_narrow(3, 3, (void *)y2r_2020_full,  sizeof(float));

const ImMat matrix_ry_601_full   (3, 3, (void *)r2y_601_full,   sizeof(float));
const ImMat matrix_ry_601_narrow (3, 3, (void *)r2y_601_narrow, sizeof(float));
const ImMat matrix_ry_709_full   (3, 3, (void *)r2y_709_full,   sizeof(float));
const ImMat matrix_ry_709_narrow (3, 3, (void *)r2y_709_narrow, sizeof(float));
const ImMat matrix_ry_2020_full  (3, 3, (void *)r2y_2020_full,  sizeof(float));
const ImMat matrix_ry_2020_narrow(3, 3, (void *)r2y_2020_full,  sizeof(float));

const ImMat matrix_srgb(3, 3, (void *)srgb,  sizeof(float));

const ImMat * color_table[2][2][4] = {
    {
        { &matrix_srgb, &matrix_yr_601_full,   &matrix_yr_709_full,   &matrix_yr_2020_full},
        { &matrix_srgb, &matrix_yr_601_narrow, &matrix_yr_709_narrow, &matrix_yr_2020_narrow}
    },
    {
        { &matrix_srgb, &matrix_ry_601_full,   &matrix_ry_709_full,   &matrix_ry_2020_full},
        { &matrix_srgb, &matrix_ry_601_narrow, &matrix_ry_709_narrow, &matrix_ry_2020_narrow}
    }
};

const float r2x_D65_Adobe[3][3] = {
    { 0.5767309,  0.1855540,  0.1881852},
    { 0.2973769,  0.6273491,  0.0752741},
    { 0.0270343,  0.0706872,  0.9911085}
};

const float r2x_D65_Apple[3][3] = {
    { 0.4497288,  0.3162486,  0.1844926},
    { 0.2446525,  0.6720283,  0.0833192},
    { 0.0251848,  0.1411824,  0.9224628}
};

const float r2x_D65_Bruce[3][3] = {
    { 0.4674162,  0.2944512,  0.1886026},
    { 0.2410115,  0.6835475,  0.0754410},
    { 0.0219101,  0.0736128,  0.9933071}
};

const float r2x_D65_PAL[3][3] = {
    { 0.4306190,  0.3415419,  0.1783091},
    { 0.2220379,  0.7066384,  0.0713236},
    { 0.0201853,  0.1295504,  0.9390944}
};

const float r2x_D65_NTSC[3][3] = {
    { 0.6068909,  0.1735011,  0.2003480},
    { 0.2989164,  0.5865990,  0.1144845},
    { 0.0000000,  0.0660957,  1.1162243}
};

const float r2x_D65_SMPTE[3][3] = {
    { 0.3935891,  0.3652497,  0.1916313},
    { 0.2124132,  0.7010437,  0.0865432},
    { 0.0187423,  0.1119313,  0.9581563}
};

const float r2x_D65_CIE[3][3] = {
    { 0.4887180,  0.3106803,  0.2006017},
    { 0.1762044,  0.8129847,  0.0108109},
    { 0.0000000,  0.0102048,  0.9897952}
};

const float r2x_D65_sRGB[3][3] = {
    { 0.4124564,  0.3575761,  0.1804375},
    { 0.2126729,  0.7151522,  0.0721750},
    { 0.0193339,  0.1191920,  0.9503041}
};

const float r2x_D50_Adobe[3][3] = {
    { 0.6097559,  0.2052401,  0.1492240},
    { 0.3111242,  0.6256560,  0.0632197},
    { 0.0194811,  0.0608902,  0.7448387}
};

const float r2x_D50_Apple[3][3] = {
    { 0.4755678,  0.3396722,  0.1489800},
    { 0.2551812,  0.6725693,  0.0722496},
    { 0.0184697,  0.1133771,  0.6933632}
};

const float r2x_D50_Bruce[3][3] = {
    { 0.4941816,  0.3204834,  0.1495550},
    { 0.2521531,  0.6844869,  0.0633600},
    { 0.0157886,  0.0629304,  0.7464909}
};

const float r2x_D50_PAL[3][3] = {
    { 0.4552773,  0.3675500,  0.1413926},
    { 0.2323025,  0.7077956,  0.0599019},
    { 0.0145457,  0.1049154,  0.7057489}
};

const float r2x_D50_NTSC[3][3] = {
    { 0.6343706,  0.1852204,  0.1446290},
    { 0.3109496,  0.5915984,  0.0974520},
    {-0.0011817,  0.0555518,  0.7708399}
};

const float r2x_D50_SMPTE[3][3] = {
    { 0.4163290,  0.3931464,  0.1547446},
    { 0.2216999,  0.7032549,  0.0750452},
    { 0.0136576,  0.0913604,  0.7201920}
};

const float r2x_D50_CIE[3][3] = {
    { 0.4868870,  0.3062984,  0.1710347},
    { 0.1746583,  0.8247541,  0.0005877},
    {-0.0012563,  0.0169832,  0.8094831}
};

const float r2x_D50_sRGB[3][3] = {
    { 0.4360747,  0.3850649,  0.1430804},
    { 0.2225045,  0.7168786,  0.0606169},
    { 0.0139322,  0.0971045,  0.7141733}
};

const float x2r_D65_Adobe[3][3] = {
    { 2.0413690, -0.5649464, -0.3446944},
    {-0.9692660,  1.8760108,  0.0415560},
    { 0.0134474, -0.1183897,  1.0154096}
};

const float x2r_D65_Apple[3][3] = {
    { 2.9515373, -1.2894116, -0.4738445},
    {-1.0851093,  1.9908566,  0.0372026},
    { 0.0854934, -0.2694964,  1.0912975}
};

const float x2r_D65_Bruce[3][3] = {
    { 2.7454669, -1.1358136, -0.4350269},
    {-0.9692660,  1.8760108,  0.0415560},
    { 0.0112723, -0.1139754,  1.013254}
};

const float x2r_D65_PAL[3][3] = {
    { 3.0628971, -1.3931791, -0.4757517},
    {-0.9692660,  1.8760108,  0.0415560},
    { 0.0678775, -0.2288548,  1.0693490}
};

const float x2r_D65_NTSC[3][3] = {
    { 1.9099961, -0.5324542, -0.2882091},
    {-0.9846663,  1.9991710, -0.0283082},
    { 0.0583056, -0.1183781,  0.8975535}
};

const float x2r_D65_SMPTE[3][3] = {
    { 3.5053960, -1.7394894, -0.5439640},
    {-1.0690722,  1.9778245,  0.0351722},
    { 0.0563200, -0.1970226,  1.0502026}
};

const float x2r_D65_CIE[3][3] = {
    { 2.3706743, -0.9000405, -0.4706338},
    {-0.5138850,  1.4253036,  0.0885814},
    { 0.0052982, -0.0146949,  1.0093968}
};

const float x2r_D65_sRGB[3][3] = {
    { 3.2404542, -1.5371385, -0.4985314},
    {-0.9692660,  1.8760108,  0.0415560},
    { 0.0556434, -0.2040259,  1.0572252}
};

const float x2r_D50_Adobe[3][3] = {
    { 1.9624274, -0.6105343, -0.3413404},
    {-0.9787684,  1.9161415,  0.0334540},
    { 0.0286869, -0.1406752,  1.3487655}
};

const float x2r_D50_Apple[3][3] = {
    { 2.8510695, -1.3605261, -0.4708281},
    {-1.0927680,  2.0348871,  0.0227598},
    { 0.1027403, -0.2964984,  1.4510659}
};

const float x2r_D50_Bruce[3][3] = {
    { 2.6502856, -1.2014485, -0.4289936},
    {-0.9787684,  1.9161415,  0.0334540},
    { 0.0264570, -0.1361227,  1.3458542}
};

const float x2r_D50_PAL[3][3] = {
    { 2.9603944, -1.4678519, -0.4685105},
    {-0.9787684,  1.9161415,  0.0334540},
    { 0.0844874, -0.2545973,  1.4216174}
};

const float x2r_D50_NTSC[3][3] = {
    { 1.8464881, -0.5521299, -0.2766458},
    {-0.9826630,  2.0044755, -0.0690396},
    { 0.0736477, -0.1453020,  1.3018376}
};

const float x2r_D50_SMPTE[3][3] = {
    { 3.3921940, -1.8264027, -0.5385522},
    {-1.0770996,  2.0213975,  0.0207989},
    { 0.0723073, -0.2217902,  1.3960932}
};

const float x2r_D50_CIE[3][3] = {
    { 2.3638081, -0.8676030, -0.4988161},
    {-0.5005940,  1.3962369,  0.1047562},
    { 0.0141712, -0.0306400,  1.2323842}
};

const float x2r_D50_sRGB[3][3] = {
    { 3.1338561, -1.6168667, -0.4906146},
    {-0.9787684,  1.9161415,  0.0334540},
    { 0.0719453, -0.2289914,  1.4052427}
};

const ImMat matrix_rx_D50_Adobe     (3, 3, (void *)r2x_D50_Adobe,   sizeof(float));
const ImMat matrix_rx_D50_Apple     (3, 3, (void *)r2x_D50_Apple,   sizeof(float));
const ImMat matrix_rx_D50_Bruce     (3, 3, (void *)r2x_D50_Bruce,   sizeof(float));
const ImMat matrix_rx_D50_PAL       (3, 3, (void *)r2x_D50_PAL,     sizeof(float));
const ImMat matrix_rx_D50_NTSC      (3, 3, (void *)r2x_D50_NTSC,    sizeof(float));
const ImMat matrix_rx_D50_SMPTE     (3, 3, (void *)r2x_D50_SMPTE,   sizeof(float));
const ImMat matrix_rx_D50_CIE       (3, 3, (void *)r2x_D50_CIE,     sizeof(float));
const ImMat matrix_rx_D50_sRGB      (3, 3, (void *)r2x_D50_sRGB,    sizeof(float));

const ImMat matrix_xr_D50_Adobe     (3, 3, (void *)x2r_D50_Adobe,   sizeof(float));
const ImMat matrix_xr_D50_Apple     (3, 3, (void *)x2r_D50_Apple,   sizeof(float));
const ImMat matrix_xr_D50_Bruce     (3, 3, (void *)x2r_D50_Bruce,   sizeof(float));
const ImMat matrix_xr_D50_PAL       (3, 3, (void *)x2r_D50_PAL,     sizeof(float));
const ImMat matrix_xr_D50_NTSC      (3, 3, (void *)x2r_D50_NTSC,    sizeof(float));
const ImMat matrix_xr_D50_SMPTE     (3, 3, (void *)x2r_D50_SMPTE,   sizeof(float));
const ImMat matrix_xr_D50_CIE       (3, 3, (void *)x2r_D50_CIE,     sizeof(float));
const ImMat matrix_xr_D50_sRGB      (3, 3, (void *)x2r_D50_sRGB,    sizeof(float));

const ImMat matrix_rx_D65_Adobe     (3, 3, (void *)r2x_D65_Adobe,   sizeof(float));
const ImMat matrix_rx_D65_Apple     (3, 3, (void *)r2x_D65_Apple,   sizeof(float));
const ImMat matrix_rx_D65_Bruce     (3, 3, (void *)r2x_D65_Bruce,   sizeof(float));
const ImMat matrix_rx_D65_PAL       (3, 3, (void *)r2x_D65_PAL,     sizeof(float));
const ImMat matrix_rx_D65_NTSC      (3, 3, (void *)r2x_D65_NTSC,    sizeof(float));
const ImMat matrix_rx_D65_SMPTE     (3, 3, (void *)r2x_D65_SMPTE,   sizeof(float));
const ImMat matrix_rx_D65_CIE       (3, 3, (void *)r2x_D65_CIE,     sizeof(float));
const ImMat matrix_rx_D65_sRGB      (3, 3, (void *)r2x_D65_sRGB,    sizeof(float));

const ImMat matrix_xr_D65_Adobe     (3, 3, (void *)x2r_D65_Adobe,   sizeof(float));
const ImMat matrix_xr_D65_Apple     (3, 3, (void *)x2r_D65_Apple,   sizeof(float));
const ImMat matrix_xr_D65_Bruce     (3, 3, (void *)x2r_D65_Bruce,   sizeof(float));
const ImMat matrix_xr_D65_PAL       (3, 3, (void *)x2r_D65_PAL,     sizeof(float));
const ImMat matrix_xr_D65_NTSC      (3, 3, (void *)x2r_D65_NTSC,    sizeof(float));
const ImMat matrix_xr_D65_SMPTE     (3, 3, (void *)x2r_D65_SMPTE,   sizeof(float));
const ImMat matrix_xr_D65_CIE       (3, 3, (void *)x2r_D65_CIE,     sizeof(float));
const ImMat matrix_xr_D65_sRGB      (3, 3, (void *)x2r_D65_sRGB,    sizeof(float));

const ImMat * xyz_color_table[2][2][8] = {
    {
        {&matrix_rx_D50_sRGB, &matrix_rx_D50_Adobe, &matrix_rx_D50_Apple, &matrix_rx_D50_Bruce, &matrix_rx_D50_PAL, &matrix_rx_D50_NTSC, &matrix_rx_D50_SMPTE, &matrix_rx_D50_CIE},
        {&matrix_rx_D65_sRGB, &matrix_rx_D65_Adobe, &matrix_rx_D65_Apple, &matrix_rx_D65_Bruce, &matrix_rx_D65_PAL, &matrix_rx_D65_NTSC, &matrix_rx_D65_SMPTE, &matrix_rx_D65_CIE}
    },
    {
        {&matrix_xr_D50_sRGB, &matrix_xr_D50_Adobe, &matrix_xr_D50_Apple, &matrix_xr_D50_Bruce, &matrix_xr_D50_PAL, &matrix_xr_D50_NTSC, &matrix_xr_D50_SMPTE, &matrix_xr_D50_CIE},
        {&matrix_xr_D65_sRGB, &matrix_xr_D65_Adobe, &matrix_xr_D65_Apple, &matrix_xr_D65_Bruce, &matrix_xr_D65_PAL, &matrix_xr_D65_NTSC, &matrix_xr_D65_SMPTE, &matrix_xr_D65_CIE}
    }
};

} // namespace ImGui 

