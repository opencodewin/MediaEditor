#pragma once
#ifndef DITHER_ERRORDIFF_DATA_H
#define DITHER_ERRORDIFF_DATA_H

const int matrix_xot[] = {0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 4, 4, 0,
                          0, 0, 0, 0, 0,  0, 0, 0, 0, 7, 7, 7, 7, 0,
                          0, 0, 0, 0, 0,  0, 0, 0, 4, 7, 7, 7, 7, 4,
                          0, 0, 0, 0, 0,  0, 0, 0, 4, 7, 7, 7, 7, 4,
                          0, 0, 2, 2, 0,  0, 3, 3, 0, 7, 8, 8, 7, 0,
                          0, 3, 3, 3, 5,  6, 5, 5, 5, 7, 5, 5, 4, 0,
                          2, 3, 3, 3, 7,  6, 5, 5, 4, 6, 1, 1, 1, 0,
                          2, 3, 3, 3, 7,  6, 5, 5, 4, 6, 1, 1, 1, 1,
                          0, 3, 3, 3, 5,  6, 5, 5, 5, 5, 1, 1, 1, 1,
                          0, 0, 2, 2, 0,  0, 3, 3, 0, 0, 0, 1, 1, 0};

const int matrix_diagonal[] = {0, -1, 5,
                               2,  3, 6};

const int matrix_floyd_steinberg[] = {0, -1, 7,
                                      3,  5, 1};

const int matrix_shiaufan_3[] = {0, 0, -1, 4,
                                 1, 1,  2, 0};

const int matrix_shiaufan_2[] = {0, 0, -1, 7,
                                 1, 3,  5, 0};

const int matrix_shiaufan_1[] = {0, 0, 0, -1, 8,
                                 1, 1, 2,  4, 0};

const int matrix_stucki[] = {0, 0, -1, 8, 4,
                             2, 4,  8, 4, 2,
                             1, 2,  4, 2, 1};

const int matrix_diffusion_1d[] = {-1, 1};

const int matrix_diffusion_2d[] = {-1, 1,
                                    1, 0};

const int matrix_fake_floyd_steinberg[] = {-1, 3,
                                            3, 2};

const int matrix_jarvis_judice_ninke[] = {0, 0, -1, 7, 5,
                                          3, 5,  7, 5, 3,
                                          1, 3,  5, 3, 1};

const int matrix_atkinson[] = {0, -1, 1, 1,
                               1,  1, 1, 0,
                               0,  1, 0, 0};

const int matrix_burkes[] = {0, 0, -1, 8, 4,
                             2, 4,  8, 4, 2};

const int matrix_sierra_3[] = {0, 0, -1, 5, 3,
                               2, 4,  5, 4, 2,
                               0, 2,  3, 2, 0};

const int matrix_sierra_2row[] = {0, 0, -1, 4, 3,
                                  1, 2,  3, 2, 1};

const int matrix_sierra_lite[] = {0, -1, 2,
                                  1,  1, 0};

const int matrix_steve_pigeon[] = {0, 0, -1, 2, 1,
                                   0, 2,  2, 2, 0,
                                   1, 0,  1, 0, 1};

const int matrix_robert_kist[] = { 0,  0, -1, 90,  0,
                                  10, 20, 30, 20, 10,
                                  10,  5, 10,  5, 10};

const int matrix_stevenson_arce[] = {0,  0,  0, -1, 32,  0, 0,
                                     6, 13, 10, 19, 10, 18, 8,
                                     0,  0, 12, 26, 12,  0, 0,
                                     3,  6,  4,  8,  4,  7, 2};

#endif  // DITHER_ERRORDIFF_DATA_H
