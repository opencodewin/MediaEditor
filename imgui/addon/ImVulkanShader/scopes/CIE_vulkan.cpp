#include "CIE_vulkan.h"
#include "CIE_shader.h"
#include "ImVulkanShader.h"
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MAX3(a,b,c) MAX(MAX(a,b),c)
#endif
static float const spectral_chromaticity[][3] = 
{
    { 0.175560, 0.005294, 0.819146 },
    { 0.175483, 0.005286, 0.819231 },
    { 0.175400, 0.005279, 0.819321 },
    { 0.175317, 0.005271, 0.819412 },
    { 0.175237, 0.005263, 0.819500 },
    { 0.175161, 0.005256, 0.819582 },
    { 0.175088, 0.005247, 0.819665 },
    { 0.175015, 0.005236, 0.819749 },
    { 0.174945, 0.005226, 0.819829 },
    { 0.174880, 0.005221, 0.819899 },
    { 0.174821, 0.005221, 0.819959 },
    { 0.174770, 0.005229, 0.820001 },
    { 0.174722, 0.005238, 0.820040 },
    { 0.174665, 0.005236, 0.820098 },
    { 0.174595, 0.005218, 0.820187 },
    { 0.174510, 0.005182, 0.820309 },
    { 0.174409, 0.005127, 0.820464 },
    { 0.174308, 0.005068, 0.820624 },
    { 0.174222, 0.005017, 0.820761 },
    { 0.174156, 0.004981, 0.820863 },
    { 0.174112, 0.004964, 0.820924 },
    { 0.174088, 0.004964, 0.820948 },
    { 0.174073, 0.004973, 0.820955 },
    { 0.174057, 0.004982, 0.820961 },
    { 0.174036, 0.004986, 0.820978 },
    { 0.174008, 0.004981, 0.821012 },
    { 0.173972, 0.004964, 0.821064 },
    { 0.173932, 0.004943, 0.821125 },
    { 0.173889, 0.004926, 0.821185 },
    { 0.173845, 0.004916, 0.821239 },
    { 0.173801, 0.004915, 0.821284 },
    { 0.173754, 0.004925, 0.821321 },
    { 0.173705, 0.004937, 0.821358 },
    { 0.173655, 0.004944, 0.821401 },
    { 0.173606, 0.004940, 0.821454 },
    { 0.173560, 0.004923, 0.821517 },
    { 0.173514, 0.004895, 0.821590 },
    { 0.173468, 0.004865, 0.821667 },
    { 0.173424, 0.004836, 0.821740 },
    { 0.173380, 0.004813, 0.821807 },
    { 0.173337, 0.004797, 0.821866 },
    { 0.173291, 0.004786, 0.821923 },
    { 0.173238, 0.004779, 0.821983 },
    { 0.173174, 0.004775, 0.822051 },
    { 0.173101, 0.004774, 0.822125 },
    { 0.173021, 0.004775, 0.822204 },
    { 0.172934, 0.004781, 0.822285 },
    { 0.172843, 0.004791, 0.822366 },
    { 0.172751, 0.004799, 0.822450 },
    { 0.172662, 0.004802, 0.822536 },
    { 0.172577, 0.004799, 0.822624 },
    { 0.172489, 0.004795, 0.822715 },
    { 0.172396, 0.004796, 0.822808 },
    { 0.172296, 0.004803, 0.822901 },
    { 0.172192, 0.004815, 0.822993 },
    { 0.172087, 0.004833, 0.823081 },
    { 0.171982, 0.004855, 0.823163 },
    { 0.171871, 0.004889, 0.823240 },
    { 0.171741, 0.004939, 0.823319 },
    { 0.171587, 0.005010, 0.823402 },
    { 0.171407, 0.005102, 0.823490 },
    { 0.171206, 0.005211, 0.823583 },
    { 0.170993, 0.005334, 0.823674 },
    { 0.170771, 0.005470, 0.823759 },
    { 0.170541, 0.005621, 0.823838 },
    { 0.170301, 0.005789, 0.823911 },
    { 0.170050, 0.005974, 0.823976 },
    { 0.169786, 0.006177, 0.824037 },
    { 0.169505, 0.006398, 0.824097 },
    { 0.169203, 0.006639, 0.824158 },
    { 0.168878, 0.006900, 0.824222 },
    { 0.168525, 0.007184, 0.824291 },
    { 0.168146, 0.007491, 0.824363 },
    { 0.167746, 0.007821, 0.824433 },
    { 0.167328, 0.008175, 0.824496 },
    { 0.166895, 0.008556, 0.824549 },
    { 0.166446, 0.008964, 0.824589 },
    { 0.165977, 0.009402, 0.824622 },
    { 0.165483, 0.009865, 0.824652 },
    { 0.164963, 0.010351, 0.824687 },
    { 0.164412, 0.010858, 0.824731 },
    { 0.163828, 0.011385, 0.824787 },
    { 0.163210, 0.011937, 0.824853 },
    { 0.162552, 0.012520, 0.824928 },
    { 0.161851, 0.013137, 0.825011 },
    { 0.161105, 0.013793, 0.825102 },
    { 0.160310, 0.014491, 0.825199 },
    { 0.159466, 0.015232, 0.825302 },
    { 0.158573, 0.016015, 0.825412 },
    { 0.157631, 0.016840, 0.825529 },
    { 0.156641, 0.017705, 0.825654 },
    { 0.155605, 0.018609, 0.825786 },
    { 0.154525, 0.019556, 0.825920 },
    { 0.153397, 0.020554, 0.826049 },
    { 0.152219, 0.021612, 0.826169 },
    { 0.150985, 0.022740, 0.826274 },
    { 0.149691, 0.023950, 0.826359 },
    { 0.148337, 0.025247, 0.826416 },
    { 0.146928, 0.026635, 0.826437 },
    { 0.145468, 0.028118, 0.826413 },
    { 0.143960, 0.029703, 0.826337 },
    { 0.142405, 0.031394, 0.826201 },
    { 0.140796, 0.033213, 0.825991 },
    { 0.139121, 0.035201, 0.825679 },
    { 0.137364, 0.037403, 0.825233 },
    { 0.135503, 0.039879, 0.824618 },
    { 0.133509, 0.042692, 0.823798 },
    { 0.131371, 0.045876, 0.822753 },
    { 0.129086, 0.049450, 0.821464 },
    { 0.126662, 0.053426, 0.819912 },
    { 0.124118, 0.057803, 0.818079 },
    { 0.121469, 0.062588, 0.815944 },
    { 0.118701, 0.067830, 0.813468 },
    { 0.115807, 0.073581, 0.810612 },
    { 0.112776, 0.079896, 0.807328 },
    { 0.109594, 0.086843, 0.803563 },
    { 0.106261, 0.094486, 0.799253 },
    { 0.102776, 0.102864, 0.794360 },
    { 0.099128, 0.112007, 0.788865 },
    { 0.095304, 0.121945, 0.782751 },
    { 0.091294, 0.132702, 0.776004 },
    { 0.087082, 0.144317, 0.768601 },
    { 0.082680, 0.156866, 0.760455 },
    { 0.078116, 0.170420, 0.751464 },
    { 0.073437, 0.185032, 0.741531 },
    { 0.068706, 0.200723, 0.730571 },
    { 0.063993, 0.217468, 0.718539 },
    { 0.059316, 0.235254, 0.705430 },
    { 0.054667, 0.254096, 0.691238 },
    { 0.050031, 0.274002, 0.675967 },
    { 0.045391, 0.294976, 0.659633 },
    { 0.040757, 0.316981, 0.642262 },
    { 0.036195, 0.339900, 0.623905 },
    { 0.031756, 0.363598, 0.604646 },
    { 0.027494, 0.387921, 0.584584 },
    { 0.023460, 0.412703, 0.563837 },
    { 0.019705, 0.437756, 0.542539 },
    { 0.016268, 0.462955, 0.520777 },
    { 0.013183, 0.488207, 0.498610 },
    { 0.010476, 0.513404, 0.476120 },
    { 0.008168, 0.538423, 0.453409 },
    { 0.006285, 0.563068, 0.430647 },
    { 0.004875, 0.587116, 0.408008 },
    { 0.003982, 0.610447, 0.385570 },
    { 0.003636, 0.633011, 0.363352 },
    { 0.003859, 0.654823, 0.341318 },
    { 0.004646, 0.675898, 0.319456 },
    { 0.006011, 0.696120, 0.297869 },
    { 0.007988, 0.715342, 0.276670 },
    { 0.010603, 0.733413, 0.255984 },
    { 0.013870, 0.750186, 0.235943 },
    { 0.017766, 0.765612, 0.216622 },
    { 0.022244, 0.779630, 0.198126 },
    { 0.027273, 0.792104, 0.180623 },
    { 0.032820, 0.802926, 0.164254 },
    { 0.038852, 0.812016, 0.149132 },
    { 0.045328, 0.819391, 0.135281 },
    { 0.052177, 0.825164, 0.122660 },
    { 0.059326, 0.829426, 0.111249 },
    { 0.066716, 0.832274, 0.101010 },
    { 0.074302, 0.833803, 0.091894 },
    { 0.082053, 0.834090, 0.083856 },
    { 0.089942, 0.833289, 0.076769 },
    { 0.097940, 0.831593, 0.070468 },
    { 0.106021, 0.829178, 0.064801 },
    { 0.114161, 0.826207, 0.059632 },
    { 0.122347, 0.822770, 0.054882 },
    { 0.130546, 0.818928, 0.050526 },
    { 0.138702, 0.814774, 0.046523 },
    { 0.146773, 0.810395, 0.042832 },
    { 0.154722, 0.805864, 0.039414 },
    { 0.162535, 0.801238, 0.036226 },
    { 0.170237, 0.796519, 0.033244 },
    { 0.177850, 0.791687, 0.030464 },
    { 0.185391, 0.786728, 0.027881 },
    { 0.192876, 0.781629, 0.025495 },
    { 0.200309, 0.776399, 0.023292 },
    { 0.207690, 0.771055, 0.021255 },
    { 0.215030, 0.765595, 0.019375 },
    { 0.222337, 0.760020, 0.017643 },
    { 0.229620, 0.754329, 0.016051 },
    { 0.236885, 0.748524, 0.014591 },
    { 0.244133, 0.742614, 0.013253 },
    { 0.251363, 0.736606, 0.012031 },
    { 0.258578, 0.730507, 0.010916 },
    { 0.265775, 0.724324, 0.009901 },
    { 0.272958, 0.718062, 0.008980 },
    { 0.280129, 0.711725, 0.008146 },
    { 0.287292, 0.705316, 0.007391 },
    { 0.294450, 0.698842, 0.006708 },
    { 0.301604, 0.692308, 0.006088 },
    { 0.308760, 0.685712, 0.005528 },
    { 0.315914, 0.679063, 0.005022 },
    { 0.323066, 0.672367, 0.004566 },
    { 0.330216, 0.665628, 0.004156 },
    { 0.337363, 0.658848, 0.003788 },
    { 0.344513, 0.652028, 0.003459 },
    { 0.351664, 0.645172, 0.003163 },
    { 0.358814, 0.638287, 0.002899 },
    { 0.365959, 0.631379, 0.002662 },
    { 0.373102, 0.624451, 0.002448 },
    { 0.380244, 0.617502, 0.002254 },
    { 0.387379, 0.610542, 0.002079 },
    { 0.394507, 0.603571, 0.001922 },
    { 0.401626, 0.596592, 0.001782 },
    { 0.408736, 0.589607, 0.001657 },
    { 0.415836, 0.582618, 0.001546 },
    { 0.422921, 0.575631, 0.001448 },
    { 0.429989, 0.568649, 0.001362 },
    { 0.437036, 0.561676, 0.001288 },
    { 0.444062, 0.554714, 0.001224 },
    { 0.451065, 0.547766, 0.001169 },
    { 0.458041, 0.540837, 0.001123 },
    { 0.464986, 0.533930, 0.001084 },
    { 0.471899, 0.527051, 0.001051 },
    { 0.478775, 0.520202, 0.001023 },
    { 0.485612, 0.513389, 0.001000 },
    { 0.492405, 0.506615, 0.000980 },
    { 0.499151, 0.499887, 0.000962 },
    { 0.505845, 0.493211, 0.000944 },
    { 0.512486, 0.486591, 0.000923 },
    { 0.519073, 0.480029, 0.000899 },
    { 0.525600, 0.473527, 0.000872 },
    { 0.532066, 0.467091, 0.000843 },
    { 0.538463, 0.460725, 0.000812 },
    { 0.544787, 0.454434, 0.000779 },
    { 0.551031, 0.448225, 0.000744 },
    { 0.557193, 0.442099, 0.000708 },
    { 0.563269, 0.436058, 0.000673 },
    { 0.569257, 0.430102, 0.000641 },
    { 0.575151, 0.424232, 0.000616 },
    { 0.580953, 0.418447, 0.000601 },
    { 0.586650, 0.412758, 0.000591 },
    { 0.592225, 0.407190, 0.000586 },
    { 0.597658, 0.401762, 0.000580 },
    { 0.602933, 0.396497, 0.000571 },
    { 0.608035, 0.391409, 0.000556 },
    { 0.612977, 0.386486, 0.000537 },
    { 0.617779, 0.381706, 0.000516 },
    { 0.622459, 0.377047, 0.000493 },
    { 0.627037, 0.372491, 0.000472 },
    { 0.631521, 0.368026, 0.000453 },
    { 0.635900, 0.363665, 0.000435 },
    { 0.640156, 0.359428, 0.000416 },
    { 0.644273, 0.355331, 0.000396 },
    { 0.648233, 0.351395, 0.000372 },
    { 0.652028, 0.347628, 0.000344 },
    { 0.655669, 0.344018, 0.000313 },
    { 0.659166, 0.340553, 0.000281 },
    { 0.662528, 0.337221, 0.000251 },
    { 0.665764, 0.334011, 0.000226 },
    { 0.668874, 0.330919, 0.000207 },
    { 0.671859, 0.327947, 0.000194 },
    { 0.674720, 0.325095, 0.000185 },
    { 0.677459, 0.322362, 0.000179 },
    { 0.680079, 0.319747, 0.000174 },
    { 0.682582, 0.317249, 0.000170 },
    { 0.684971, 0.314863, 0.000167 },
    { 0.687250, 0.312586, 0.000164 },
    { 0.689426, 0.310414, 0.000160 },
    { 0.691504, 0.308342, 0.000154 },
    { 0.693490, 0.306366, 0.000145 },
    { 0.695389, 0.304479, 0.000133 },
    { 0.697206, 0.302675, 0.000119 },
    { 0.698944, 0.300950, 0.000106 },
    { 0.700606, 0.299301, 0.000093 },
    { 0.702193, 0.297725, 0.000083 },
    { 0.703709, 0.296217, 0.000074 },
    { 0.705163, 0.294770, 0.000067 },
    { 0.706563, 0.293376, 0.000061 },
    { 0.707918, 0.292027, 0.000055 },
    { 0.709231, 0.290719, 0.000050 },
    { 0.710500, 0.289453, 0.000047 },
    { 0.711724, 0.288232, 0.000044 },
    { 0.712901, 0.287057, 0.000041 },
    { 0.714032, 0.285929, 0.000040 },
    { 0.715117, 0.284845, 0.000038 },
    { 0.716159, 0.283804, 0.000036 },
    { 0.717159, 0.282806, 0.000035 },
    { 0.718116, 0.281850, 0.000034 },
    { 0.719033, 0.280935, 0.000032 },
    { 0.719912, 0.280058, 0.000030 },
    { 0.720753, 0.279219, 0.000028 },
    { 0.721555, 0.278420, 0.000026 },
    { 0.722315, 0.277662, 0.000023 },
    { 0.723032, 0.276948, 0.000020 },
    { 0.723702, 0.276282, 0.000016 },
    { 0.724328, 0.275660, 0.000012 },
    { 0.724914, 0.275078, 0.000007 },
    { 0.725467, 0.274530, 0.000003 },
    { 0.725992, 0.274008, 0.000000 },
    { 0.726495, 0.273505, 0.000000 },
    { 0.726975, 0.273025, 0.000000 },
    { 0.727432, 0.272568, 0.000000 },
    { 0.727864, 0.272136, 0.000000 },
    { 0.728272, 0.271728, 0.000000 },
    { 0.728656, 0.271344, 0.000000 },
    { 0.729020, 0.270980, 0.000000 },
    { 0.729361, 0.270639, 0.000000 },
    { 0.729678, 0.270322, 0.000000 },
    { 0.729969, 0.270031, 0.000000 },
    { 0.730234, 0.269766, 0.000000 },
    { 0.730474, 0.269526, 0.000000 },
    { 0.730693, 0.269307, 0.000000 },
    { 0.730896, 0.269104, 0.000000 },
    { 0.731089, 0.268911, 0.000000 },
    { 0.731280, 0.268720, 0.000000 },
    { 0.731467, 0.268533, 0.000000 },
    { 0.731650, 0.268350, 0.000000 },
    { 0.731826, 0.268174, 0.000000 },
    { 0.731993, 0.268007, 0.000000 },
    { 0.732150, 0.267850, 0.000000 },
    { 0.732300, 0.267700, 0.000000 },
    { 0.732443, 0.267557, 0.000000 },
    { 0.732581, 0.267419, 0.000000 },
    { 0.732719, 0.267281, 0.000000 },
    { 0.732859, 0.267141, 0.000000 },
    { 0.733000, 0.267000, 0.000000 },
    { 0.733142, 0.266858, 0.000000 },
    { 0.733281, 0.266719, 0.000000 },
    { 0.733417, 0.266583, 0.000000 },
    { 0.733551, 0.266449, 0.000000 },
    { 0.733683, 0.266317, 0.000000 },
    { 0.733813, 0.266187, 0.000000 },
    { 0.733936, 0.266064, 0.000000 },
    { 0.734047, 0.265953, 0.000000 },
    { 0.734143, 0.265857, 0.000000 },
    { 0.734221, 0.265779, 0.000000 },
    { 0.734286, 0.265714, 0.000000 },
    { 0.734341, 0.265659, 0.000000 },
    { 0.734390, 0.265610, 0.000000 },
    { 0.734438, 0.265562, 0.000000 },
    { 0.734482, 0.265518, 0.000000 },
    { 0.734523, 0.265477, 0.000000 },
    { 0.734560, 0.265440, 0.000000 },
    { 0.734592, 0.265408, 0.000000 },
    { 0.734621, 0.265379, 0.000000 },
    { 0.734649, 0.265351, 0.000000 },
    { 0.734673, 0.265327, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
    { 0.734690, 0.265310, 0.000000 },
};

namespace ImGui
{
/* Standard white point chromaticities. */
#define C     0.310063, 0.316158
#define E     1.0/3.0, 1.0/3.0
#define D50   0.34570, 0.3585
#define D65   0.312713, 0.329016
#define GAMMA_REC709    0.      /* Rec. 709 */

typedef struct _ColorSystem {
    double xRed, yRed,                /* Red primary illuminant */
           xGreen, yGreen,            /* Green primary illuminant */
           xBlue, yBlue,              /* Blue primary illuminant */
           xWhite, yWhite,            /* White point */
           gamma;             /* gamma of nonlinear correction */
} ColorSystem;

static const ColorSystem color_systems[] = {
    /* [NTSCsystem] = */ {
        0.67,  0.33,  0.21,  0.71,  0.14,  0.08,
        C, GAMMA_REC709
    },
    /* [EBUsystem] = */ {
        0.64,  0.33,  0.29,  0.60,  0.15,  0.06,
        D65, GAMMA_REC709
    },
    /* [SMPTEsystem] = */ {
        0.630, 0.340, 0.310, 0.595, 0.155, 0.070,
        D65, GAMMA_REC709
    },
    /* [SMPTE240Msystem] = */ {
        0.670, 0.330, 0.210, 0.710, 0.150, 0.060,
        D65, GAMMA_REC709
    },
    /* [APPLEsystem] = */ {
        0.625, 0.340, 0.280, 0.595, 0.115, 0.070,
        D65, GAMMA_REC709
    },
    /* [wRGBsystem] = */ {
        0.7347, 0.2653, 0.1152, 0.8264, 0.1566, 0.0177,
        D50, GAMMA_REC709
    },
    /* [CIE1931system] = */ {
        0.7347, 0.2653, 0.2738, 0.7174, 0.1666, 0.0089,
        E, GAMMA_REC709
    },
    /* [Rec709system] = */ {
        0.64,  0.33,  0.30,  0.60,  0.15,  0.06,
        D65, GAMMA_REC709
    },
    /* [Rec2020system] = */ {
        0.708,  0.292,  0.170,  0.797,  0.131,  0.046,
        D65, GAMMA_REC709
    },
    /* [DCIP3] = */ {
        0.680,  0.320,  0.265,  0.690,  0.150,  0.060,
        0.314,  0.351, GAMMA_REC709
    },
};

static inline void invert_matrix3x3(ImGui::ImMat& in, ImGui::ImMat& out)
{
    double m00 = in.at<float>(0, 0);
    double m01 = in.at<float>(0, 1);
    double m02 = in.at<float>(0, 2);
    double m10 = in.at<float>(1, 0);
    double m11 = in.at<float>(1, 1);
    double m12 = in.at<float>(1, 2);
    double m20 = in.at<float>(2, 0);
    double m21 = in.at<float>(2, 1);
    double m22 = in.at<float>(2, 2);
    int i, j;
    double det;

    out.at<float>(0, 0) =  (m11 * m22 - m21 * m12);
    out.at<float>(0, 1) = -(m01 * m22 - m21 * m02);
    out.at<float>(0, 2) =  (m01 * m12 - m11 * m02);
    out.at<float>(1, 0) = -(m10 * m22 - m20 * m12);
    out.at<float>(1, 1) =  (m00 * m22 - m20 * m02);
    out.at<float>(1, 2) = -(m00 * m12 - m10 * m02);
    out.at<float>(2, 0) =  (m10 * m21 - m20 * m11);
    out.at<float>(2, 1) = -(m00 * m21 - m20 * m01);
    out.at<float>(2, 2) =  (m00 * m11 - m10 * m01);

    det = m00 * out.at<float>(0, 0) + m10 * out.at<float>(0, 1) + m20 * out.at<float>(0, 2);
    det = 1.0 / det;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++)
            out.at<float>(i, j) *= det;
    }
}

