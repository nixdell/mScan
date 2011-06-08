#include "cv.h"
#include "cxcore.h"
#include "highgui.h"
#include "ImageProcessing.h"
#include <string>
#include <iostream>
#include <fstream>
#include <vector>

#include "./ImageProcessing.h"
#include "./FileUtils.h"

using namespace std;
using namespace cv;

/*
 * processing constants
 */
#define DILATION 6
#define BLOCK_SIZE 3
#define DIST_PARAM 500

#define EXAMPLE_WIDTH 14
#define EXAMPLE_HEIGHT 18

// how wide is the segment in pixels
#define SEGMENT_WIDTH 144

// how tall is the segment in pixels
#define SEGMENT_HEIGHT 200

#define SCALEPARAM 0.55

// buffer around segment in pixels
#define SEGMENT_BUFFER 70

#define EIGENBUBBLES 3

#define DEBUG 1

Mat comparison_vectors;
PCA my_PCA;
vector <bubble_val> training_bubble_values;
vector <Point2f> training_bubbles_locations;
float weight_param;
string imgfilename;
Point search_window(1, 1);

void configCornerArray(vector<Point2f>& corners, Point2f* corners_a);
void straightenImage(const Mat& input_image, Mat& output_image);
double rateBubble(Mat& det_img_gray, Point bubble_location);
bubble_val checkBubble(Mat& det_img_gray, Point bubble_location);
void getSegmentLocations(vector<Point2f> &segmentcorners, string segfile);
vector<bubble_val> processSegment(Mat &segment, string bubble_offsets);
Mat getSegmentMat(Mat &img, Point2f &corner);
void find_bounding_lines(Mat& img, int* upper, int* lower, bool vertical);
void align_segment(Mat& img, Mat& aligned_segment);

template <class Tp>
void configCornerArray(vector<Tp>& orig_corners, Point2f* corners_a, float expand) {
  float min_dist;
  int min_idx;
  float dist;
  
  vector<Point2f> corners;
  
  for(int i = 0; i < orig_corners.size(); i++ ){
    corners.push_back(Point2f(float(orig_corners[i].x), float(orig_corners[i].y)));
  }
  //Make sure the form corners map to the correct image corner
  //by snaping the nearest form corner to each image corner.
  for(int i = 0; i < 4; i++) {
    min_dist = FLT_MAX;
    for(int j = 0; j < corners.size(); j++ ){
      dist = norm(corners[j]-corners_a[i]);
      if(dist < min_dist){
        min_dist = dist;
        min_idx = j;
      }
    }
    corners_a[i]=corners[min_idx] + expand * (corners_a[i] - corners[min_idx]);
    //Two relatively minor reasons for doing this,
    //1. Small efficiency gain
    //2. If 2 corners share a closest point this resolves the conflict.
    corners.erase(corners.begin()+min_idx);
  }
}

vector<vector<bubble_val> > ProcessImage(string &imagefilename, string &bubblefilename, float &weight) {
  #if DEBUG > 0
  cout << "debug level is: " << DEBUG << endl;
  #endif
  string seglocfile("segment-offsets-tmp.txt");
  string buboffsetfile("bubble-offsets.txt");
  weight_param = weight;
  vector < Point2f > corners, segment_locations;
  vector<bubble_val> bubble_vals;
  vector<vector<bubble_val> > segment_results;
  Mat img, imgGrey, out, warped;
  imgfilename = imagefilename;

  // Read the input image
  img = imread(imagefilename);
  if (img.data == NULL) {
    return vector<vector<bubble_val> >();
  }

  #if DEBUG > 0
  cout << "converting to grayscale" << endl;
  #endif
  cvtColor(img, imgGrey, CV_RGB2GRAY);

  Mat straightened_image(3300 * SCALEPARAM, 2550 * SCALEPARAM, CV_8U);

  #if DEBUG > 0
  cout << "straightening image" << endl;
  #endif
  straightenImage(imgGrey, straightened_image);
  
  #if DEBUG > 0
  cout << "writing to output image" << endl;
  imwrite("straightened_" + imagefilename, straightened_image);
  #endif
  
  #if DEBUG > 0
  cout << "getting segment locations" << endl;
  #endif
  getSegmentLocations(segment_locations, seglocfile);

  #if DEBUG > 0
  cout << "creating and processing segment images" << endl;
  #endif
  for (vector<Point2f>::iterator it = segment_locations.begin();
       it != segment_locations.end(); it++) {
    Point2f loc((*it).x * SCALEPARAM, (*it).y * SCALEPARAM);
    Mat segment = getSegmentMat(straightened_image, loc);
    Mat aligned_segment(segment.rows - (SEGMENT_BUFFER * SCALEPARAM),
                        segment.cols - (SEGMENT_BUFFER * SCALEPARAM), CV_8UC1);
    #if DEBUG > 0
    cout << "aligning segment" << endl;
    #endif
    align_segment(segment, aligned_segment);
    segment_results.push_back(processSegment(aligned_segment, buboffsetfile));
    aligned_segment.copyTo(segment);

    #if DEBUG > 0
    string segfilename("marked_");
    stringstream ss;
    ss << loc.x;
    ss << loc.y;
    segfilename.append(ss.str());
    segfilename.append(".jpg");
    imwrite(segfilename, segment);
    #endif
  }

  return segment_results;
}

