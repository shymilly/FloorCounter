#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <filesystem>

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

using namespace std;
using namespace cv;

struct Result {
    string filename;
    int expectedFloors;
    int detectedFloors;
    double timeMs;
    int detectedPeriodPx;
    float peakCorrValue;
    int buildingHeightPx;
    bool success;
    string paramsUsed;
};

vector<float> computeVerticalProfile(const Mat& gray, int stripWidthPercent = 20) {
    int height = gray.rows;
    int width = gray.cols;
    int stripWidth = max(1, width * stripWidthPercent / 100);
    int startX = (width - stripWidth) / 2;
    Rect roi(startX, 0, stripWidth, height);
    Mat strip = gray(roi);
    vector<float> profile(height);
    for (int y = 0; y < height; ++y) {
        profile[y] = static_cast<float>(mean(strip.row(y))[0]);
    }
    return profile;
}

vector<float> normalizeSignal(const vector<float>& signal) {
    float mean = accumulate(signal.begin(), signal.end(), 0.0f) / signal.size();
    vector<float> normalized(signal.size());
    for (size_t i = 0; i < signal.size(); ++i) {
        normalized[i] = signal[i] - mean;
    }
    return normalized;
}

vector<float> computeAutocorrelation(const vector<float>& signal, int maxLag) {
    int n = signal.size();
    vector<float> normSignal = normalizeSignal(signal);
    vector<float> corr(maxLag + 1, 0.0f);
    for (int lag = 1; lag <= maxLag && lag < n; ++lag) {
        float sum = 0.0f;
        int count = 0;
        for (int i = 0; i < n - lag; ++i) {
            sum += normSignal[i] * normSignal[i + lag];
            count++;
        }
        if (count > 0) corr[lag] = sum / count;
    }
    float maxVal = 0;
    for (int lag = 1; lag <= maxLag; ++lag) {
        if (corr[lag] > maxVal) maxVal = corr[lag];
    }
    if (maxVal > 1e-6f) {
        for (int lag = 1; lag <= maxLag; ++lag) {
            corr[lag] /= maxVal;
        }
    }
    return corr;
}

int findDominantPeriod(const vector<float>& corr, int minPeriod, int maxPeriod, float peakThreshold = 0.5f) {
    int bestPeriod = minPeriod;
    float bestPeak = 0.0f;
    for (int period = minPeriod; period <= maxPeriod && period < (int)corr.size(); ++period) {
        if (corr[period] > peakThreshold && corr[period] > bestPeak) {
            bool isPeak = true;
            if (period > minPeriod && corr[period] <= corr[period - 1]) isPeak = false;
            if (period < maxPeriod - 1 && corr[period] <= corr[period + 1]) isPeak = false;
            if (isPeak) {
                bestPeak = corr[period];
                bestPeriod = period;
            }
        }
    }
    return bestPeriod;
}

Result processImage(const Mat& img, const string& filename, int expectedFloors,
                    int stripWidthPercent = 20, int minPeriodPx = 25, int maxPeriodPx = 80,
                    float peakThreshold = 0.5f, bool autoCropBuilding = true) {
    auto start = chrono::high_resolution_clock::now();
    Result res;
    res.filename = filename;
    res.expectedFloors = expectedFloors;
    res.success = false;
    
    Mat gray;
    if (img.channels() == 3) cvtColor(img, gray, COLOR_BGR2GRAY);
    else gray = img.clone();
    
    Mat workingImg = gray.clone();
    if (autoCropBuilding) {
        int height = workingImg.rows;
        workingImg = workingImg(Rect(0, height/10, workingImg.cols, height - height/5));
    }
    
    int height = workingImg.rows;
    res.buildingHeightPx = height;
    
    vector<float> profile = computeVerticalProfile(workingImg, stripWidthPercent);
    vector<float> corr = computeAutocorrelation(profile, maxPeriodPx);
    int periodPx = findDominantPeriod(corr, minPeriodPx, maxPeriodPx, peakThreshold);
    
    res.detectedPeriodPx = periodPx;
    res.peakCorrValue = (periodPx < (int)corr.size()) ? corr[periodPx] : 0.0f;
    
    if (periodPx > 5) {
        int rawFloors = max(1, height / periodPx);
        res.detectedFloors = min(rawFloors, 50);
        res.success = true;
    } else {
        res.detectedFloors = -1;
    }
    
    char params[200];
    sprintf(params, "strip=%d%%, period=[%d,%d], thresh=%.1f", stripWidthPercent, minPeriodPx, maxPeriodPx, peakThreshold);
    res.paramsUsed = params;
    
    auto end = chrono::high_resolution_clock::now();
    res.timeMs = chrono::duration<double, milli>(end - start).count();
    return res;
}