static inline void get_rgb2xyz_matrix(ColorSystem system, ImGui::ImMat& m)
{
    double S[3], X[4], Z[4];
    int i;

    X[0] = system.xRed   / system.yRed;
    X[1] = system.xGreen / system.yGreen;
    X[2] = system.xBlue  / system.yBlue;
    X[3] = system.xWhite / system.yWhite;

    Z[0] = (1 - system.xRed   - system.yRed)   / system.yRed;
    Z[1] = (1 - system.xGreen - system.yGreen) / system.yGreen;
    Z[2] = (1 - system.xBlue  - system.yBlue)  / system.yBlue;
    Z[3] = (1 - system.xWhite - system.yWhite) / system.yWhite;

    for (i = 0; i < 3; i++) {
        m.at<float>(0, i) = X[i];
        m.at<float>(1, i) = 1;
        m.at<float>(2, i) = Z[i];
    }

    invert_matrix3x3(m, m);

    for (i = 0; i < 3; i++)
        S[i] = m.at<float>(i, 0) * X[3] + m.at<float>(i, 1) * 1 + m.at<float>(i, 2) * Z[3];

    for (i = 0; i < 3; i++) {
        m.at<float>(0, i) = S[i] * X[i];
        m.at<float>(1, i) = S[i] * 1;
        m.at<float>(2, i) = S[i] * Z[i];
    }
}