void align_segment(Mat& img, Mat& aligned_segment){
  vector < vector<Point> > contours;
  vector < vector<Point> > borderContours;
  vector < Point > approx;
  vector < Point > maxRect;
  
  //Threshold the image
  //Maybe we should dilate or blur or something first?
  //The ideal image would be black lines and white boxes with nothing in them
  //so if we can filter to get something closer to that, it is a good thing.
  Scalar my_mean;
  Scalar my_stddev;
  meanStdDev(img, my_mean, my_stddev);
  Mat imgThresh = img > (my_mean.val[0]-.05*my_stddev.val[0]);
  
  // Find all external contours of the image
  findContours(imgThresh, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);
  
  float maxContourArea = 0;
  // Iterate through all detected contours
  for (size_t i = 0; i < contours.size(); ++i) {
    // reduce the number of points in the contour
    approxPolyDP(Mat(contours[i]), approx,
                     arcLength(Mat(contours[i]), true) * 0.1, true);

    float area = fabs(contourArea(Mat(approx)));

    if (area > maxContourArea) {
      maxRect = approx;
      maxContourArea = area;
    }
    //Maybe I could refine this by using a corner detector and using
    //the 4 contour points with the highest responce?
  }
  if ( maxRect.size() == 4 && isContourConvex(Mat(maxRect)) && maxContourArea > (img.cols/2) * (img.rows/2)) {
    Point2f segment_corners[4] = {Point2f(0,0),Point2f(aligned_segment.cols,0),
      Point2f(0,aligned_segment.rows),Point2f(aligned_segment.cols,aligned_segment.rows)};
    Point2f corners_a[4] = {Point2f(0,0),Point2f(img.cols,0),
      Point2f(0,img.rows),Point2f(img.cols,img.rows)};
    
    configCornerArray(maxRect, corners_a, .1);
    Mat H = getPerspectiveTransform(corners_a , segment_corners);
    warpPerspective(img, aligned_segment, H, aligned_segment.size());
  }
  else{//use the bounding line method if the contour method fails
    int top = 0, bottom = 0, left = 0, right = 0;
      find_bounding_lines(img, &top, &bottom, false);
      find_bounding_lines(img, &left, &right, true);
      
    //debug stuff
    /*
    img.copyTo(aligned_segment);
    const Point* p = &maxRect[0];
    int n = (int) maxRect.size();
    polylines(aligned_segment, &p, &n, 1, true, 200, 2, CV_AA);
    
    img.row(top)+=200;
      img.row(bottom)+=200;
      img.col(left)+=200;
      img.col(right)+=200;
    img.copyTo(aligned_segment);
    */

      float bounding_lines_threshold = .2;
      if ((abs((bottom - top) - aligned_segment.rows) < bounding_lines_threshold * aligned_segment.rows) &&
        (abs((right - left) - aligned_segment.cols) < bounding_lines_threshold * aligned_segment.cols) &&
        top + aligned_segment.rows < img.rows &&  top + aligned_segment.cols < img.cols) {
        
        img(Rect(left, top, aligned_segment.cols, aligned_segment.rows)).copyTo(aligned_segment);
        
      }
      else{
        img(Rect(SEGMENT_BUFFER * SCALEPARAM, SEGMENT_BUFFER * SCALEPARAM,
                 aligned_segment.cols, aligned_segment.rows)).copyTo(aligned_segment);
      }
  }
}

