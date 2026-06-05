#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
 
using namespace cv;
using namespace std;
 
// НАСТРАИВАЕМЫЕ ПАРАМЕТРЫ
const float MIN_GAP_RATIO = 0.35f; // 0.35 – мягкая фильтрация (0.5 была сильная)
const float GRADIENT_THRESH = 0.4f; // порог градиента для поиска линий
const int MERGE_DISTANCE = 15; // объединение близких пиков (пикселей)
 
Mat original;
Point2f top_point, bottom_point;
bool top_selected = false, bottom_selected = false;
 
void mouseCallback(int event, int x, int y, int flags, void* userdata) {
if (event == EVENT_LBUTTONDOWN) {
if (!top_selected) {
top_point = Point2f(x, y);
top_selected = true;
cout << "Top of building: (" << x << ", " << y << ")" << endl;
}
else if (!bottom_selected) {
bottom_point = Point2f(x, y);
bottom_selected = true;
cout << "Bottom of building: (" << x << ", " << y << ")" << endl;
}
}
}
 
Mat extractFacadeStrip(const Mat& img, Point2f top, Point2f bottom) {
float x_center = (top.x + bottom.x) / 2;
float height = abs(bottom.y - top.y);
float width = height * 0.6f;
int left = max(0, (int)(x_center - width/2));
int right = min(img.cols - 1, (int)(x_center + width/2));
int topY = min(top.y, bottom.y);
int bottomY = max(top.y, bottom.y);
Rect roi(left, topY, right - left, bottomY - topY);
return img(roi).clone();
}
 
vector<int> findPeakLines(const Mat& facadeStrip, float gradientThreshold, int mergeDistance) {
Mat gray;
cvtColor(facadeStrip, gray, COLOR_BGR2GRAY);
Mat gradY;
Sobel(gray, gradY, CV_32F, 0, 1, 3);
Mat absGradY;
convertScaleAbs(gradY, absGradY);
 
vector<float> verticalProfile(absGradY.rows, 0);
for (int i = 0; i < absGradY.rows; i++) {
verticalProfile[i] = sum(absGradY.row(i))[0] / absGradY.cols;
}
 
float thresh = *max_element(verticalProfile.begin(), verticalProfile.end()) * gradientThreshold;
vector<int> peaks;
for (size_t i = 2; i < verticalProfile.size() - 2; i++) {
if (verticalProfile[i] > thresh &&
verticalProfile[i] > verticalProfile[i-1] &&
verticalProfile[i] > verticalProfile[i+1]) {
peaks.push_back(i);
}
}
 
// Слияние близких пиков
vector<int> merged;
for (size_t i = 0; i < peaks.size(); i++) {
if (merged.empty() || peaks[i] - merged.back() > mergeDistance) {
merged.push_back(peaks[i]);
} else {
int sum = merged.back();
int cnt = 1;
while (i+1 < peaks.size() && peaks[i+1] - peaks[i] < mergeDistance) {
sum += peaks[++i];
cnt++;
}
merged.back() = sum / cnt;
}
}
return merged;
}
 
vector<int> filterLinesByDistance(const vector<int>& lines, float minGapRatio) {
if (lines.size() < 2) return lines;
 
// Вычисляем все расстояния между соседними линиями
vector<int> gaps;
for (size_t i = 1; i < lines.size(); i++) {
gaps.push_back(lines[i] - lines[i-1]);
}
// Находим медиану расстояний
sort(gaps.begin(), gaps.end());
int medianGap = gaps[gaps.size() / 2];
int minGap = static_cast<int>(medianGap * minGapRatio);
 
vector<int> filtered;
filtered.push_back(lines[0]);
for (size_t i = 1; i < lines.size(); i++) {
if (lines[i] - filtered.back() >= minGap) {
filtered.push_back(lines[i]);
}
}
return filtered;
}
 
int countFloorsFromLines(const vector<int>& lines, int totalHeight) {
if (lines.size() < 2) return 1;
 
// Обрезаем линии, слишком близкие к краям
int margin = totalHeight * 0.05;
vector<int> validLines;
for (int y : lines) {
if (y > margin && y < totalHeight - margin) {
validLines.push_back(y);
}
}
 
if (validLines.size() < 2) return 1;
 
// Фильтруем по расстоянию
vector<int> filtered = filterLinesByDistance(validLines, MIN_GAP_RATIO);
 
// Количество промежутков между линиями
int gapsCount = filtered.size() - 1;
// Каждый этаж даёт 2 промежутка (верхняя и нижняя граница)
int floors = (gapsCount + 1) / 2;
return max(1, floors);
}
 
int countFloorsByGradient(const Mat& facadeStrip) {
vector<int> lines = findPeakLines(facadeStrip, GRADIENT_THRESH, MERGE_DISTANCE);
return countFloorsFromLines(lines, facadeStrip.rows);
}
 
void drawResult(Mat& img, Point2f top, Point2f bottom, int floors) {
rectangle(img, Point(top.x-20, top.y), Point(bottom.x+20, bottom.y), Scalar(0,255,0), 2);
putText(img, "Floors: " + to_string(floors), Point(top.x-20, top.y-10),
FONT_HERSHEY_SIMPLEX, 1.2, Scalar(0,0,255), 3);
 
Mat facade = extractFacadeStrip(original, top, bottom);
vector<int> lines = findPeakLines(facade, GRADIENT_THRESH, MERGE_DISTANCE);
vector<int> filteredLines = filterLinesByDistance(lines, MIN_GAP_RATIO);
 
// Отсекаем слишком близкие к краю
int margin = facade.rows * 0.05;
vector<int> validLines;
for (int y : filteredLines) {
if (y > margin && y < facade.rows - margin) {
validLines.push_back(y);
}
}
 
int left = max(0, (int)((top.x+bottom.x)/2 - facade.cols/2));
int topY = min(top.y, bottom.y);
for (int y : validLines) {
line(img, Point(left, topY + y), Point(left + facade.cols, topY + y), Scalar(255,255,0), 2);
}
}
 
int main() {
original = imread("building16.jpg");
if (original.empty()) {
cout << "Error: cannot load building2.jpg" << endl;
return -1;
}
 
namedWindow("Select top and bottom", WINDOW_NORMAL);
setMouseCallback("Select top and bottom", mouseCallback);
 
Mat display = original.clone();
imshow("Select top and bottom", display);
cout << "Click on the TOP of the building, then click on the BOTTOM." << endl;
waitKey(0);
 
if (!top_selected || !bottom_selected) {
cout << "Error: you must select both points." << endl;
return -1;
}
 
Mat facadeStrip = extractFacadeStrip(original, top_point, bottom_point);
if (facadeStrip.empty()) {
cout << "Error: invalid region" << endl;
return -1;
}
 
imshow("Facade strip", facadeStrip);
 
int floors = countFloorsByGradient(facadeStrip);
cout << "\n*** Estimated floors: " << floors << " ***" << endl;
 
Mat result = original.clone();
drawResult(result, top_point, bottom_point, floors);
imshow("Result", result);
 
waitKey(0);
return 0;
} 