void createSyntheticImage(int floors, const string& filename) {
    int floorHeight = 45;
    int buildingWidth = 220;
    int imageWidth = 550;
    int buildingHeight = floors * floorHeight + 70;
    int imageHeight = buildingHeight + 100;
    
    Mat img(imageHeight, imageWidth, CV_8UC3, Scalar(135, 206, 235));
    rectangle(img, Point(0, imageHeight - 50), Point(imageWidth, imageHeight), Scalar(34, 139, 34), FILLED);
    
    int buildingX = (imageWidth - buildingWidth) / 2;
    int buildingY = 50;
    rectangle(img, Point(buildingX, buildingY), Point(buildingX + buildingWidth, buildingY + buildingHeight), Scalar(180, 180, 180), FILLED);
    
    vector<Point> roof = {Point(buildingX - 10, buildingY), Point(buildingX + buildingWidth/2, buildingY - 30), Point(buildingX + buildingWidth + 10, buildingY)};
    fillPoly(img, vector<vector<Point>>{roof}, Scalar(100, 100, 100));
    
    int windowWidth = buildingWidth / 4;
    int windowHeight = floorHeight - 8;
    for (int f = 0; f < floors; ++f) {
        int y = buildingY + 12 + f * floorHeight;
        for (int w = 0; w < 3; ++w) {
            int x = buildingX + 20 + w * (windowWidth + 15);
            if (x + windowWidth < buildingX + buildingWidth - 10) {
                rectangle(img, Point(x, y), Point(x + windowWidth, y + windowHeight), Scalar(100, 150, 200), FILLED);
                rectangle(img, Point(x, y), Point(x + windowWidth, y + windowHeight), Scalar(80, 80, 80), 1);
            }
        }
    }
    
    rectangle(img, Point(buildingX + buildingWidth/2 - 35, buildingY + buildingHeight - 55), Point(buildingX + buildingWidth/2 + 35, buildingY + buildingHeight), Scalar(100, 80, 60), FILLED);
    putText(img, to_string(floors) + " floors", Point(buildingX + 10, buildingY + buildingHeight - 15), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 0, 0), 2);
    
    imwrite(filename, img);
    cout << "  Created: " << filename << " (" << floors << " floors)" << endl;
}

int main() {
    cout << "\n==================================================" << endl;
    cout << "   FLOOR COUNTER - Autocorrelation Algorithm" << endl;
    cout << "==================================================\n" << endl;
    
    vector<int> testFloors = {3, 5, 8, 10, 12, 15};
    vector<string> testImages;
    
    cout << "Creating synthetic test images..." << endl;
    for (int floors : testFloors) {
        string filename = "building_" + to_string(floors) + "f.png";
        createSyntheticImage(floors, filename);
        testImages.push_back(filename);
    }
    
    cout << "\n--- Testing algorithm ---\n" << endl;
    
    vector<tuple<int, int, int, float>> paramSets = {
        {20, 25, 70, 0.5f}, {15, 25, 70, 0.5f}, {30, 25, 70, 0.5f},
        {20, 20, 60, 0.5f}, {20, 30, 90, 0.5f}, {20, 25, 70, 0.7f},
        {20, 25, 70, 0.3f}, {25, 25, 75, 0.5f}, {35, 25, 80, 0.5f},
        {20, 22, 68, 0.55f}, {15, 28, 72, 0.45f}, {20, 30, 80, 0.6f}
    };
    
    for (const auto& imgFile : testImages) {
        Mat img = imread(imgFile);
        if (img.empty()) {
            cout << "ERROR: Cannot load " << imgFile << endl;
            continue;
        }
        
        cout << "Image: " << imgFile << endl;
        for (const auto& params : paramSets) {
            int strip = get<0>(params), minP = get<1>(params), maxP = get<2>(params);
            float thresh = get<3>(params);
            Result r = processImage(img, imgFile, stoi(imgFile.substr(imgFile.find('_')+1, imgFile.find('f')-imgFile.find('_')-1)),
                                    strip, minP, maxP, thresh, true);
            cout << "  [" << strip << "% " << minP << "-" << maxP << " @" << thresh << "] -> "
                 << r.detectedFloors << " floors, time=" << r.timeMs << "ms "
                 << (r.detectedFloors == r.expectedFloors ? "✓" : "✗") << endl;
        }
        cout << endl;
    }
    
    cout << "Done! Check generated .png files." << endl;
    return 0;
}