vector<bubble_val> processSegment(Mat &segment, string bubble_offsets) {
  vector<Point2f> bubble_locations;
  vector<bubble_val> retvals;
  string line;
  float bubx, buby;
  ifstream offsets(bubble_offsets.c_str());

  if (offsets.is_open()) {
    while (getline(offsets, line)) {
      if (line.size() > 2) {
        stringstream ss(line);

        ss >> bubx;
        ss >> buby;
        Point2f bubble(bubx * SCALEPARAM, buby * SCALEPARAM);
        bubble_locations.push_back(bubble);
      }
    }
  }

  vector<Point2f>::iterator it;
  for (it = bubble_locations.begin(); it != bubble_locations.end(); it++) {
    bubble_val current_bubble = checkBubble(segment, *it);
    #if DEBUG > 0
    Scalar color(0, 0, 0);
    if (current_bubble == 1) {
      color = (255, 255, 255);
    }
    rectangle(segment, (*it)-Point2f(EXAMPLE_WIDTH/2,EXAMPLE_HEIGHT/2),
              (*it)+Point2f(EXAMPLE_WIDTH/2,EXAMPLE_HEIGHT/2), color);
    #endif
    retvals.push_back(current_bubble);
  }

  return retvals;
}

Mat getSegmentMat(Mat &img, Point2f &corner) {
  Mat segment;
  Point2f segcenter;
  segcenter = corner;
  segcenter.x += (SEGMENT_WIDTH*SCALEPARAM)/2;
  segcenter.y += (SEGMENT_HEIGHT*SCALEPARAM)/2;
  Size segsize((SEGMENT_WIDTH + SEGMENT_BUFFER) * SCALEPARAM,
               (SEGMENT_HEIGHT + SEGMENT_BUFFER) * SCALEPARAM);
  getRectSubPix(img, segsize, segcenter, segment);

  return segment;
}

void getSegmentLocations(vector<Point2f> &segmentcorners, string segfile) {
  string line;
  float segx = 0, segy = 0;

  ifstream segstream(segfile.c_str());
  if (segstream.is_open()) {
    while (getline(segstream, line)) {
      if (line.size() > 2) {
        stringstream ss(line);

        ss >> segx;
        ss >> segy;
        Point2f corner(segx, segy);
        #if DEBUG > 1
        cout << "adding segment corner " << corner << endl;
        #endif
        segmentcorners.push_back(corner);
      }
    }
  }
}

void find_bounding_lines(Mat& img, int* upper, int* lower, bool vertical) {
  int center_size = 20 * SCALEPARAM;
  Mat grad_img, out;
  Sobel(img, grad_img, 0, int(!vertical), int(vertical));
  //multiply(grad_img, img/100, grad_img);//seems to yield improvements on bright images
  reduce(grad_img, out, int(!vertical), CV_REDUCE_SUM, CV_32F);
  GaussianBlur(out, out, Size(1,(int)(3 * SCALEPARAM)), 1.0 * SCALEPARAM);

  if( vertical )
    transpose(out,out);

  Point min_location_top;
  Point min_location_bottom;
  minMaxLoc(out(Range(3, out.rows/2 - center_size), Range(0,1)), NULL,NULL,&min_location_top);
  minMaxLoc(out(Range(out.rows/2 + center_size,out.rows - 3), Range(0,1)), NULL,NULL,&min_location_bottom);
  *upper = min_location_top.y;
  *lower = min_location_bottom.y + out.rows/2 + center_size;
}

