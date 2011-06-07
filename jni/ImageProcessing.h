/*
 * Header file for image processing functions.
 */
#ifndef IMAGEPROCESSING_H
#define IMAGEPROCESSING_H

#include "cv.h"
#include "cxcore.h"
#include "highgui.h"
#include <string>

enum bubble_val { EMPTY_BUBBLE, FILLED_BUBBLE, FALSE_POSITIVE };

// takes a filename and processes the entire image for bubbles
void ProcessImage(char* &imagefilename, char* &bubblefilename, float &weight);

// trains the program what bubbles look like
void train_PCA_classifier();

// takes a filename and JSON spec and looks for bubbles according
// to locations coded in the JSON
// int ProcessImage(string image, JSON_OBJ json);

// function prototype to pass image processing parameters
// int ProcessImage(string image, <params>);

#endif
