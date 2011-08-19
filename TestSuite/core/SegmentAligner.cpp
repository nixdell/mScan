#include "configuration.h"
#include "Addons.h"
#include "AlignmentUtils.h"
#include "SegmentAligner.h"

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <iostream>

#ifdef OUTPUT_DEBUG_IMAGES
#include "NameGenerator.h"
NameGenerator alignmentNamer("debug_segment_images/", false);
#endif

//#define USE_INTERSECTIONS
//There is a tradeoff with using intersections.
//It seems to work better on messy segments, however,
//on segments with multiple min energy lines we are more likely
//to choose the wrong line than with the contour method.

using namespace std;
using namespace cv;

// Try to distil the maximum quad (4 point contour) from a convex contour of many points.
// if none is found maxQuad will not be altered.
// TODO: Find out what happens when you try to simplify a contour that already has just 4 points.
float maxQuadSimplify(vector <Point>& contour, vector<Point>& maxQuad, float current_approx_p){
	
	float area = 0;
	
	float arc_len = arcLength(Mat(contour), true);
	while (current_approx_p < 1) {
		vector <Point> approx;
		approxPolyDP(Mat(contour), approx, arc_len * current_approx_p, true);
		if (approx.size() == 4){
			maxQuad = approx;
			area = fabs(contourArea(Mat(approx)));
			break;
		}
		current_approx_p += .01;
	}
	return area;
}
//Find the largest quad contour in img
//Warning: destroys img
vector<Point> findMaxQuad(Mat& img, float approx_p_seed = 0){
	vector<Point> maxRect;
	vector < vector<Point> > contours;
	// Find all external contours of the image
	findContours(img, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

	float maxContourArea = 0;
	// Iterate through all detected contours
	for (size_t i = 0; i < contours.size(); ++i) {
		vector<Point> contour, quad;
		convexHull(Mat(contours[i]), contour);
		float area = maxQuadSimplify(contour, quad, approx_p_seed);

		if (area > maxContourArea) {
			maxRect = quad;
			maxContourArea = area;
		}
	}
	return maxRect;
}
//Sum the pixels that lie on a line starting and ending in the specified rows.
int lineSum(const Mat& img, int start, int end, bool transpose) {

	int hSpan;
	if(transpose){
		hSpan = img.rows - 1;
	}
	else{
		hSpan = img.cols - 1;
	}
	
	int sum = 0;
	double slope = (double)(end - start)/hSpan;
	
	for(int i = 0; i<=hSpan; i++) {
		int j = start + slope*i;

		if(j < 0){
			sum+=127;
		}
		else{
			if(transpose){
				sum += img.at<uchar>(i, j);
			}
			else{
				sum += img.at<uchar>(j, i);
			}
		}
	}
	return sum;
}
void findLinesHelper(const Mat& img, int& start, int& end, const Rect& roi, bool flip, bool transpose) {
	int vSpan, hSpan;
	int range, midpoint;
	float maxSlope = .15;

	if(transpose){
		vSpan = img.cols - 1;
		hSpan = img.rows - 1;
		midpoint = roi.x;
		range = roi.y;
	}
	else{		
		vSpan = img.rows - 1;
		hSpan = img.cols - 1;
		midpoint = roi.y;
		range = roi.y;
	}
	
	//The param limits the weigting to a certain magnitude, in this case 10% of the max.
	int param = .15 * 255 * (hSpan + 1);
	float maxSsdFromMidpoint = 2*range*range;
	
	int minLs = INT_MAX;
	for(int i = midpoint - range; i < midpoint + range; i++) {
		for(int j = MAX(i-hSpan*maxSlope, midpoint - range); j < MIN(i+hSpan*maxSlope, midpoint + range); j++) {

			float ssdFromMidpoint = (i - midpoint)*(i - midpoint) + (j - midpoint)*(j - midpoint);
			int ls = param * ssdFromMidpoint / maxSsdFromMidpoint;
			if(flip){
				ls += lineSum(img, vSpan - i, vSpan - j, transpose);
			}
			else{
				ls += lineSum(img, i, j, transpose);
			}
			if( ls < minLs ){
				start = i;
				end = j;
				minLs = ls;
			}
		}
	}
}
//Find the minimum energy lines crossing the image.
//A and B are the start and end points of the line.
void findLines(const Mat& img, Point& A, Point& B, const Rect& roi, bool flip, bool transpose) {
	int start, end;
	
	findLinesHelper(img, start, end, roi, flip, transpose);
	
	if(flip && transpose){
		A = Point(img.cols - 1 - start, img.rows-1);
		B = Point(img.cols - 1 - end, 0);
	}
	else if(!flip && transpose){
		A = Point(start, 0);
		B = Point(end, img.rows -1);
	}
	else if(flip && !transpose){
		A = Point(0, img.rows - 1 - start);
		B = Point(img.cols-1, img.rows - 1 - end);
	}
	else{
		A = Point(0, start);
		B = Point(img.cols-1, end);
	}
}
inline Point findIntersection(const Point& P1, const Point& P2,
						const Point& P3, const Point& P4){
	// From determinant formula here:
	// http://en.wikipedia.org/wiki/Line_intersection
	int denom = (P1.x - P2.x) * (P3.y - P4.y) - (P1.y - P2.y) * (P3.x - P4.x);
	return Point(
		( (P1.x * P2.y - P1.y * P2.x) * (P3.x - P4.x) -
		  (P1.x - P2.x) * (P3.x * P4.y - P3.y * P4.x) ) / denom,
		( (P1.x * P2.y - P1.y * P2.x) * (P3.y - P4.y) -
		  (P1.y - P2.y) * (P3.x * P4.y - P3.y * P4.x) ) / denom);
}
//TODO: reimplement scaling to 128 px
vector<Point> findSegment(const Mat& img, const Rect& roi){
	Mat imgThresh, temp_img, temp_img2;
	
	int blurSize = 9;
	
	#if 1
		blur(img, temp_img, 2*Size(2*blurSize+1, 2*blurSize+1));
	#else
		//Not sure if this had advantages or not...
		//My theory is that it is slower but more accurate,
		//but I don't know if either difference is significant enough to notice.
		//Will need to test.
		GaussianBlur(img, temp_img, Size(9, 9), 3, 3);
	#endif

	imgThresh = (img - temp_img) > 0;

	Rect contractedRoi = resizeRect(roi, .7);
	
	imgThresh(contractedRoi) = Scalar::all(255);
	
	Point A1, B1, A2, B2, A3, B3, A4, B4;
	findLines(imgThresh, A1, B1, roi, false, true);
	findLines(imgThresh, A2, B2, roi, false, false);
	findLines(imgThresh, A3, B3, roi, true, true);
	findLines(imgThresh, A4, B4, roi, true, false);


	#ifdef USE_INTERSECTIONS
		vector <Point> quad;
		quad.push_back(findIntersection(A1, B1, A2, B2));
		quad.push_back(findIntersection(A2, B2, A3, B3));
		quad.push_back(findIntersection(A3, B3, A4, B4));
		quad.push_back(findIntersection(A4, B4, A1, B1));
		
		//TODO: add some code that does this:
		//		lines can be used to mask off sections of the image
		//		if there is a height/width discrepancy move the weighting line to the averate of the expected lines.
		
	#else
		#define EXPANSION_PERCENTAGE .05
		line( imgThresh, A1, B1, Scalar::all(0), 1, 4);
		line( imgThresh, A2, B2, Scalar::all(0), 1, 4);
		line( imgThresh, A3, B3, Scalar::all(0), 1, 4);
		line( imgThresh, A4, B4, Scalar::all(0), 1, 4);
		Mat imgThresh2;
		imgThresh.copyTo(imgThresh2);
		vector<Point> quad = findMaxQuad(imgThresh2, 0);
		quad = expandCorners(quad, EXPANSION_PERCENTAGE);
	#endif
	
	#if 0 
		//refine corner locations
		//This seems to actually do worse

		TermCriteria termcrit(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS,20,0.03);
		cornerSubPix(img, quad, Size(3,3), Size(2, 2), termcrit);
	#endif
	
	#ifdef OUTPUT_DEBUG_IMAGES
		Mat dbg_out, dbg_out2;
		imgThresh.copyTo(dbg_out);
		string segfilename = alignmentNamer.get_unique_name("alignment_debug_");
		segfilename.append(".jpg");
		imwrite(segfilename, dbg_out);
	#endif

	return quad;
}
