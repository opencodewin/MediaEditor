#include <immat.h>
#include <iostream>

const float u_base_data[] = 
{
   -73587.203125,    18534.218750,    17761.292969,   -71635.500000,    -1239.267456,    20176.726562,   -67755.859375,   -19247.423828,
    21949.949219,   -63909.558594,   -35499.628906,    25764.832031,   -58451.136719,   -52979.507812,    34959.335938,   -48654.300781,
   -66975.859375,    51184.789062,   -36992.675781,   -75374.070312,    70958.078125,   -22101.191406,   -82379.976562,    89177.695312,
      -99.712112,   -86363.851562,    96279.804688,    21907.609375,   -82129.093750,    89108.585938,    36704.296875,   -75410.398438,
    70913.046875,    48279.632812,   -67213.429688,    51247.210938,    57985.000000,   -53404.750000,    35025.199219,    63446.265625,
   -36008.441406,    25864.835938,    67145.460938,   -19624.191406,    21921.433594,    70689.125000,    -1505.619141,    20048.048828,
    72560.289062,    18477.806641,    17913.117188,   -57513.765625,    40301.324219,    80472.179688,   -49231.597656,    46685.488281,
    92443.515625,   -38662.386719,    48791.496094,   100783.859375,   -28514.189453,    47888.933594,   105897.250000,   -19345.310547,
    45338.933594,   108324.585938,    18272.402344,    45574.085938,   108260.851562,    27488.544922,    48179.992188,   105741.304688,
    37696.300781,    49140.707031,   100549.234375,    48449.042969,    47009.718750,    92279.570312,    56731.460938,    40457.625000,
    80323.414062,     -260.905487,    26333.482422,   111411.218750,     -216.724335,    14285.270508,   120398.492188,     -110.514717,
     2285.394043,   129726.859375,     -114.867363,    -7999.345703,   131937.234375,   -12262.216797,   -16963.917969,   109804.531250,
    -7183.948242,   -18007.380859,   114322.296875,     -283.630371,   -19356.876953,   116415.718750,     6558.261230,   -17986.718750,
   114289.453125,    11582.026367,   -16901.191406,   109755.765625,   -43531.933594,    25934.927734,    87251.898438,   -37155.386719,
    29778.888672,    95412.054688,   -28087.015625,    29900.937500,    95595.085938,   -19645.060547,    25604.958984,    93105.296875,
   -27260.740234,    23498.718750,    95298.773438,   -36732.367188,    23025.500000,    93026.265625,    18359.757812,    25608.427734,
    92784.015625,    26858.835938,    30010.142578,    95186.898438,    36151.250000,    29765.195312,    95168.234375,    42730.125000,
    25819.912109,    87187.484375,    35768.945312,    23163.605469,    92950.140625,    26177.851562,    23485.007812,    95104.828125,
   -25777.628906,   -41231.679688,    99206.101562,   -16756.039062,   -35284.289062,   110437.351562,    -5994.805664,   -31389.068359,
   116480.539062,     -264.001709,   -32509.755859,   117079.453125,     5443.545410,   -31391.337891,   116457.257812,    16149.167969,
   -35290.683594,   110337.359375,    24596.279297,   -41268.218750,    98994.296875,    15613.271484,   -45419.242188,   108561.773438,
     7920.009277,   -48314.191406,   112856.117188,     -239.078278,   -48816.875000,   113861.015625,    -8337.559570,   -48279.386719,
   113022.656250,   -15942.025391,   -45445.546875,   108729.125000,   -23246.917969,   -40716.281250,    99953.726562,    -7677.564453,
   -38100.976562,   111375.851562,     -379.065887,   -37913.093750,   113163.101562,     6976.025391,   -38159.476562,   111353.492188,
    22934.208984,   -40797.828125,    99583.453125,     6796.848633,   -40506.558594,   111575.406250,     -401.142456,   -40866.496094,
   112449.687500,    -7499.820801,   -40433.066406,   111496.351562,
};