static inline void xy_to_upvp(double xc, double yc, double * const up, double * const vp)
{
/*
    Given 1931 chromaticities x, y, determine 1976 coordinates u', v'
*/
    *up = 4 * xc / (- 2 * xc + 12 * yc + 3);
    *vp = 9 * yc / (- 2 * xc + 12 * yc + 3);
}

static inline void xy_to_uv(double xc, double yc, double * const u, double * const v)
{
/*
    Given 1931 chromaticities x, y, determine 1960 coordinates u, v
*/
    *u = 4 * xc / (- 2 * xc + 12 * yc + 3);
    *v = 6 * yc / (- 2 * xc + 12 * yc + 3);
}

static inline void uv_to_xy(double   const u, double   const v, double * const xc, double * const yc)
{
/*
    Given 1970 coordinates u, v, determine 1931 chromaticities x, y
*/
    *xc = 3 * u / (2 * u - 8 * v + 4);
    *yc = 2 * v / (2 * u - 8 * v + 4);
}

static inline void upvp_to_xy(double   const up, double   const vp, double * const xc, double * const yc)
{
/*
    Given 1976 coordinates u', v', determine 1931 chromaticities x, y
*/
    *xc = 9 * up / (6 * up - 16 * vp + 12);
    *yc = 4 * vp / (6 * up - 16 * vp + 12);
}

