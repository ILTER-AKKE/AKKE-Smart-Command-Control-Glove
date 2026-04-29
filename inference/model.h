#pragma once
#include <cstdarg>
namespace Eloquent {
    namespace ML {
        namespace Port {
            class DecisionTree {
                public:
                    /**
                    * Predict class for features vector
                    */
                    int predict(float *x) {
                        if (x[261] <= 1.7303001284599304) {
                            if (x[62] <= -1.4750182032585144) {
                                return 9;
                            }

                            else {
                                if (x[533] <= 2.719262480735779) {
                                    if (x[48] <= -2.805757761001587) {
                                        return 7;
                                    }

                                    else {
                                        if (x[300] <= -2.180982530117035) {
                                            return 11;
                                        }

                                        else {
                                            if (x[349] <= -1.7708148956298828) {
                                                return 5;
                                            }

                                            else {
                                                if (x[544] <= 1.8274309635162354) {
                                                    if (x[359] <= 1.1169883012771606) {
                                                        if (x[501] <= 0.5841213166713715) {
                                                            if (x[457] <= 1.4014433026313782) {
                                                                return 8;
                                                            }

                                                            else {
                                                                return 17;
                                                            }
                                                        }

                                                        else {
                                                            return 4;
                                                        }
                                                    }

                                                    else {
                                                        return 22;
                                                    }
                                                }

                                                else {
                                                    return 19;
                                                }
                                            }
                                        }
                                    }
                                }

                                else {
                                    return 0;
                                }
                            }
                        }

                        else {
                            return 18;
                        }
                    }

                    /**
                    * Predict readable class name
                    */
                    const char* predictLabel(float *x) {
                        return idxToLabel(predict(x));
                    }

                    /**
                    * Convert class idx to readable name
                    */
                    const char* idxToLabel(uint8_t classIdx) {
                        switch (classIdx) {
                            case 0:
                            return "breacher";
                            case 1:
                            return "cover_area";
                            case 2:
                            return "dog";
                            case 3:
                            return "eight";
                            case 4:
                            return "enemy";
                            case 5:
                            return "five";
                            case 6:
                            return "four";
                            case 7:
                            return "freeze";
                            case 8:
                            return "gas";
                            case 9:
                            return "hear";
                            case 10:
                            return "hostage";
                            case 11:
                            return "I_understand";
                            case 12:
                            return "leader";
                            case 13:
                            return "line_abreast_form";
                            case 14:
                            return "me";
                            case 15:
                            return "nine";
                            case 16:
                            return "one";
                            case 17:
                            return "pistol";
                            case 18:
                            return "rifle";
                            case 19:
                            return "seven";
                            case 20:
                            return "six";
                            case 21:
                            return "sniper";
                            case 22:
                            return "stop";
                            case 23:
                            return "ten";
                            case 24:
                            return "three";
                            case 25:
                            return "two";
                            case 26:
                            return "watch";
                            case 27:
                            return "you";
                            default:
                            return "Houston we have a problem";
                        }
                    }

                protected:
                };
            }
        }
    }