void straightenImage(const Mat& input_image, Mat& output_image) {
  #if DEBUG > 0
  cout << "entering StraightenImage" << endl;
  #endif
  Point2f orig_corners[4];
  Point2f corners_a[4];
  vector < Point2f > corners;

  Mat tmask, input_image_dilated;

  // Create a mask that limits corner detection to the corners of the image.
  tmask= Mat::zeros(input_image.rows, input_image.cols, CV_8U);
  circle(tmask, Point(0,0), (tmask.cols+tmask.rows)/8, Scalar(255,255,255), -1);
  circle(tmask, Point(0,tmask.rows), (tmask.cols+tmask.rows)/8, Scalar(255,255,255), -1);
  circle(tmask, Point(tmask.cols,0), (tmask.cols+tmask.rows)/8, Scalar(255,255,255), -1);
  circle(tmask, Point(tmask.cols,tmask.rows), (tmask.cols+tmask.rows)/8, Scalar(255,255,255), -1);

  //orig_corners = {Point(0,0),Point(img.cols,0),Point(0,img.rows),Point(img.cols,img.rows)};
  orig_corners[0] = Point(0,0);
  orig_corners[1] = Point(output_image.cols,0);
  orig_corners[2] = Point(0,output_image.rows);
  orig_corners[3] = Point(output_image.cols,output_image.rows);

  #if DEBUG > 0
  cout << "dilating image" << endl;
  #endif
  // Dilating reduces noise, thin lines and small marks.
  dilate(input_image, input_image_dilated, Mat(), Point(-1, -1), DILATION);

  /*
  Params for goodFeaturesToTrack:
  Source Mat, Dest Mat
  Number of features/interest points to return
  Minimum feature quality
  Min distance between corners (Probably needs parameterization depending on im. res. and form)
  Mask
  Block Size (not totally sure but I think it's similar to aperture)
  Use Harris detector (true) or cornerMinEigenVal() (false)
  Free parameter of Harris detector
  */
  #if DEBUG > 0
  cout << "finding corners of the paper" << endl;
  #endif
  goodFeaturesToTrack(input_image_dilated, corners, 4, 0.01, DIST_PARAM, tmask, BLOCK_SIZE, false, 0.04);

  // Initialize the value of corners_a to that of orig_corners
  memcpy(corners_a, orig_corners, sizeof(orig_corners));
  configCornerArray(corners, corners_a);
  
  Mat H = getPerspectiveTransform(corners_a , orig_corners);
  #if DEBUG > 0
  cout << "resizing and warping" << endl;
  #endif
  warpPerspective(input_image, output_image, H, output_image.size());

  #if DEBUG > 0
  cout << "exiting StraightenImage" << endl;
  #endif
}

/*
Takes a vector of corners and converts it into a properly formatted corner array.
Warning: destroys the corners vector in the process.
*/
void configCornerArray(vector<Point2f>& corners, Point2f* corners_a){
  #if DEBUG > 0
  cout << "in configCornerArray" << endl;
  #endif
  float min_dist;
  int min_idx;
  float dist;
  
  //Make sure the form corners map to the correct image corner
  //by snaping the nearest form corner to each image corner.
  for(int i = 0; i < 4; i++) {
    min_dist = FLT_MAX;
    for(int j = 0; j < corners.size(); j++ ){
      dist = norm(corners[j]-corners_a[i]);
      if(dist < min_dist){
        min_dist = dist;
        min_idx = j;
      }
    }
    corners_a[i]=corners[min_idx];
    //Two relatively minor reasons for doing this,
    //1. Small efficiency gain
    //2. If 2 corners share a closest point this resolves the conflict.
    corners.erase(corners.begin()+min_idx);
  }

  #if DEBUG > 0
  cout << "exiting configCornerArray" << endl;
  #endif
}

//Rate a location on how likely it is to be a bubble
double rateBubble(Mat& det_img_gray, Point bubble_location) {
    Mat query_pixels, pca_components;
    getRectSubPix(det_img_gray, Size(EXAMPLE_WIDTH,EXAMPLE_HEIGHT), bubble_location, query_pixels);
    query_pixels.reshape(0,1).convertTo(query_pixels, CV_32F);
    pca_components = my_PCA.project(query_pixels);
    //The rating is the SSD of query pixels and their back projection
    Mat out = my_PCA.backProject(pca_components)- query_pixels;
    return sum(out.mul(out)).val[0];
}