static inline void rgb_to_xy(double rc, double gc, double bc,
                            double * const x, double * const y, double * const z,
                            ImGui::ImMat &m)
{
    double sum;

    *x = m.at<float>(0, 0) * rc + m.at<float>(0, 1) * gc + m.at<float>(0, 2) * bc;
    *y = m.at<float>(1, 0) * rc + m.at<float>(1, 1) * gc + m.at<float>(1, 2) * bc;
    *z = m.at<float>(2, 0) * rc + m.at<float>(2, 1) * gc + m.at<float>(2, 2) * bc;

    sum = *x + *y + *z;
    if (sum == 0)
        sum = 1;
    *x = *x / sum;
    *y = *y / sum;
}

static inline void xyz_to_rgb(ImGui::ImMat &m, double xc, double yc, double zc, double * const r, double * const g, double * const b)
{
    *r = m.at<float>(0, 0) * xc + m.at<float>(0, 1) * yc + m.at<float>(0, 2) * zc;
    *g = m.at<float>(1, 0) * xc + m.at<float>(1, 1) * yc + m.at<float>(1, 2) * zc;
    *b = m.at<float>(2, 0) * xc + m.at<float>(2, 1) * yc + m.at<float>(2, 2) * zc;
}

static inline void monochrome_color_location(double waveLength, int w, int h,
                          int cie, int *xP, int *yP)
{
    const int ix = waveLength - 360;
    const double pX = spectral_chromaticity[ix][0];
    const double pY = spectral_chromaticity[ix][1];
    const double pZ = spectral_chromaticity[ix][2];
    const double px = pX / (pX + pY + pZ);
    const double py = pY / (pX + pY + pZ);

    if (cie == LUV) 
    {
        double up, vp;

        xy_to_upvp(px, py, &up, &vp);
        *xP = up * (w - 1);
        *yP = (h - 1) - vp * (h - 1);
    } 
    else if (cie == UCS) 
    {
        double u, v;

        xy_to_uv(px, py, &u, &v);
        *xP = u * (w - 1);
        *yP = (h - 1) - v * (h - 1);
    } 
    else if (cie == XYY) 
    {
        *xP = px * (w - 1);
        *yP = (h - 1) - py * (h - 1);
    } 
    else 
    {
        assert(0);
    }
}