int main(int argc, char ** argv)
{
#if 0
    int mw = 4;
    int mh = 4;
    ImGui::ImMat A, B, C, X;
    A.create_type(mw, mh, IM_DT_FLOAT32);
    B.create_type(mw, mh, IM_DT_FLOAT32);
    X.create_type(mw, mh, IM_DT_INT8);

    for (int y = 0; y < A.h; y++)
    {
        for (int x = 0; x < A.w; x++)
        {
            A.at<float>(x,y) += y * A.w + x + 1;
        }
    }

    for (int y = 0; y < B.h; y++)
    {
        for (int x = 0; x < B.w; x++)
        {
            B.at<float>(x,y) = y * B.w + x + 1;
        }
    }

    A.print("A");
    B.print("B");

    C = A.diag<float>();
    C.print("C=A.diag");

    // scalar math
    C = B + 2.f;
    C.print("C=B+2");

    B += 2.f;
    B.print("B+=2");

    C = B - 2.f;
    C.print("C=B-2");

    B -= 2.f;
    B.print("B-=2");

    C = A * 2.0f;
    C.print("C=A*2");

    A *= 2.0f;
    A.print("A*=2");

    C = A / 2.0f;
    C.print("C=A/2");

    A /= 2.0f;
    A.print("A/=2");

    // mat math
    C = A + B;
    C.print("C=A+B");

    A += B;
    A.print("A+=B");

    C = A - B;
    C.print("C=A-B");

    A -= B;
    A.print("A-=B");

    C = A * B;
    C.print("C=A*B");

    A *= B;
    A.print("A*=B");

    C = A.clip(200, 500);
    C.print("C=A.clip(200,500)");

    // mat tranform
    auto t = A.t();
    t.print("A.t");

    // mat setting
    auto e = A.eye(1.f);
    e.print("A.eye");

    auto n = A.randn<float>(0.f, 5.f);
    n.print("A.randn");

    // mat matrix math
    C = n.inv<float>();
    C.print("C=A.randn.i");

    X = X.randn(128.f, 128.f);
    X.print("INT8 randn");

    // fp16
    ImGui::ImMat A16, B16, C16;
    A16.create_type(mw, mh, IM_DT_FLOAT16);
    B16.create_type(mw, mh, IM_DT_FLOAT16);
    for (int y = 0; y < A16.h; y++)
    {
        for (int x = 0; x < A16.w; x++)
        {
            A16.at<uint16_t>(x,y) = im_float32_to_float16(y * A16.w + x + 1);
        }
    }
    for (int y = 0; y < B16.h; y++)
    {
        for (int x = 0; x < B16.w; x++)
        {
            B16.at<uint16_t>(x,y) = im_float32_to_float16(y * B16.w + x + 1);
        }
    }

    A16.print("A16");
    B16.print("B16");

    // fp16 scalar math
    C16 = B16 + 2.f;
    C16.print("C16=B16+2");

    B16 += 2.f;
    B16.print("B16+=2");

    C16 = B16 - 2.f;
    C16.print("C16=B16-2");

    B16 -= 2.f;
    B16.print("B16-=2");

    C16 = A16 * 2.0f;
    C16.print("C16=A16*2");

    A16 *= 2.0f;
    A16.print("A16*=2");

    C16 = A16 / 2.0f;
    C16.print("C16=A16/2");

    A16 /= 2.0f;
    A16.print("A16/=2");

    // mat math
    C16 = A16 + B16;
    C16.print("C16=A16+B16");

    A16 += B16;
    A16.print("A16+=B16");

    C16 = A16 - B16;
    C16.print("C16=A16-B16");

    A16 -= B16;
    A16.print("A16-=B16");

    C16 = A16 * B16;
    C16.print("C16=A16*B16");

    A16 *= B16;
    A16.print("A16*=B16");

    C16 = A16.clip(200, 500);
    C16.print("C16=A16.clip(200,500)");

    // mat tranform
    auto t16 = A16.t();
    t16.print("A16.t");

    // mat setting
    auto e16 = A16.eye(1.f);
    e16.print("A16.eye");

    auto n16 = A16.randn<float>(0.f, 5.f);
    n16.print("A16.randn");

    // mat matrix math
    //C16 = n16.inv<float>();
    //C16.print("C16=A16.randn.i");

    A.create_type(2, 2, IM_DT_FLOAT32);
    A.at<float>(0, 0) = 3.f;
    A.at<float>(1, 0) = 8.f;
    A.at<float>(0, 1) = 4.f;
    A.at<float>(1, 1) = 6.f;
    A.print("A");
    auto ra = A.determinant();
    fprintf(stderr, "A.determinant:%f\n", ra);

    A.at<float>(0, 0) = 4.f;
    A.at<float>(1, 0) = 6.f;
    A.at<float>(0, 1) = 3.f;
    A.at<float>(1, 1) = 8.f;
    A.print("A");
    auto ra2 = A.determinant();
    fprintf(stderr, "A.determinant:%f\n", ra2);

    B.create_type(3, 3, IM_DT_FLOAT32);
    B.at<float>(0, 0) = 6.f;
    B.at<float>(1, 0) = 1.f;
    B.at<float>(2, 0) = 1.f;
    B.at<float>(0, 1) = 4.f;
    B.at<float>(1, 1) = -2.f;
    B.at<float>(2, 1) = 5.f;
    B.at<float>(0, 2) = 2.f;
    B.at<float>(1, 2) = 8.f;
    B.at<float>(2, 2) = 7.f;
    B.print("B");
    auto rb = B.determinant();
    fprintf(stderr, "B.determinant:%f\n", rb);

    auto nb = -B;
    nb.print("-B");

    auto sb = B.sum();
    sb.print("B.sum");

    ImGui::ImMat d(1, 3);
    d.at<float>(0, 0) = 1;
    d.at<float>(0, 1) = 2;
    d.at<float>(0, 2) = 3;
    ImGui::ImMat diag_ = d.diag<float>();
    diag_.print("diag");

    ImGui::ImMat src_matrix(2,5);
    src_matrix.at<float>(0,0) = 38.2946; src_matrix.at<float>(1, 0) = 51.6963;
    src_matrix.at<float>(0,1) = 73.5318; src_matrix.at<float>(1, 1) = 51.5014;
    src_matrix.at<float>(0,2) = 56.0252; src_matrix.at<float>(1, 2) = 71.7366;
    src_matrix.at<float>(0,3) = 41.5493; src_matrix.at<float>(1, 3) = 92.3655;
    src_matrix.at<float>(0,4) = 70.7299; src_matrix.at<float>(1, 4) = 92.2041;
    ImGui::ImMat dst_matrix(2,5);
    dst_matrix.at<float>(0, 0) = 18; dst_matrix.at<float>(1, 0) = 31;
    dst_matrix.at<float>(0, 1) = 53; dst_matrix.at<float>(1, 1) = 31;
    dst_matrix.at<float>(0, 2) = 36; dst_matrix.at<float>(1, 2) = 51;
    dst_matrix.at<float>(0, 3) = 21; dst_matrix.at<float>(1, 3) = 72;
    dst_matrix.at<float>(0, 4) = 50; dst_matrix.at<float>(1, 4) = 72;
    
    ImGui::ImMat M = ImGui::similarTransform(dst_matrix, src_matrix);
    src_matrix.print("src");
    dst_matrix.print("dst");
    M.print("M");
#endif
    const float feature_buffer[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}; 
    ImGui::ImMat p_;
    p_.create_type(4, 3, (void*)feature_buffer, IM_DT_FLOAT32);
    ImGui::ImMat base;
    base.create_type(1, 204, (void*)u_base_data, IM_DT_FLOAT32);
    ImGui::ImMat p = p_.crop(ImPoint(0, 0), ImPoint(3, 3));
    ImGui::ImMat offset = p_.crop(ImPoint(3, 0), ImPoint(4, 3));
    ImGui::ImMat rbase = base.reshape(3, 68);
    ImGui::ImMat V = p * rbase.t() + offset.repeat(68, 1);
    ImGui::ImMat vertex_x = V.crop(ImPoint(0, 0), ImPoint(68, 1));
    ImGui::ImMat vertex_y = V.crop(ImPoint(0, 1), ImPoint(68, 2));
    ImGui::ImMat vertex_depth = V.crop(ImPoint(0, 2), ImPoint(68, 3));
    p_.print("p_");
    offset.print("offset");
    rbase.print("rbase");
    V.print("V");
    vertex_x.print("vertex_x");
    vertex_y.print("vertex_y");
    vertex_depth.print("vertex_depth");

    ImGui::ImMat v_mean = vertex_depth.mean();
    v_mean.print("v_mean");

    float vmin, vmax;
    int imin, imax;
    vertex_depth.minmax(&vmin, &vmax, &imin, &imax);
    std::cout << "Min:" << vmin << "(" << imin << ")" << " Max:" << vmax << "(" << imax << ")" << std::endl;
    vertex_depth.normalize(0.f, 255.f, ImGui::ImMat::NormTypes::NORM_MINMAX);

    vertex_depth.print("vertex_depth norm");

    auto norm = vertex_depth.norm(ImGui::ImMat::NormTypes::NORM_L1);
    std::cout << "vertex_depth.norm l1:" << norm << std::endl;
    norm = vertex_depth.norm(ImGui::ImMat::NormTypes::NORM_L2);
    std::cout << "vertex_depth.norm l2:" << norm << std::endl;
    norm = vertex_depth.norm(ImGui::ImMat::NormTypes::NORM_INF);
    std::cout << "vertex_depth.norm inf:" << norm << std::endl;

    const float r1data[] = {1, 2, 3};
    const float r2data[] = {3, 2, 1};
    ImGui::ImMat r1(3, 1, (void*)r1data, 4u);
    ImGui::ImMat r2(3, 1, (void*)r2data, 4u);
    ImGui::ImMat r3(r1.w, r1.h);
    r3.at<float>(0,0) = r1.at<float>(1,0) * r2.at<float>(2,0) - r1.at<float>(2,0) * r2.at<float>(1,0);
    r3.at<float>(1,0) = r1.at<float>(2,0) * r2.at<float>(0,0) - r1.at<float>(0,0) * r2.at<float>(2,0);
    r3.at<float>(2,0) = r1.at<float>(0,0) * r2.at<float>(1,0) - r1.at<float>(1,0) * r2.at<float>(0,0);

    r1.print("r1");
    r2.print("r2");
    r3.print("r3");

    ImGui::ImMat ba(4, 3);
    ba = ba.eye(1.f);
    ImGui::ImMat t3d = ba.crop(ImPoint(3, 0), ImPoint(4, 3)).t();
    ImGui::ImMat R = r1.vconcat(r2);
    R = R.vconcat(r3);
    ImGui::ImMat P = R.hconcat(t3d.t());

    ba.print("ba");
    t3d.print("t3d");
    P.print("P");

    return 0;
}