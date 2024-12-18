// IDNA  15.1.0

// clang-format off
#ifndef ADA_IDNA_IDENTIFIER_TABLES_H
#define ADA_IDNA_IDENTIFIER_TABLES_H
#include <cstdint>

namespace ada::idna {

const uint32_t id_continue[1344][2] =
{
	{48, 57}, {65, 90}, {95, 95}, {97, 122},
	{170, 170}, {181, 181}, {183, 183}, {186, 186},
	{192, 214}, {216, 246}, {248, 442}, {443, 443},
	{444, 447}, {448, 451}, {452, 659}, {660, 660},
	{661, 687}, {688, 705}, {710, 721}, {736, 740},
	{748, 748}, {750, 750}, {768, 879}, {880, 883},
	{884, 884}, {886, 887}, {890, 890}, {891, 893},
	{895, 895}, {902, 902}, {903, 903}, {904, 906},
	{908, 908}, {910, 929}, {931, 1013}, {1015, 1153},
	{1155, 1159}, {1162, 1327}, {1329, 1366}, {1369, 1369},
	{1376, 1416}, {1425, 1469}, {1471, 1471}, {1473, 1474},
	{1476, 1477}, {1479, 1479}, {1488, 1514}, {1519, 1522},
	{1552, 1562}, {1568, 1599}, {1600, 1600}, {1601, 1610},
	{1611, 1631}, {1632, 1641}, {1646, 1647}, {1648, 1648},
	{1649, 1747}, {1749, 1749}, {1750, 1756}, {1759, 1764},
	{1765, 1766}, {1767, 1768}, {1770, 1773}, {1774, 1775},
	{1776, 1785}, {1786, 1788}, {1791, 1791}, {1808, 1808},
	{1809, 1809}, {1810, 1839}, {1840, 1866}, {1869, 1957},
	{1958, 1968}, {1969, 1969}, {1984, 1993}, {1994, 2026},
	{2027, 2035}, {2036, 2037}, {2042, 2042}, {2045, 2045},
	{2048, 2069}, {2070, 2073}, {2074, 2074}, {2075, 2083},
	{2084, 2084}, {2085, 2087}, {2088, 2088}, {2089, 2093},
	{2112, 2136}, {2137, 2139}, {2144, 2154}, {2160, 2183},
	{2185, 2190}, {2200, 2207}, {2208, 2248}, {2249, 2249},
	{2250, 2273}, {2275, 2306}, {2307, 2307}, {2308, 2361},
	{2362, 2362}, {2363, 2363}, {2364, 2364}, {2365, 2365},
	{2366, 2368}, {2369, 2376}, {2377, 2380}, {2381, 2381},
	{2382, 2383}, {2384, 2384}, {2385, 2391}, {2392, 2401},
	{2402, 2403}, {2406, 2415}, {2417, 2417}, {2418, 2432},
	{2433, 2433}, {2434, 2435}, {2437, 2444}, {2447, 2448},
	{2451, 2472}, {2474, 2480}, {2482, 2482}, {2486, 2489},
	{2492, 2492}, {2493, 2493}, {2494, 2496}, {2497, 2500},
	{2503, 2504}, {2507, 2508}, {2509, 2509}, {2510, 2510},
	{2519, 2519}, {2524, 2525}, {2527, 2529}, {2530, 2531},
	{2534, 2543}, {2544, 2545}, {2556, 2556}, {2558, 2558},
	{2561, 2562}, {2563, 2563}, {2565, 2570}, {2575, 2576},
	{2579, 2600}, {2602, 2608}, {2610, 2611}, {2613, 2614},
	{2616, 2617}, {2620, 2620}, {2622, 2624}, {2625, 2626},
	{2631, 2632}, {2635, 2637}, {2641, 2641}, {2649, 2652},
	{2654, 2654}, {2662, 2671}, {2672, 2673}, {2674, 2676},
	{2677, 2677}, {2689, 2690}, {2691, 2691}, {2693, 2701},
	{2703, 2705}, {2707, 2728}, {2730, 2736}, {2738, 2739},
	{2741, 2745}, {2748, 2748}, {2749, 2749}, {2750, 2752},
	{2753, 2757}, {2759, 2760}, {2761, 2761}, {2763, 2764},
	{2765, 2765}, {2768, 2768}, {2784, 2785}, {2786, 2787},
	{2790, 2799}, {2809, 2809}, {2810, 2815}, {2817, 2817},
	{2818, 2819}, {2821, 2828}, {2831, 2832}, {2835, 2856},
	{2858, 2864}, {2866, 2867}, {2869, 2873}, {2876, 2876},
	{2877, 2877}, {2878, 2878}, {2879, 2879}, {2880, 2880},
	{2881, 2884}, {2887, 2888}, {2891, 2892}, {2893, 2893},
	{2901, 2902}, {2903, 2903}, {2908, 2909}, {2911, 2913},
	{2914, 2915}, {2918, 2927}, {2929, 2929}, {2946, 2946},
	{2947, 2947}, {2949, 2954}, {2958, 2960}, {2962, 2965},
	{2969, 2970}, {2972, 2972}, {2974, 2975}, {2979, 2980},
	{2984, 2986}, {2990, 3001}, {3006, 3007}, {3008, 3008},
	{3009, 3010}, {3014, 3016}, {3018, 3020}, {3021, 3021},
	{3024, 3024}, {3031, 3031}, {3046, 3055}, {3072, 3072},
	{3073, 3075}, {3076, 3076}, {3077, 3084}, {3086, 3088},
	{3090, 3112}, {3114, 3129}, {3132, 3132}, {3133, 3133},
	{3134, 3136}, {3137, 3140}, {3142, 3144}, {3146, 3149},
	{3157, 3158}, {3160, 3162}, {3165, 3165}, {3168, 3169},
	{3170, 3171}, {3174, 3183}, {3200, 3200}, {3201, 3201},
	{3202, 3203}, {3205, 3212}, {3214, 3216}, {3218, 3240},
	{3242, 3251}, {3253, 3257}, {3260, 3260}, {3261, 3261},
	{3262, 3262}, {3263, 3263}, {3264, 3268}, {3270, 3270},
	{3271, 3272}, {3274, 3275}, {3276, 3277}, {3285, 3286},
	{3293, 3294}, {3296, 3297}, {3298, 3299}, {3302, 3311},
	{3313, 3314}, {3315, 3315}, {3328, 3329}, {3330, 3331},
	{3332, 3340}, {3342, 3344}, {3346, 3386}, {3387, 3388},
	{3389, 3389}, {3390, 3392}, {3393, 3396}, {3398, 3400},
	{3402, 3404}, {3405, 3405}, {3406, 3406}, {3412, 3414},
	{3415, 3415}, {3423, 3425}, {3426, 3427}, {3430, 3439},
	{3450, 3455}, {3457, 3457}, {3458, 3459}, {3461, 3478},
	{3482, 3505}, {3507, 3515}, {3517, 3517}, {3520, 3526},
	{3530, 3530}, {3535, 3537}, {3538, 3540}, {3542, 3542},
	{3544, 3551}, {3558, 3567}, {3570, 3571}, {3585, 3632},
	{3633, 3633}, {3634, 3635}, {3636, 3642}, {3648, 3653},
	{3654, 3654}, {3655, 3662}, {3664, 3673}, {3713, 3714},
	{3716, 3716}, {3718, 3722}, {3724, 3747}, {3749, 3749},
	{3751, 3760}, {3761, 3761}, {3762, 3763}, {3764, 3772},
	{3773, 3773}, {3776, 3780}, {3782, 3782}, {3784, 3790},
	{3792, 3801}, {3804, 3807}, {3840, 3840}, {3864, 3865},
	{3872, 3881}, {3893, 3893}, {3895, 3895}, {3897, 3897},
	{3902, 3903}, {3904, 3911}, {3913, 3948}, {3953, 3966},
	{3967, 3967}, {3968, 3972}, {3974, 3975}, {3976, 3980},
	{3981, 3991}, {3993, 4028}, {4038, 4038}, {4096, 4138},
	{4139, 4140}, {4141, 4144}, {4145, 4145}, {4146, 4151},
	{4152, 4152}, {4153, 4154}, {4155, 4156}, {4157, 4158},
	{4159, 4159}, {4160, 4169}, {4176, 4181}, {4182, 4183},
	{4184, 4185}, {4186, 4189}, {4190, 4192}, {4193, 4193},
	{4194, 4196}, {4197, 4198}, {4199, 4205}, {4206, 4208},
	{4209, 4212}, {4213, 4225}, {4226, 4226}, {4227, 4228},
	{4229, 4230}, {4231, 4236}, {4237, 4237}, {4238, 4238},
	{4239, 4239}, {4240, 4249}, {4250, 4252}, {4253, 4253},
	{4256, 4293}, {4295, 4295}, {4301, 4301}, {4304, 4346},
	{4348, 4348}, {4349, 4351}, {4352, 4680}, {4682, 4685},
	{4688, 4694}, {4696, 4696}, {4698, 4701}, {4704, 4744},
	{4746, 4749}, {4752, 4784}, {4786, 4789}, {4792, 4798},
	{4800, 4800}, {4802, 4805}, {4808, 4822}, {4824, 4880},
	{4882, 4885}, {4888, 4954}, {4957, 4959}, {4969, 4977},
	{4992, 5007}, {5024, 5109}, {5112, 5117}, {5121, 5740},
	{5743, 5759}, {5761, 5786}, {5792, 5866}, {5870, 5872},
	{5873, 5880}, {5888, 5905}, {5906, 5908}, {5909, 5909},
	{5919, 5937}, {5938, 5939}, {5940, 5940}, {5952, 5969},
	{5970, 5971}, {5984, 5996}, {5998, 6000}, {6002, 6003},
	{6016, 6067}, {6068, 6069}, {6070, 6070}, {6071, 6077},
	{6078, 6085}, {6086, 6086}, {6087, 6088}, {6089, 6099},
	{6103, 6103}, {6108, 6108}, {6109, 6109}, {6112, 6121},
	{6155, 6157}, {6159, 6159}, {6160, 6169}, {6176, 6210},
	{6211, 6211}, {6212, 6264}, {6272, 6276}, {6277, 6278},
	{6279, 6312}, {6313, 6313}, {6314, 6314}, {6320, 6389},
	{6400, 6430}, {6432, 6434}, {6435, 6438}, {6439, 6440},
	{6441, 6443}, {6448, 6449}, {6450, 6450}, {6451, 6456},
	{6457, 6459}, {6470, 6479}, {6480, 6509}, {6512, 6516},
	{6528, 6571}, {6576, 6601}, {6608, 6617}, {6618, 6618},
	{6656, 6678}, {6679, 6680}, {6681, 6682}, {6683, 6683},
	{6688, 6740}, {6741, 6741}, {6742, 6742}, {6743, 6743},
	{6744, 6750}, {6752, 6752}, {6753, 6753}, {6754, 6754},
	{6755, 6756}, {6757, 6764}, {6765, 6770}, {6771, 6780},
	{6783, 6783}, {6784, 6793}, {6800, 6809}, {6823, 6823},
	{6832, 6845}, {6847, 6862}, {6912, 6915}, {6916, 6916},
	{6917, 6963}, {6964, 6964}, {6965, 6965}, {6966, 6970},
	{6971, 6971}, {6972, 6972}, {6973, 6977}, {6978, 6978},
	{6979, 6980}, {6981, 6988}, {6992, 7001}, {7019, 7027},
	{7040, 7041}, {7042, 7042}, {7043, 7072}, {7073, 7073},
	{7074, 7077}, {7078, 7079}, {7080, 7081}, {7082, 7082},
	{7083, 7085}, {7086, 7087}, {7088, 7097}, {7098, 7141},
	{7142, 7142}, {7143, 7143}, {7144, 7145}, {7146, 7148},
	{7149, 7149}, {7150, 7150}, {7151, 7153}, {7154, 7155},
	{7168, 7203}, {7204, 7211}, {7212, 7219}, {7220, 7221},
	{7222, 7223}, {7232, 7241}, {7245, 7247}, {7248, 7257},
	{7258, 7287}, {7288, 7293}, {7296, 7304}, {7312, 7354},
	{7357, 7359}, {7376, 7378}, {7380, 7392}, {7393, 7393},
	{7394, 7400}, {7401, 7404}, {7405, 7405}, {7406, 7411},
	{7412, 7412}, {7413, 7414}, {7415, 7415}, {7416, 7417},
	{7418, 7418}, {7424, 7467}, {7468, 7530}, {7531, 7543},
	{7544, 7544}, {7545, 7578}, {7579, 7615}, {7616, 7679},
	{7680, 7957}, {7960, 7965}, {7968, 8005}, {8008, 8013},
	{8016, 8023}, {8025, 8025}, {8027, 8027}, {8029, 8029},
	{8031, 8061}, {8064, 8116}, {8118, 8124}, {8126, 8126},
	{8130, 8132}, {8134, 8140}, {8144, 8147}, {8150, 8155},
	{8160, 8172}, {8178, 8180}, {8182, 8188}, {8204, 8205},
	{8255, 8256}, {8276, 8276}, {8305, 8305}, {8319, 8319},
	{8336, 8348}, {8400, 8412}, {8417, 8417}, {8421, 8432},
	{8450, 8450}, {8455, 8455}, {8458, 8467}, {8469, 8469},
	{8472, 8472}, {8473, 8477}, {8484, 8484}, {8486, 8486},
	{8488, 8488}, {8490, 8493}, {8494, 8494}, {8495, 8500},
	{8501, 8504}, {8505, 8505}, {8508, 8511}, {8517, 8521},
	{8526, 8526}, {8544, 8578}, {8579, 8580}, {8581, 8584},
	{11264, 11387}, {11388, 11389}, {11390, 11492}, {11499, 11502},
	{11503, 11505}, {11506, 11507}, {11520, 11557}, {11559, 11559},
	{11565, 11565}, {11568, 11623}, {11631, 11631}, {11647, 11647},
	{11648, 11670}, {11680, 11686}, {11688, 11694}, {11696, 11702},
	{11704, 11710}, {11712, 11718}, {11720, 11726}, {11728, 11734},
	{11736, 11742}, {11744, 11775}, {12293, 12293}, {12294, 12294},
	{12295, 12295}, {12321, 12329}, {12330, 12333}, {12334, 12335},
	{12337, 12341}, {12344, 12346}, {12347, 12347}, {12348, 12348},
	{12353, 12438}, {12441, 12442}, {12443, 12444}, {12445, 12446},
	{12447, 12447}, {12449, 12538}, {12539, 12539}, {12540, 12542},
	{12543, 12543}, {12549, 12591}, {12593, 12686}, {12704, 12735},
	{12784, 12799}, {13312, 19903}, {19968, 40980}, {40981, 40981},
	{40982, 42124}, {42192, 42231}, {42232, 42237}, {42240, 42507},
	{42508, 42508}, {42512, 42527}, {42528, 42537}, {42538, 42539},
	{42560, 42605}, {42606, 42606}, {42607, 42607}, {42612, 42621},
	{42623, 42623}, {42624, 42651}, {42652, 42653}, {42654, 42655},
	{42656, 42725}, {42726, 42735}, {42736, 42737}, {42775, 42783},
	{42786, 42863}, {42864, 42864}, {42865, 42887}, {42888, 42888},
	{42891, 42894}, {42895, 42895}, {42896, 42954}, {42960, 42961},
	{42963, 42963}, {42965, 42969}, {42994, 42996}, {42997, 42998},
	{42999, 42999}, {43000, 43001}, {43002, 43002}, {43003, 43009},
	{43010, 43010}, {43011, 43013}, {43014, 43014}, {43015, 43018},
	{43019, 43019}, {43020, 43042}, {43043, 43044}, {43045, 43046},
	{43047, 43047}, {43052, 43052}, {43072, 43123}, {43136, 43137},
	{43138, 43187}, {43188, 43203}, {43204, 43205}, {43216, 43225},
	{43232, 43249}, {43250, 43255}, {43259, 43259}, {43261, 43262},
	{43263, 43263}, {43264, 43273}, {43274, 43301}, {43302, 43309},
	{43312, 43334}, {43335, 43345}, {43346, 43347}, {43360, 43388},
	{43392, 43394}, {43395, 43395}, {43396, 43442}, {43443, 43443},
	{43444, 43445}, {43446, 43449}, {43450, 43451}, {43452, 43453},
	{43454, 43456}, {43471, 43471}, {43472, 43481}, {43488, 43492},
	{43493, 43493}, {43494, 43494}, {43495, 43503}, {43504, 43513},
	{43514, 43518}, {43520, 43560}, {43561, 43566}, {43567, 43568},
	{43569, 43570}, {43571, 43572}, {43573, 43574}, {43584, 43586},
	{43587, 43587}, {43588, 43595}, {43596, 43596}, {43597, 43597},
	{43600, 43609}, {43616, 43631}, {43632, 43632}, {43633, 43638},
	{43642, 43642}, {43643, 43643}, {43644, 43644}, {43645, 43645},
	{43646, 43695}, {43696, 43696}, {43697, 43697}, {43698, 43700},
	{43701, 43702}, {43703, 43704}, {43705, 43709}, {43710, 43711},
	{43712, 43712}, {43713, 43713}, {43714, 43714}, {43739, 43740},
	{43741, 43741}, {43744, 43754}, {43755, 43755}, {43756, 43757},
	{43758, 43759}, {43762, 43762}, {43763, 43764}, {43765, 43765},
	{43766, 43766}, {43777, 43782}, {43785, 43790}, {43793, 43798},
	{43808, 43814}, {43816, 43822}, {43824, 43866}, {43868, 43871},
	{43872, 43880}, {43881, 43881}, {43888, 43967}, {43968, 44002},
	{44003, 44004}, {44005, 44005}, {44006, 44007}, {44008, 44008},
	{44009, 44010}, {44012, 44012}, {44013, 44013}, {44016, 44025},
	{44032, 55203}, {55216, 55238}, {55243, 55291}, {63744, 64109},
	{64112, 64217}, {64256, 64262}, {64275, 64279}, {64285, 64285},
	{64286, 64286}, {64287, 64296}, {64298, 64310}, {64312, 64316},
	{64318, 64318}, {64320, 64321}, {64323, 64324}, {64326, 64433},
	{64467, 64829}, {64848, 64911}, {64914, 64967}, {65008, 65019},
	{65024, 65039}, {65056, 65071}, {65075, 65076}, {65101, 65103},
	{65136, 65140}, {65142, 65276}, {65296, 65305}, {65313, 65338},
	{65343, 65343}, {65345, 65370}, {65381, 65381}, {65382, 65391},
	{65392, 65392}, {65393, 65437}, {65438, 65439}, {65440, 65470},
	{65474, 65479}, {65482, 65487}, {65490, 65495}, {65498, 65500},
	{65536, 65547}, {65549, 65574}, {65576, 65594}, {65596, 65597},
	{65599, 65613}, {65616, 65629}, {65664, 65786}, {65856, 65908},
	{66045, 66045}, {66176, 66204}, {66208, 66256}, {66272, 66272},
	{66304, 66335}, {66349, 66368}, {66369, 66369}, {66370, 66377},
	{66378, 66378}, {66384, 66421}, {66422, 66426}, {66432, 66461},
	{66464, 66499}, {66504, 66511}, {66513, 66517}, {66560, 66639},
	{66640, 66717}, {66720, 66729}, {66736, 66771}, {66776, 66811},
	{66816, 66855}, {66864, 66915}, {66928, 66938}, {66940, 66954},
	{66956, 66962}, {66964, 66965}, {66967, 66977}, {66979, 66993},
	{66995, 67001}, {67003, 67004}, {67072, 67382}, {67392, 67413},
	{67424, 67431}, {67456, 67461}, {67463, 67504}, {67506, 67514},
	{67584, 67589}, {67592, 67592}, {67594, 67637}, {67639, 67640},
	{67644, 67644}, {67647, 67669}, {67680, 67702}, {67712, 67742},
	{67808, 67826}, {67828, 67829}, {67840, 67861}, {67872, 67897},
	{67968, 68023}, {68030, 68031}, {68096, 68096}, {68097, 68099},
	{68101, 68102}, {68108, 68111}, {68112, 68115}, {68117, 68119},
	{68121, 68149}, {68152, 68154}, {68159, 68159}, {68192, 68220},
	{68224, 68252}, {68288, 68295}, {68297, 68324}, {68325, 68326},
	{68352, 68405}, {68416, 68437}, {68448, 68466}, {68480, 68497},
	{68608, 68680}, {68736, 68786}, {68800, 68850}, {68864, 68899},
	{68900, 68903}, {68912, 68921}, {69248, 69289}, {69291, 69292},
	{69296, 69297}, {69373, 69375}, {69376, 69404}, {69415, 69415},
	{69424, 69445}, {69446, 69456}, {69488, 69505}, {69506, 69509},
	{69552, 69572}, {69600, 69622}, {69632, 69632}, {69633, 69633},
	{69634, 69634}, {69635, 69687}, {69688, 69702}, {69734, 69743},
	{69744, 69744}, {69745, 69746}, {69747, 69748}, {69749, 69749},
	{69759, 69761}, {69762, 69762}, {69763, 69807}, {69808, 69810},
	{69811, 69814}, {69815, 69816}, {69817, 69818}, {69826, 69826},
	{69840, 69864}, {69872, 69881}, {69888, 69890}, {69891, 69926},
	{69927, 69931}, {69932, 69932}, {69933, 69940}, {69942, 69951},
	{69956, 69956}, {69957, 69958}, {69959, 69959}, {69968, 70002},
	{70003, 70003}, {70006, 70006}, {70016, 70017}, {70018, 70018},
	{70019, 70066}, {70067, 70069}, {70070, 70078}, {70079, 70080},
	{70081, 70084}, {70089, 70092}, {70094, 70094}, {70095, 70095},
	{70096, 70105}, {70106, 70106}, {70108, 70108}, {70144, 70161},
	{70163, 70187}, {70188, 70190}, {70191, 70193}, {70194, 70195},
	{70196, 70196}, {70197, 70197}, {70198, 70199}, {70206, 70206},
	{70207, 70208}, {70209, 70209}, {70272, 70278}, {70280, 70280},
	{70282, 70285}, {70287, 70301}, {70303, 70312}, {70320, 70366},
	{70367, 70367}, {70368, 70370}, {70371, 70378}, {70384, 70393},
	{70400, 70401}, {70402, 70403}, {70405, 70412}, {70415, 70416},
	{70419, 70440}, {70442, 70448}, {70450, 70451}, {70453, 70457},
	{70459, 70460}, {70461, 70461}, {70462, 70463}, {70464, 70464},
	{70465, 70468}, {70471, 70472}, {70475, 70477}, {70480, 70480},
	{70487, 70487}, {70493, 70497}, {70498, 70499}, {70502, 70508},
	{70512, 70516}, {70656, 70708}, {70709, 70711}, {70712, 70719},
	{70720, 70721}, {70722, 70724}, {70725, 70725}, {70726, 70726},
	{70727, 70730}, {70736, 70745}, {70750, 70750}, {70751, 70753},
	{70784, 70831}, {70832, 70834}, {70835, 70840}, {70841, 70841},
	{70842, 70842}, {70843, 70846}, {70847, 70848}, {70849, 70849},
	{70850, 70851}, {70852, 70853}, {70855, 70855}, {70864, 70873},
	{71040, 71086}, {71087, 71089}, {71090, 71093}, {71096, 71099},
	{71100, 71101}, {71102, 71102}, {71103, 71104}, {71128, 71131},
	{71132, 71133}, {71168, 71215}, {71216, 71218}, {71219, 71226},
	{71227, 71228}, {71229, 71229}, {71230, 71230}, {71231, 71232},
	{71236, 71236}, {71248, 71257}, {71296, 71338}, {71339, 71339},
	{71340, 71340}, {71341, 71341}, {71342, 71343}, {71344, 71349},
	{71350, 71350}, {71351, 71351}, {71352, 71352}, {71360, 71369},
	{71424, 71450}, {71453, 71455}, {71456, 71457}, {71458, 71461},
	{71462, 71462}, {71463, 71467}, {71472, 71481}, {71488, 71494},
	{71680, 71723}, {71724, 71726}, {71727, 71735}, {71736, 71736},
	{71737, 71738}, {71840, 71903}, {71904, 71913}, {71935, 71942},
	{71945, 71945}, {71948, 71955}, {71957, 71958}, {71960, 71983},
	{71984, 71989}, {71991, 71992}, {71995, 71996}, {71997, 71997},
	{71998, 71998}, {71999, 71999}, {72000, 72000}, {72001, 72001},
	{72002, 72002}, {72003, 72003}, {72016, 72025}, {72096, 72103},
	{72106, 72144}, {72145, 72147}, {72148, 72151}, {72154, 72155},
	{72156, 72159}, {72160, 72160}, {72161, 72161}, {72163, 72163},
	{72164, 72164}, {72192, 72192}, {72193, 72202}, {72203, 72242},
	{72243, 72248}, {72249, 72249}, {72250, 72250}, {72251, 72254},
	{72263, 72263}, {72272, 72272}, {72273, 72278}, {72279, 72280},
	{72281, 72283}, {72284, 72329}, {72330, 72342}, {72343, 72343},
	{72344, 72345}, {72349, 72349}, {72368, 72440}, {72704, 72712},
	{72714, 72750}, {72751, 72751}, {72752, 72758}, {72760, 72765},
	{72766, 72766}, {72767, 72767}, {72768, 72768}, {72784, 72793},
	{72818, 72847}, {72850, 72871}, {72873, 72873}, {72874, 72880},
	{72881, 72881}, {72882, 72883}, {72884, 72884}, {72885, 72886},
	{72960, 72966}, {72968, 72969}, {72971, 73008}, {73009, 73014},
	{73018, 73018}, {73020, 73021}, {73023, 73029}, {73030, 73030},
	{73031, 73031}, {73040, 73049}, {73056, 73061}, {73063, 73064},
	{73066, 73097}, {73098, 73102}, {73104, 73105}, {73107, 73108},
	{73109, 73109}, {73110, 73110}, {73111, 73111}, {73112, 73112},
	{73120, 73129}, {73440, 73458}, {73459, 73460}, {73461, 73462},
	{73472, 73473}, {73474, 73474}, {73475, 73475}, {73476, 73488},
	{73490, 73523}, {73524, 73525}, {73526, 73530}, {73534, 73535},
	{73536, 73536}, {73537, 73537}, {73538, 73538}, {73552, 73561},
	{73648, 73648}, {73728, 74649}, {74752, 74862}, {74880, 75075},
	{77712, 77808}, {77824, 78895}, {78912, 78912}, {78913, 78918},
	{78919, 78933}, {82944, 83526}, {92160, 92728}, {92736, 92766},
	{92768, 92777}, {92784, 92862}, {92864, 92873}, {92880, 92909},
	{92912, 92916}, {92928, 92975}, {92976, 92982}, {92992, 92995},
	{93008, 93017}, {93027, 93047}, {93053, 93071}, {93760, 93823},
	{93952, 94026}, {94031, 94031}, {94032, 94032}, {94033, 94087},
	{94095, 94098}, {94099, 94111}, {94176, 94177}, {94179, 94179},
	{94180, 94180}, {94192, 94193}, {94208, 100343}, {100352, 101589},
	{101632, 101640}, {110576, 110579}, {110581, 110587}, {110589, 110590},
	{110592, 110882}, {110898, 110898}, {110928, 110930}, {110933, 110933},
	{110948, 110951}, {110960, 111355}, {113664, 113770}, {113776, 113788},
	{113792, 113800}, {113808, 113817}, {113821, 113822}, {118528, 118573},
	{118576, 118598}, {119141, 119142}, {119143, 119145}, {119149, 119154},
	{119163, 119170}, {119173, 119179}, {119210, 119213}, {119362, 119364},
	{119808, 119892}, {119894, 119964}, {119966, 119967}, {119970, 119970},
	{119973, 119974}, {119977, 119980}, {119982, 119993}, {119995, 119995},
	{119997, 120003}, {120005, 120069}, {120071, 120074}, {120077, 120084},
	{120086, 120092}, {120094, 120121}, {120123, 120126}, {120128, 120132},
	{120134, 120134}, {120138, 120144}, {120146, 120485}, {120488, 120512},
	{120514, 120538}, {120540, 120570}, {120572, 120596}, {120598, 120628},
	{120630, 120654}, {120656, 120686}, {120688, 120712}, {120714, 120744},
	{120746, 120770}, {120772, 120779}, {120782, 120831}, {121344, 121398},
	{121403, 121452}, {121461, 121461}, {121476, 121476}, {121499, 121503},
	{121505, 121519}, {122624, 122633}, {122634, 122634}, {122635, 122654},
	{122661, 122666}, {122880, 122886}, {122888, 122904}, {122907, 122913},
	{122915, 122916}, {122918, 122922}, {122928, 122989}, {123023, 123023},
	{123136, 123180}, {123184, 123190}, {123191, 123197}, {123200, 123209},
	{123214, 123214}, {123536, 123565}, {123566, 123566}, {123584, 123627},
	{123628, 123631}, {123632, 123641}, {124112, 124138}, {124139, 124139},
	{124140, 124143}, {124144, 124153}, {124896, 124902}, {124904, 124907},
	{124909, 124910}, {124912, 124926}, {124928, 125124}, {125136, 125142},
	{125184, 125251}, {125252, 125258}, {125259, 125259}, {125264, 125273},
	{126464, 126467}, {126469, 126495}, {126497, 126498}, {126500, 126500},
	{126503, 126503}, {126505, 126514}, {126516, 126519}, {126521, 126521},
	{126523, 126523}, {126530, 126530}, {126535, 126535}, {126537, 126537},
	{126539, 126539}, {126541, 126543}, {126545, 126546}, {126548, 126548},
	{126551, 126551}, {126553, 126553}, {126555, 126555}, {126557, 126557},
	{126559, 126559}, {126561, 126562}, {126564, 126564}, {126567, 126570},
	{126572, 126578}, {126580, 126583}, {126585, 126588}, {126590, 126590},
	{126592, 126601}, {126603, 126619}, {126625, 126627}, {126629, 126633},
	{126635, 126651}, {130032, 130041}, {131072, 173791}, {173824, 177977},
	{177984, 178205}, {178208, 183969}, {183984, 191456}, {191472, 192093},
	{194560, 195101}, {196608, 201546}, {201552, 205743}, {917760, 917999}
};
const uint32_t id_start[740][2] =
{
	{65, 90}, {97, 122}, {170, 170}, {181, 181},
	{186, 186}, {192, 214}, {216, 246}, {248, 442},
	{443, 443}, {444, 447}, {448, 451}, {452, 659},
	{660, 660}, {661, 687}, {688, 705}, {710, 721},
	{736, 740}, {748, 748}, {750, 750}, {880, 883},
	{884, 884}, {886, 887}, {890, 890}, {891, 893},
	{895, 895}, {902, 902}, {904, 906}, {908, 908},
	{910, 929}, {931, 1013}, {1015, 1153}, {1162, 1327},
	{1329, 1366}, {1369, 1369}, {1376, 1416}, {1488, 1514},
	{1519, 1522}, {1568, 1599}, {1600, 1600}, {1601, 1610},
	{1646, 1647}, {1649, 1747}, {1749, 1749}, {1765, 1766},
	{1774, 1775}, {1786, 1788}, {1791, 1791}, {1808, 1808},
	{1810, 1839}, {1869, 1957}, {1969, 1969}, {1994, 2026},
	{2036, 2037}, {2042, 2042}, {2048, 2069}, {2074, 2074},
	{2084, 2084}, {2088, 2088}, {2112, 2136}, {2144, 2154},
	{2160, 2183}, {2185, 2190}, {2208, 2248}, {2249, 2249},
	{2308, 2361}, {2365, 2365}, {2384, 2384}, {2392, 2401},
	{2417, 2417}, {2418, 2432}, {2437, 2444}, {2447, 2448},
	{2451, 2472}, {2474, 2480}, {2482, 2482}, {2486, 2489},
	{2493, 2493}, {2510, 2510}, {2524, 2525}, {2527, 2529},
	{2544, 2545}, {2556, 2556}, {2565, 2570}, {2575, 2576},
	{2579, 2600}, {2602, 2608}, {2610, 2611}, {2613, 2614},
	{2616, 2617}, {2649, 2652}, {2654, 2654}, {2674, 2676},
	{2693, 2701}, {2703, 2705}, {2707, 2728}, {2730, 2736},
	{2738, 2739}, {2741, 2745}, {2749, 2749}, {2768, 2768},
	{2784, 2785}, {2809, 2809}, {2821, 2828}, {2831, 2832},
	{2835, 2856}, {2858, 2864}, {2866, 2867}, {2869, 2873},
	{2877, 2877}, {2908, 2909}, {2911, 2913}, {2929, 2929},
	{2947, 2947}, {2949, 2954}, {2958, 2960}, {2962, 2965},
	{2969, 2970}, {2972, 2972}, {2974, 2975}, {2979, 2980},
	{2984, 2986}, {2990, 3001}, {3024, 3024}, {3077, 3084},
	{3086, 3088}, {3090, 3112}, {3114, 3129}, {3133, 3133},
	{3160, 3162}, {3165, 3165}, {3168, 3169}, {3200, 3200},
	{3205, 3212}, {3214, 3216}, {3218, 3240}, {3242, 3251},
	{3253, 3257}, {3261, 3261}, {3293, 3294}, {3296, 3297},
	{3313, 3314}, {3332, 3340}, {3342, 3344}, {3346, 3386},
	{3389, 3389}, {3406, 3406}, {3412, 3414}, {3423, 3425},
	{3450, 3455}, {3461, 3478}, {3482, 3505}, {3507, 3515},
	{3517, 3517}, {3520, 3526}, {3585, 3632}, {3634, 3635},
	{3648, 3653}, {3654, 3654}, {3713, 3714}, {3716, 3716},
	{3718, 3722}, {3724, 3747}, {3749, 3749}, {3751, 3760},
	{3762, 3763}, {3773, 3773}, {3776, 3780}, {3782, 3782},
	{3804, 3807}, {3840, 3840}, {3904, 3911}, {3913, 3948},
	{3976, 3980}, {4096, 4138}, {4159, 4159}, {4176, 4181},
	{4186, 4189}, {4193, 4193}, {4197, 4198}, {4206, 4208},
	{4213, 4225}, {4238, 4238}, {4256, 4293}, {4295, 4295},
	{4301, 4301}, {4304, 4346}, {4348, 4348}, {4349, 4351},
	{4352, 4680}, {4682, 4685}, {4688, 4694}, {4696, 4696},
	{4698, 4701}, {4704, 4744}, {4746, 4749}, {4752, 4784},
	{4786, 4789}, {4792, 4798}, {4800, 4800}, {4802, 4805},
	{4808, 4822}, {4824, 4880}, {4882, 4885}, {4888, 4954},
	{4992, 5007}, {5024, 5109}, {5112, 5117}, {5121, 5740},
	{5743, 5759}, {5761, 5786}, {5792, 5866}, {5870, 5872},
	{5873, 5880}, {5888, 5905}, {5919, 5937}, {5952, 5969},
	{5984, 5996}, {5998, 6000}, {6016, 6067}, {6103, 6103},
	{6108, 6108}, {6176, 6210}, {6211, 6211}, {6212, 6264},
	{6272, 6276}, {6277, 6278}, {6279, 6312}, {6314, 6314},
	{6320, 6389}, {6400, 6430}, {6480, 6509}, {6512, 6516},
	{6528, 6571}, {6576, 6601}, {6656, 6678}, {6688, 6740},
	{6823, 6823}, {6917, 6963}, {6981, 6988}, {7043, 7072},
	{7086, 7087}, {7098, 7141}, {7168, 7203}, {7245, 7247},
	{7258, 7287}, {7288, 7293}, {7296, 7304}, {7312, 7354},
	{7357, 7359}, {7401, 7404}, {7406, 7411}, {7413, 7414},
	{7418, 7418}, {7424, 7467}, {7468, 7530}, {7531, 7543},
	{7544, 7544}, {7545, 7578}, {7579, 7615}, {7680, 7957},
	{7960, 7965}, {7968, 8005}, {8008, 8013}, {8016, 8023},
	{8025, 8025}, {8027, 8027}, {8029, 8029}, {8031, 8061},
	{8064, 8116}, {8118, 8124}, {8126, 8126}, {8130, 8132},
	{8134, 8140}, {8144, 8147}, {8150, 8155}, {8160, 8172},
	{8178, 8180}, {8182, 8188}, {8305, 8305}, {8319, 8319},
	{8336, 8348}, {8450, 8450}, {8455, 8455}, {8458, 8467},
	{8469, 8469}, {8472, 8472}, {8473, 8477}, {8484, 8484},
	{8486, 8486}, {8488, 8488}, {8490, 8493}, {8494, 8494},
	{8495, 8500}, {8501, 8504}, {8505, 8505}, {8508, 8511},
	{8517, 8521}, {8526, 8526}, {8544, 8578}, {8579, 8580},
	{8581, 8584}, {11264, 11387}, {11388, 11389}, {11390, 11492},
	{11499, 11502}, {11506, 11507}, {11520, 11557}, {11559, 11559},
	{11565, 11565}, {11568, 11623}, {11631, 11631}, {11648, 11670},
	{11680, 11686}, {11688, 11694}, {11696, 11702}, {11704, 11710},
	{11712, 11718}, {11720, 11726}, {11728, 11734}, {11736, 11742},
	{12293, 12293}, {12294, 12294}, {12295, 12295}, {12321, 12329},
	{12337, 12341}, {12344, 12346}, {12347, 12347}, {12348, 12348},
	{12353, 12438}, {12443, 12444}, {12445, 12446}, {12447, 12447},
	{12449, 12538}, {12540, 12542}, {12543, 12543}, {12549, 12591},
	{12593, 12686}, {12704, 12735}, {12784, 12799}, {13312, 19903},
	{19968, 40980}, {40981, 40981}, {40982, 42124}, {42192, 42231},
	{42232, 42237}, {42240, 42507}, {42508, 42508}, {42512, 42527},
	{42538, 42539}, {42560, 42605}, {42606, 42606}, {42623, 42623},
	{42624, 42651}, {42652, 42653}, {42656, 42725}, {42726, 42735},
	{42775, 42783}, {42786, 42863}, {42864, 42864}, {42865, 42887},
	{42888, 42888}, {42891, 42894}, {42895, 42895}, {42896, 42954},
	{42960, 42961}, {42963, 42963}, {42965, 42969}, {42994, 42996},
	{42997, 42998}, {42999, 42999}, {43000, 43001}, {43002, 43002},
	{43003, 43009}, {43011, 43013}, {43015, 43018}, {43020, 43042},
	{43072, 43123}, {43138, 43187}, {43250, 43255}, {43259, 43259},
	{43261, 43262}, {43274, 43301}, {43312, 43334}, {43360, 43388},
	{43396, 43442}, {43471, 43471}, {43488, 43492}, {43494, 43494},
	{43495, 43503}, {43514, 43518}, {43520, 43560}, {43584, 43586},
	{43588, 43595}, {43616, 43631}, {43632, 43632}, {43633, 43638},
	{43642, 43642}, {43646, 43695}, {43697, 43697}, {43701, 43702},
	{43705, 43709}, {43712, 43712}, {43714, 43714}, {43739, 43740},
	{43741, 43741}, {43744, 43754}, {43762, 43762}, {43763, 43764},
	{43777, 43782}, {43785, 43790}, {43793, 43798}, {43808, 43814},
	{43816, 43822}, {43824, 43866}, {43868, 43871}, {43872, 43880},
	{43881, 43881}, {43888, 43967}, {43968, 44002}, {44032, 55203},
	{55216, 55238}, {55243, 55291}, {63744, 64109}, {64112, 64217},
	{64256, 64262}, {64275, 64279}, {64285, 64285}, {64287, 64296},
	{64298, 64310}, {64312, 64316}, {64318, 64318}, {64320, 64321},
	{64323, 64324}, {64326, 64433}, {64467, 64829}, {64848, 64911},
	{64914, 64967}, {65008, 65019}, {65136, 65140}, {65142, 65276},
	{65313, 65338}, {65345, 65370}, {65382, 65391}, {65392, 65392},
	{65393, 65437}, {65438, 65439}, {65440, 65470}, {65474, 65479},
	{65482, 65487}, {65490, 65495}, {65498, 65500}, {65536, 65547},
	{65549, 65574}, {65576, 65594}, {65596, 65597}, {65599, 65613},
	{65616, 65629}, {65664, 65786}, {65856, 65908}, {66176, 66204},
	{66208, 66256}, {66304, 66335}, {66349, 66368}, {66369, 66369},
	{66370, 66377}, {66378, 66378}, {66384, 66421}, {66432, 66461},
	{66464, 66499}, {66504, 66511}, {66513, 66517}, {66560, 66639},
	{66640, 66717}, {66736, 66771}, {66776, 66811}, {66816, 66855},
	{66864, 66915}, {66928, 66938}, {66940, 66954}, {66956, 66962},
	{66964, 66965}, {66967, 66977}, {66979, 66993}, {66995, 67001},
	{67003, 67004}, {67072, 67382}, {67392, 67413}, {67424, 67431},
	{67456, 67461}, {67463, 67504}, {67506, 67514}, {67584, 67589},
	{67592, 67592}, {67594, 67637}, {67639, 67640}, {67644, 67644},
	{67647, 67669}, {67680, 67702}, {67712, 67742}, {67808, 67826},
	{67828, 67829}, {67840, 67861}, {67872, 67897}, {67968, 68023},
	{68030, 68031}, {68096, 68096}, {68112, 68115}, {68117, 68119},
	{68121, 68149}, {68192, 68220}, {68224, 68252}, {68288, 68295},
	{68297, 68324}, {68352, 68405}, {68416, 68437}, {68448, 68466},
	{68480, 68497}, {68608, 68680}, {68736, 68786}, {68800, 68850},
	{68864, 68899}, {69248, 69289}, {69296, 69297}, {69376, 69404},
	{69415, 69415}, {69424, 69445}, {69488, 69505}, {69552, 69572},
	{69600, 69622}, {69635, 69687}, {69745, 69746}, {69749, 69749},
	{69763, 69807}, {69840, 69864}, {69891, 69926}, {69956, 69956},
	{69959, 69959}, {69968, 70002}, {70006, 70006}, {70019, 70066},
	{70081, 70084}, {70106, 70106}, {70108, 70108}, {70144, 70161},
	{70163, 70187}, {70207, 70208}, {70272, 70278}, {70280, 70280},
	{70282, 70285}, {70287, 70301}, {70303, 70312}, {70320, 70366},
	{70405, 70412}, {70415, 70416}, {70419, 70440}, {70442, 70448},
	{70450, 70451}, {70453, 70457}, {70461, 70461}, {70480, 70480},
	{70493, 70497}, {70656, 70708}, {70727, 70730}, {70751, 70753},
	{70784, 70831}, {70852, 70853}, {70855, 70855}, {71040, 71086},
	{71128, 71131}, {71168, 71215}, {71236, 71236}, {71296, 71338},
	{71352, 71352}, {71424, 71450}, {71488, 71494}, {71680, 71723},
	{71840, 71903}, {71935, 71942}, {71945, 71945}, {71948, 71955},
	{71957, 71958}, {71960, 71983}, {71999, 71999}, {72001, 72001},
	{72096, 72103}, {72106, 72144}, {72161, 72161}, {72163, 72163},
	{72192, 72192}, {72203, 72242}, {72250, 72250}, {72272, 72272},
	{72284, 72329}, {72349, 72349}, {72368, 72440}, {72704, 72712},
	{72714, 72750}, {72768, 72768}, {72818, 72847}, {72960, 72966},
	{72968, 72969}, {72971, 73008}, {73030, 73030}, {73056, 73061},
	{73063, 73064}, {73066, 73097}, {73112, 73112}, {73440, 73458},
	{73474, 73474}, {73476, 73488}, {73490, 73523}, {73648, 73648},
	{73728, 74649}, {74752, 74862}, {74880, 75075}, {77712, 77808},
	{77824, 78895}, {78913, 78918}, {82944, 83526}, {92160, 92728},
	{92736, 92766}, {92784, 92862}, {92880, 92909}, {92928, 92975},
	{92992, 92995}, {93027, 93047}, {93053, 93071}, {93760, 93823},
	{93952, 94026}, {94032, 94032}, {94099, 94111}, {94176, 94177},
	{94179, 94179}, {94208, 100343}, {100352, 101589}, {101632, 101640},
	{110576, 110579}, {110581, 110587}, {110589, 110590}, {110592, 110882},
	{110898, 110898}, {110928, 110930}, {110933, 110933}, {110948, 110951},
	{110960, 111355}, {113664, 113770}, {113776, 113788}, {113792, 113800},
	{113808, 113817}, {119808, 119892}, {119894, 119964}, {119966, 119967},
	{119970, 119970}, {119973, 119974}, {119977, 119980}, {119982, 119993},
	{119995, 119995}, {119997, 120003}, {120005, 120069}, {120071, 120074},
	{120077, 120084}, {120086, 120092}, {120094, 120121}, {120123, 120126},
	{120128, 120132}, {120134, 120134}, {120138, 120144}, {120146, 120485},
	{120488, 120512}, {120514, 120538}, {120540, 120570}, {120572, 120596},
	{120598, 120628}, {120630, 120654}, {120656, 120686}, {120688, 120712},
	{120714, 120744}, {120746, 120770}, {120772, 120779}, {122624, 122633},
	{122634, 122634}, {122635, 122654}, {122661, 122666}, {122928, 122989},
	{123136, 123180}, {123191, 123197}, {123214, 123214}, {123536, 123565},
	{123584, 123627}, {124112, 124138}, {124139, 124139}, {124896, 124902},
	{124904, 124907}, {124909, 124910}, {124912, 124926}, {124928, 125124},
	{125184, 125251}, {125259, 125259}, {126464, 126467}, {126469, 126495},
	{126497, 126498}, {126500, 126500}, {126503, 126503}, {126505, 126514},
	{126516, 126519}, {126521, 126521}, {126523, 126523}, {126530, 126530},
	{126535, 126535}, {126537, 126537}, {126539, 126539}, {126541, 126543},
	{126545, 126546}, {126548, 126548}, {126551, 126551}, {126553, 126553},
	{126555, 126555}, {126557, 126557}, {126559, 126559}, {126561, 126562},
	{126564, 126564}, {126567, 126570}, {126572, 126578}, {126580, 126583},
	{126585, 126588}, {126590, 126590}, {126592, 126601}, {126603, 126619},
	{126625, 126627}, {126629, 126633}, {126635, 126651}, {131072, 173791},
	{173824, 177977}, {177984, 178205}, {178208, 183969}, {183984, 191456},
	{191472, 192093}, {194560, 195101}, {196608, 201546}, {201552, 205743}
};


} // namespace ada::idna
#endif // ADA_IDNA_IDENTIFIER_TABLES_H