static inline void color_location(double px, double py, float w, float h, int cie, float *xP, float *yP)
{
    if (cie == LUV) 
    {
        double up, vp;

        xy_to_upvp(px, py, &up, &vp);
        *xP = up * (w - 1);
        *yP = (h - 1) - vp * (h - 1);
    } 
    else if (cie == UCS) 
    {
        double u, v;

        xy_to_uv(px, py, &u, &v);
        *xP = u * (w - 1);
        *yP = (h - 1) - v * (h - 1);
    } 
    else if (cie == XYY) 
    {
        *xP = px * (w - 1);
        *yP = (h - 1) - py * (h - 1);
    } 
    else 
    {
        assert(0);
    }
}

static inline void draw_line(uint8_t *const pixels, int linesize,
                            int x0, int y0, int x1, int y1,
                            int w, int h,
                            const uint8_t *const rgbcolor)
{
    int dx  = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy  = std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2, e2;

    for (;;) 
    {
        pixels[y0 * linesize + x0 * 4 + 0] = rgbcolor[0];
        pixels[y0 * linesize + x0 * 4 + 1] = rgbcolor[1];
        pixels[y0 * linesize + x0 * 4 + 2] = rgbcolor[2];
        pixels[y0 * linesize + x0 * 4 + 3] = rgbcolor[3];

        if (x0 == x1 && y0 == y1)
            break;

        e2 = err;

        if (e2 >-dx)
        {
            err -= dy;
            x0 += sx;
        }

        if (e2 < dy)
        {
            err += dx;
            y0 += sy;
        }
    }
}

static inline void tongue_outline(uint8_t* const pixels,
                                int       const linesize,
                                int       const w,
                                int       const h,
                                uint8_t  const maxval,
                                int       const cie)
{
    const uint8_t rgbcolor[4] = { maxval, maxval, maxval, maxval };
    int wavelength;
    int lx, ly;
    int fx, fy;

    for (wavelength = 360; wavelength <= 830; wavelength++) 
    {
        int icx, icy;

        monochrome_color_location(wavelength, w, h, cie,
                                &icx, &icy);

        if (wavelength > 360)
            draw_line(pixels, linesize, lx, ly, icx, icy, w, h, rgbcolor);
        else 
        {
            fx = icx;
            fy = icy;
        }
        lx = icx;
        ly = icy;
    }
    draw_line(pixels, linesize, lx, ly, fx, fy, w, h, rgbcolor);
}