//Compare the bubbles with all the bubbles used in the classifier.
bubble_val checkBubble(Mat& det_img_gray, Point bubble_location) {
    Mat query_pixels;
    //This bit of code finds the location in the search_window most likely to be a bubble
    //then it checks that rather than the exact specified location.
    Mat out = Mat::zeros(Size(search_window.y*2, search_window.x*2) , CV_32FC1);
    Point offset = Point(bubble_location.x - search_window.x, bubble_location.y - search_window.y);
    for(size_t i = 0; i < search_window.y*2; i+=1) {
        for(size_t j = 0; j < search_window.x*2; j+=1) {
          /*cout << "accessing row " << i << " and column " << j << endl;
          cout << "max row is " << out.rows << " and max column is " << out.cols << endl << endl;*/
          out.row(j).col(i) += rateBubble(det_img_gray, Point(i,j) + offset);
        }
    }
    Point min_location;
    minMaxLoc(out, NULL,NULL, &min_location);
    circle(det_img_gray, min_location+offset, 1, Scalar(255), -1);

    getRectSubPix(det_img_gray, Point(EXAMPLE_WIDTH,EXAMPLE_HEIGHT), min_location + offset, query_pixels);

    query_pixels.reshape(0,1).convertTo(query_pixels, CV_32F);
   
    Mat responce;
    matchTemplate(comparison_vectors, my_PCA.project(query_pixels), responce, CV_TM_CCOEFF_NORMED);
   
    //Here we find the best match in our PCA training set with weighting applied.
    reduce(responce, out, 1, CV_REDUCE_MAX);
    int max_idx = -1;
    float max_responce = 0;
    for(size_t i = 0; i < training_bubble_values.size(); i+=1) {
        float current_responce = sum(out.row(i)).val[0];
        switch( training_bubble_values[i] ){
            case FILLED_BUBBLE:
                current_responce *= weight_param;
                break;
            case EMPTY_BUBBLE:
                current_responce *= (1 - weight_param);
                break;
        }
        if(current_responce > max_responce){
            max_idx = i;
            max_responce = current_responce;
        }
    }

    return training_bubble_values[max_idx];
}

void train_PCA_classifier() {

 // Set training_bubble_values here
  training_bubble_values.push_back(FILLED_BUBBLE);
  training_bubble_values.push_back(EMPTY_BUBBLE);
  training_bubble_values.push_back(FILLED_BUBBLE);
  training_bubble_values.push_back(EMPTY_BUBBLE);
  training_bubble_values.push_back(FILLED_BUBBLE);
  training_bubble_values.push_back(EMPTY_BUBBLE);
  training_bubble_values.push_back(FILLED_BUBBLE);
  training_bubble_values.push_back(FILLED_BUBBLE);
  training_bubble_values.push_back(EMPTY_BUBBLE);
  training_bubble_values.push_back(FILLED_BUBBLE);
  training_bubble_values.push_back(FILLED_BUBBLE);

  Mat example_strip = imread("example_strip.jpg");
  Mat example_strip_bw;
  cvtColor(example_strip, example_strip_bw, CV_RGB2GRAY);

  int numexamples = example_strip_bw.cols / EXAMPLE_WIDTH;
  Mat PCA_set = Mat::zeros(numexamples, EXAMPLE_HEIGHT*EXAMPLE_WIDTH, CV_32F);

  for (int i = 0; i < numexamples; i++) {
    Mat PCA_set_row = example_strip_bw(Rect(i * EXAMPLE_WIDTH, 0,
                                            EXAMPLE_WIDTH, EXAMPLE_HEIGHT));
    PCA_set_row.convertTo(PCA_set_row, CV_32F);
    PCA_set.row(i) += PCA_set_row.reshape(0,1);
  }

  my_PCA = PCA(PCA_set, Mat(), CV_PCA_DATA_AS_ROW, EIGENBUBBLES);
  comparison_vectors = my_PCA.project(PCA_set);
}