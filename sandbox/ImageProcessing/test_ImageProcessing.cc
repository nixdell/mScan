#include <iostream>
#include <fstream>
#include <string>
#include "./ImageProcessing.h"

#define DEBUG 0

using namespace std;

void check_values(vector<bubble_val> &found, int *tpos, int *tneg, int *fpos, int *fneg);

int main(int argc, char *argv[]) {
  // teach the program what empty and filled bubbles look like
  train_PCA_classifier();

  // image to be processed
  string image("vr.jpg");

  // bubble location file
  string bubbles("bubble-locations.full2");
  float i;

  // testing loop, currently iterates over weight_param
  for (i = 0.00; i <= 1.00; i += 0.01) {
    // process the image with the given parameter value
    vector<bubble_val> bubble_vals = ProcessImage(image, bubbles, i);

    // get true positive, false positive, true negative, and false negative data
    int tpos = 0, tneg = 0, fpos = 0, fneg = 0;
    check_values(bubble_vals, &tpos, &tneg, &fpos, &fneg);

    // print out results to console, >> filename to save to file
    std::cout << "parameter_name, parameter_value, true_positives, ";
    std::cout << "false_positives, true_negatives, false_negatives" << std::endl;
    std::cout << "\"weight parameter\", " << i << ", ";
    std::cout << tpos << ", " << fpos << ", ";
    std::cout << tneg << ", " << fneg << std::endl;
  }
}

void check_values(vector<bubble_val> &found, int *tpos, int *tneg, int *fpos, int *fneg) {
  vector<int> actual;
  string line;
  int bubble, i;

  // file with actual bubble values, format:
  //   val val val val val val\n
  //   val val val val val val\n
  // etc, example:
  //   0 0 0 0 0 1 0 0 0 0 0 0 0
  // reads from left to right within row, top to bottom within segment
  string valfile("bubble-vals");

  // read in bubble values
  ifstream bubble_value_file(valfile.c_str());
  if (bubble_value_file.is_open()) {
    while (getline(bubble_value_file, line)) {
      stringstream ss(line);

      while (ss.good()) {
        ss >> bubble;
        actual.push_back(bubble);
      }
    }
  }

  for (i = 0; i < actual.size(); i++) {
    // true positive
    if (found[i] == 1 && actual[i] == 1) {
      (*tpos)++;
    }
    // true negative
    else if (found[i] == 0 && actual[i] == 0) {
      (*tneg)++;
    }
    // false positive
    else if (found[i] == 1 && actual[i] == 0) {
      (*fpos)++;
    }
    // false negative
    else if (found[i] == 0 && actual[i] == 1) {
      (*fneg)++;
    }
  }
}