static inline void find_tongue(
    uint8_t*  const pixels,
    int       const w,
    int       const linesize,
    int       const row,
    int *     const presentP,
    int *     const leftEdgeP,
    int *     const rightEdgeP)
{
    int i;

    for (i = 0; i < w && pixels[row * linesize + i * 4 + 0] == 0; i++)
        ;

    if (i >= w) 
    {
        *presentP = 0;
    } 
    else 
    {
        int j;
        int const leftEdge = i;

        *presentP = 1;

        for (j = w - 1; j >= leftEdge && pixels[row * linesize + j * 4 + 0] == 0; j--)
            ;

        *rightEdgeP = j;
        *leftEdgeP = leftEdge;
    }
}

static inline int constrain_rgb(double * const r,
                                double * const g,
                                double * const b)
{
/*----------------------------------------------------------------------------
    If  the  requested RGB shade contains a negative weight for one of
    the primaries, it lies outside the color  gamut  accessible  from
    the  given  triple  of  primaries.  Desaturate it by adding white,
    equal quantities of R, G, and B, enough to make RGB all positive.
-----------------------------------------------------------------------------*/
    double w;

    /* Amount of white needed is w = - min(0, *r, *g, *b) */
    w = (0 < *r) ? 0 : *r;
    w = (w < *g) ? w : *g;
    w = (w < *b) ? w : *b;
    w = - w;

    /* Add just enough white to make r, g, b all positive. */
    if (w > 0) 
    {
        *r += w;  *g += w; *b += w;
        return 1;                     /* Color modified to fit RGB gamut */
    }

    return 0;                         /* Color within RGB gamut */
}

static inline void gamma_correct(const ColorSystem * const cs, double * const c)
{
    double gamma;
    double cc;

    gamma = cs->gamma;

    if (gamma == 0.)
    {
        /* Rec. 709 gamma correction. */
        cc = 0.018;
        if (*c < cc) {
            *c *= (1.099 * pow(cc, 0.45) - 0.099) / cc;
        } else {
            *c = 1.099 * pow(*c, 0.45) - 0.099;
        }
    } 
    else 
    {
    /* Nonlinear color = (Linear color)^(1/gamma) */
        *c = pow(*c, 1. / gamma);
    }
}

static inline void gamma_correct_rgb(
    const ColorSystem * const cs,
    double * const r,
    double * const g,
    double * const b)
{
    gamma_correct(cs, r);
    gamma_correct(cs, g);
    gamma_correct(cs, b);
}

static inline void fill_in_tongue(
    uint8_t*                   const pixels,
    int                        const linesize,
    int                        const w,
    int                        const h,
    uint8_t                    const maxval,
    const ColorSystem *        const cs,
    ImGui::ImMat               &m,
    int                        const cie,
    bool                       const correct_gamma,
    float                      const contrast)
{
    /* 
    Scan the image line by line and  fill  the  tongue  outline
    with the RGB values determined by the color system for the x-y
    co-ordinates within the tongue.
    */

    for (int y = 0; y < h; ++y) 
    {
        int  present;  /* There is some tongue on this line */
        int leftEdge; /* x position of leftmost pixel in tongue on this line */
        int rightEdge; /* same, but rightmost */

        find_tongue(pixels, w, linesize, y, &present, &leftEdge, &rightEdge);

        if (present) 
        {
            for (int x = leftEdge+1; x < rightEdge; ++x) 
            {
                double cx, cy, cz, jr, jg, jb, jmax;
                int r, g, b, mx = maxval;

                if (cie == LUV) 
                {
                    double up, vp;
                    up = ((double) x) / (w - 1);
                    vp = 1.0 - ((double) y) / (h - 1);
                    upvp_to_xy(up, vp, &cx, &cy);
                    cz = 1.0 - (cx + cy);
                } 
                else if (cie == UCS) 
                {
                    double u, v;
                    u = ((double) x) / (w - 1);
                    v = 1.0 - ((double) y) / (h - 1);
                    uv_to_xy(u, v, &cx, &cy);
                    cz = 1.0 - (cx + cy);
                } 
                else if (cie == XYY) 
                {
                    cx = ((double) x) / (w - 1);
                    cy = 1.0 - ((double) y) / (h - 1);
                    cz = 1.0 - (cx + cy);
                } 
                else 
                {
                    assert(0);
                }

                xyz_to_rgb(m, cx, cy, cz, &jr, &jg, &jb);

                // Check whether the requested color  is  within  the
                // gamut  achievable with the given color system.  If
                // not, draw it in a reduced  intensity,  interpolated
                // by desaturation to the closest within-gamut color.

                if (constrain_rgb(&jr, &jg, &jb))
                    mx *= contrast;

                jmax = MAX3(jr, jg, jb);
                if (jmax > 0) 
                {
                    jr = jr / jmax;
                    jg = jg / jmax;
                    jb = jb / jmax;
                }
                // gamma correct from linear rgb to nonlinear rgb.
                if (correct_gamma)
                    gamma_correct_rgb(cs, &jr, &jg, &jb);
                r = mx * jr;
                g = mx * jg;
                b = mx * jb;
                pixels[y * linesize + x * 4 + 0] = r;
                pixels[y * linesize + x * 4 + 1] = g;
                pixels[y * linesize + x * 4 + 2] = b;
                pixels[y * linesize + x * 4 + 3] = 0;
            }
        }
    }
}

