#pragma once
#ifndef DITHER_RIEMERSMA_DATA_H
#define DITHER_RIEMERSMA_DATA_H

const char* hilbert_axiom = "X";
const char hilbert_keys[] = {'X', 'Y'};
const char* hilbert_rules[] = {"-YF+XFX+FY-",
                               "+XF-YFY-FX+"};
const int hilbert_orientation[2] = {1, 0};

const char* hilbert_mod_axiom = "AFA+F+AFA";
const char  hilbert_mod_keys[] = {'A', 'B'};
const char* hilbert_mod_rules[] ={"-BF+AFA+FB-",
                                  "+AF-BFB-FA+"};
const int hilbert_mod_orientation[2] = {0, 1};

const char* peano_axiom = "L";
const char  peano_keys[] = {'L', 'R'};
const char* peano_rules[] = {"LFRFL-F-RFLFR+F+LFRFL",
                             "RFLFR+F+LFRFL-F-RFLFR"};
const int peano_orientation[2] = {1, 0};

const char* fass0_axiom = "L";
const char  fass0_keys[] = {'L', 'R'};
const char* fass0_rules[] = {"LF+RFR+FL-FRF-LFL-FR+F+RF-LFL-FRF-LF+RFR+FL",
                             "RFRF-LFLFL-F-LF+RFR+FLF+RFRFRFR+FLFLFL-"};
const int fass0_orientation[2] = {0, 1};

const char* fass1_axiom = "L";
const char  fass1_keys[] = {'L', 'R'};
const char* fass1_rules[] = {"LF+RFR+FL-F-LFLFL-FRFR+",
                             "-LFLF+RFRFR+F+RF-LFL-FR"};
const int fass1_orientation[2] = {0, 1};

const char* fass2_axiom = "L";
const char  fass2_keys[] = {'L', 'R'};
const char* fass2_rules[] = {"LFLF+RFR+FLFL-FRF-LFL-FR+F+RF-LFL-FRFRFR+",
                             "-LFLFLF+RFR+FL-F-LF+RFR+FLF+RFRF-LFL-FRFR"};
const int fass2_orientation[2] = {0, 1};

const char* gosper_axiom = "YF";
const char  gosper_keys[] = {'X', 'Y'};
const char* gosper_rules[] = {"XFX-YF-YF+FX+FX-YF-YFFX+YF+FXFXYF-FX+YF+FXFX+YF-FXYF-YF-FX+FX+YFYF-",
                              "+FXFX-YF-YF+FX+FXYF+FX-YFYF-FX-YF+FXYFYF-FX-YFFX+FX+YF-YF-FX+FX+YFY"};
const int gosper_orientation[2] = {0, 1};

const char* fass_spiral_axiom = "FY";
const char  fass_spiral_keys[] = {'L', 'R', 'Y'};
const char* fass_spiral_rules[] = {"LF+RFR+FL-F-LFLFL-FRFR+",
                                   "-LFLF+RFRFR+F+RF-LFL-FR",
                                   "Y+RFR+FLF+RFRFR+FLFLF"};
const int fass_spiral_orientation[2] = {0, 1};

#endif  // DITHER_RIEMERSMA_DATA_H