static inline void plot_gamuts(uint8_t *pixels, int linesize, int w, int h,
                                int cie, int gamuts, uint8_t const maxval)
{
    const uint8_t rgbcolor[4] = { maxval, maxval, maxval, maxval };
    const ColorSystem *cs = &color_systems[gamuts];
    int rx, ry, gx, gy, bx, by;

    if (cie == LUV) {
        double wup, wvp;
        xy_to_upvp(cs->xRed, cs->yRed, &wup, &wvp);
        rx = (w - 1) * wup;
        ry = (h - 1) - ((int) ((h - 1) * wvp));
        xy_to_upvp(cs->xGreen, cs->yGreen, &wup, &wvp);
        gx = (w - 1) * wup;
        gy = (h - 1) - ((int) ((h - 1) * wvp));
        xy_to_upvp(cs->xBlue, cs->yBlue, &wup, &wvp);
        bx = (w - 1) * wup;
        by = (h - 1) - ((int) ((h - 1) * wvp));
    } else if (cie == UCS) {
        double wu, wv;
        xy_to_uv(cs->xRed, cs->yRed, &wu, &wv);
        rx = (w - 1) * wu;
        ry = (h - 1) - ((int) ((h - 1) * wv));
        xy_to_uv(cs->xGreen, cs->yGreen, &wu, &wv);
        gx = (w - 1) * wu;
        gy = (h - 1) - ((int) ((h - 1) * wv));
        xy_to_uv(cs->xBlue, cs->yBlue, &wu, &wv);
        bx = (w - 1) * wu;
        by = (h - 1) - ((int) ((h - 1) * wv));
    } else if (cie == XYY) {
        rx = (w - 1) * cs->xRed;
        ry = (h - 1) - ((int) ((h - 1) * cs->yRed));
        gx = (w - 1) * cs->xGreen;
        gy = (h - 1) - ((int) ((h - 1) * cs->yGreen));
        bx = (w - 1) * cs->xBlue;
        by = (h - 1) - ((int) ((h - 1) * cs->yBlue));
    } else {
        assert(0);
    }

    draw_line(pixels, linesize, rx, ry, gx, gy, w, h, rgbcolor);
    draw_line(pixels, linesize, gx, gy, bx, by, w, h, rgbcolor);
    draw_line(pixels, linesize, bx, by, rx, ry, w, h, rgbcolor);
}

/////////////////////////////////////////////////////////////////////////////////////////////
CIE_vulkan::CIE_vulkan(int gpu)
{
    xyz_matrix.create(3, 3, 4u, 1, nullptr);
    xyz_imatrix.create(3, 3, 4u, 1, nullptr);
    get_rgb2xyz_matrix(color_systems[color_system], xyz_matrix);
    invert_matrix3x3(xyz_matrix, xyz_imatrix);
    backgroud.create(size, size, 4, 1u, 4);
    draw_backbroud();

    vkdev = get_gpu_device(gpu);
    opt.blob_vkallocator = vkdev->acquire_blob_allocator();
    opt.staging_vkallocator = vkdev->acquire_staging_allocator();
#ifdef VULKAN_SHADER_FP16
    opt.use_fp16_arithmetic = true;
    opt.use_fp16_storage = true;
#endif
    cmd = new VkCompute(vkdev, "CIE");

    std::vector<vk_specialization_type> specializations(0);
    std::vector<uint32_t> spirv_data;
    
    if (compile_spirv_module(CIE_set_data, opt, spirv_data) == 0)
    {
        pipe_set = new Pipeline(vkdev);
        pipe_set->set_optimal_local_size_xyz(8, 8, 1);
        pipe_set->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(CIE_data, opt, spirv_data) == 0)
    {
        pipe = new Pipeline(vkdev);
        pipe->set_optimal_local_size_xyz(8, 8, 1);
        pipe->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    if (compile_spirv_module(CIE_merge_data, opt, spirv_data) == 0)
    {
        pipe_merge = new Pipeline(vkdev);
        pipe_merge->set_optimal_local_size_xyz(8, 8, 1);
        pipe_merge->create(spirv_data.data(), spirv_data.size() * 4, specializations);
        spirv_data.clear();
    }

    VkTransfer tran_xyz_matrix(vkdev);
    tran_xyz_matrix.record_upload(xyz_matrix, xyz_matrix_gpu, opt, false);
    tran_xyz_matrix.submit_and_wait();

    VkTransfer tran_background(vkdev);
    tran_background.record_upload(backgroud, backgroud_gpu, opt, false);
    tran_background.submit_and_wait();

    buffer.create_type(size, size, IM_DT_INT32, opt.blob_vkallocator);
    buffer.color_format = IM_CF_GRAY;

    cmd->reset();
}

CIE_vulkan::~CIE_vulkan()
{
    if (vkdev)
    {
        if (pipe_set) { delete pipe_set; pipe_set = nullptr; }
        if (pipe) { delete pipe; pipe = nullptr; }
        if (pipe_merge) { delete pipe_merge; pipe_merge = nullptr; }
        if (cmd) { delete cmd; cmd = nullptr; }
        if (opt.blob_vkallocator) { vkdev->reclaim_blob_allocator(opt.blob_vkallocator); opt.blob_vkallocator = nullptr; }
        if (opt.staging_vkallocator) { vkdev->reclaim_staging_allocator(opt.staging_vkallocator); opt.staging_vkallocator = nullptr; }
    }
}

void CIE_vulkan::GetWhitePoint(ColorsSystems cs, float w, float h, float* x, float* y)
{
    double px = color_systems[cs].xWhite;
    double py = color_systems[cs].yWhite; 
    color_location(px, py, w, h, cie, x, y);
}

void CIE_vulkan::GetRedPoint(ColorsSystems cs, float w, float h, float* x, float* y)
{
    double px = color_systems[cs].xRed;
    double py = color_systems[cs].xRed; 
    color_location(px, py, w, h, cie, x, y);
}

void CIE_vulkan::GetBluePoint(ColorsSystems cs, float w, float h, float* x, float* y)
{
    double px = color_systems[cs].xBlue;
    double py = color_systems[cs].yBlue; 
    color_location(px, py, w, h, cie, x, y);
}

void CIE_vulkan::GetGreenPoint(ColorsSystems cs, float w, float h, float* x, float* y)
{
    double px = color_systems[cs].xGreen;
    double py = color_systems[cs].yGreen; 
    color_location(px, py, w, h, cie, x, y);
}

void CIE_vulkan::SetParam(int _color_system, int _cie, int _size, int _gamuts, float _contrast, bool _correct_gamma)
{
    if (color_system != _color_system || cie != _cie || size != _size || gamuts != _gamuts || contrast != _contrast || correct_gamma != _correct_gamma)
    {
        cie = _cie;
        size = _size;
        color_system = _color_system;
        gamuts = _gamuts;
        contrast = _contrast;
        correct_gamma = _correct_gamma;
        get_rgb2xyz_matrix(color_systems[color_system], xyz_matrix);
        invert_matrix3x3(xyz_matrix, xyz_imatrix);
        backgroud.create(size, size, 4, 1u, 4);
        draw_backbroud();

        VkTransfer tran_xyz_matrix(vkdev);
        tran_xyz_matrix.record_upload(xyz_matrix, xyz_matrix_gpu, opt, false);
        tran_xyz_matrix.submit_and_wait();

        VkTransfer tran_background(vkdev);
        tran_background.record_upload(backgroud, backgroud_gpu, opt, false);
        tran_background.submit_and_wait();

        buffer.create_type(size, size, IM_DT_INT32, opt.blob_vkallocator);
        buffer.color_format = IM_CF_GRAY;
    }
}

void CIE_vulkan::draw_backbroud()
{
    if (!backgroud.empty())
    {
        for (int y = 0; y < backgroud.h; y++)
        {
            for (int x = 0; x < backgroud.w; x++)
            {
                backgroud.at<uint8_t>(x, y, rgba_map[0]) = 0;
                backgroud.at<uint8_t>(x, y, rgba_map[1]) = 0;
                backgroud.at<uint8_t>(x, y, rgba_map[2]) = 0;
                backgroud.at<uint8_t>(x, y, rgba_map[3]) = 255;
            }
        }
        tongue_outline((uint8_t *)backgroud.data, backgroud.w * 4, size, size, 128, cie);
        fill_in_tongue((uint8_t *)backgroud.data, backgroud.w * 4, size, size, 255, &color_systems[color_system], xyz_imatrix, cie, correct_gamma, contrast);
        plot_gamuts((uint8_t *)backgroud.data, backgroud.w * 4, size, size, cie, color_system, 128);
        if (gamuts != color_system)
            plot_gamuts((uint8_t *)backgroud.data, backgroud.w * 4, size, size, cie, gamuts, 128);
    }
}

void CIE_vulkan::upload_param(const VkMat& src, VkMat& dst, float intensity, bool show_color)
{
    std::vector<VkMat> bindings_set(1);
    bindings_set[0] = buffer;
    std::vector<vk_constant_type> constants_set(2);
    constants_set[0].i = buffer.w;
    constants_set[1].i = buffer.h;
    cmd->record_pipeline(pipe_set, bindings_set, constants_set, buffer);

    std::vector<VkMat> bindings(6);
    if      (src.type == IM_DT_INT8)     bindings[0] = src;
    else if (src.type == IM_DT_INT16 || src.type == IM_DT_INT16_BE)    bindings[1] = src;
    else if (src.type == IM_DT_FLOAT16)  bindings[2] = src;
    else if (src.type == IM_DT_FLOAT32)  bindings[3] = src;
    bindings[4] = buffer;
    bindings[5] = xyz_matrix_gpu;
    std::vector<vk_constant_type> constants(9);
    constants[0].i = src.w;
    constants[1].i = src.h;
    constants[2].i = src.c;
    constants[3].i = src.color_format;
    constants[4].i = src.type;
    constants[5].i = buffer.w;
    constants[6].i = buffer.h;
    constants[7].i = 1;
    constants[8].i = cie;
    cmd->record_pipeline(pipe, bindings, constants, src);

    std::vector<VkMat> bindings_merge(9);
    if      (dst.type == IM_DT_INT8)     bindings_merge[0] = dst;
    else if (dst.type == IM_DT_INT16 || dst.type == IM_DT_INT16_BE)    bindings_merge[1] = dst;
    else if (dst.type == IM_DT_FLOAT16)  bindings_merge[2] = dst;
    else if (dst.type == IM_DT_FLOAT32)  bindings_merge[3] = dst;

    if      (backgroud_gpu.type == IM_DT_INT8)     bindings_merge[4] = backgroud_gpu;
    else if (backgroud_gpu.type == IM_DT_INT16 || backgroud_gpu.type == IM_DT_INT16_BE)    bindings_merge[5] = backgroud_gpu;
    else if (backgroud_gpu.type == IM_DT_FLOAT16)  bindings_merge[6] = backgroud_gpu;
    else if (backgroud_gpu.type == IM_DT_FLOAT32)  bindings_merge[7] = backgroud_gpu;

    bindings_merge[8] = buffer;
    std::vector<vk_constant_type> constants_merge(12);
    constants_merge[0].i = backgroud_gpu.w;
    constants_merge[1].i = backgroud_gpu.h;
    constants_merge[2].i = backgroud_gpu.c;
    constants_merge[3].i = backgroud_gpu.color_format;
    constants_merge[4].i = backgroud_gpu.type;
    constants_merge[5].i = dst.w;
    constants_merge[6].i = dst.h;
    constants_merge[7].i = dst.c;
    constants_merge[8].i = dst.color_format;
    constants_merge[9].i = dst.type;
    constants_merge[10].i = show_color ? 1 : 0;
    constants_merge[11].f = intensity;
    cmd->record_pipeline(pipe_merge, bindings_merge, constants_merge, dst);
}

double CIE_vulkan::scope(const ImMat& src, ImMat& dst, float intensity, bool show_color)
{
    double ret = 0.0;
    if (!vkdev || !pipe_set || !pipe || !pipe_merge || !cmd)
    {
        return ret;
    }

    VkMat dst_gpu;
    dst_gpu.create_type(size, size, 4, IM_DT_INT8, opt.blob_vkallocator);

    VkMat src_gpu;
    if (src.device == IM_DD_VULKAN)
    {
        src_gpu = src;
    }
    else if (src.device == IM_DD_CPU)
    {
        cmd->record_clone(src, src_gpu, opt);
    }

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_start();
#endif

    upload_param(src_gpu, dst_gpu, intensity, show_color);

#ifdef VULKAN_SHADER_BENCHMARK
    cmd->benchmark_end();
#endif

    // download
    if (dst.device == IM_DD_CPU)
        cmd->record_clone(dst_gpu, dst, opt);
    else if (dst.device == IM_DD_VULKAN)
        dst = dst_gpu;
    cmd->submit_and_wait();
#ifdef VULKAN_SHADER_BENCHMARK
    ret = cmd->benchmark();
    //cmd->benchmark_print();
#endif
    cmd->reset();
    dst.copy_attribute(src);
    dst.flags |= IM_MAT_FLAGS_CUSTOM_UPDATED;
    return ret;
}

} // namespace ImGui