// #include <iostream>
// #include <chrono>
// #include <numeric>
// #include <filesystem>
// #include <map>
// #include <vector>
// #include <algorithm>
// #include <cmath>
// #include <opencv2/opencv.hpp>
// #include <opencv2/core.hpp>
// #include <opencv2/imgproc.hpp>
// #include <opencv2/highgui.hpp>

// using namespace std;
// using namespace cv;

// // ================================
// // НАСТРАИВАЕМЫЕ ПАРАМЕТРЫ АЛГОРИТМА
// // ================================

// struct AlgorithmConfig {
//     // Параметры предобработки
//     int gaussianBlurSize = 5;           // Размер ядра размытия (нечетное, 3-9)
//     double claheClipLimit = 2.0;        // CLAHE ограничение контраста (1-4)
//     int claheGridSize = 8;              // CLAHE размер сетки (4-16)
//     bool useCLAHE = true;               // Использовать CLAHE вместо equalizeHist
    
//     // Параметры вертикального профиля
//     int stripWidthPercent = 30;         // Ширина вертикальной полосы (10-50%)
//     int numVerticalBands = 5;           // Количество вертикальных полос (3-7)
//     int profileSmoothingWindow = 7;     // Сглаживание профиля (5-15)
//     bool useMultiScale = true;          // Использовать многомасштабный анализ
    
//     // Параметры автокорреляции
//     int minPeriodPx = 35;               // Минимальная высота этажа (25-50)
//     int maxPeriodPx = 100;              // Максимальная высота этажа (70-150)
//     float peakThreshold = 0.25f;        // Порог для пиков (0.15-0.45)
//     int peakNeighborhood = 3;            // Окрестность для поиска пиков (2-5)
//     int maxLagPercent = 50;              // Максимальный лаг в % от высоты (30-60)
    
//     // Параметры оценки этажей
//     int minFloors = 1;                  // Минимальное количество этажей
//     int maxFloors = 25;                 // Максимальное количество этажей
//     bool harmonicCorrection = true;     // Коррекция гармоник (удвоение/уполовинивание)
//     float confidenceMinThreshold = 0.2f; // Минимальная уверенность для принятия (0.15-0.35)
    
//     // Параметры валидации
//     bool useWindowValidation = true;    // Использовать валидацию по окнам
//     int windowValidationCannyLow = 50;  // Canny нижний порог
//     int windowValidationCannyHigh = 150; // Canny верхний порог
    
//     // Параметры фильтрации
//     bool autoDetectBuilding = true;     // Автообнаружение границ здания
//     int buildingCropTopPercent = 5;     // Обрезка сверху (% от высоты)
//     int buildingCropBottomPercent = 5;  // Обрезка снизу (% от высоты)
    
//     // Параметры визуализации
//     bool showVisualization = true;      // Показывать окна с визуализацией
//     int saveVisualization = false;       // Сохранять визуализацию в файл
// };

// // ================================
// // СТРУКТУРА РЕЗУЛЬТАТА
// // ================================
// struct BuildingResult {
//     string filename;
//     int detectedFloors;
//     double timeMs;
//     int confidence;
//     int periodPx;
//     int buildingHeightPx;
//     double peakCorrelation;
//     string methodUsed;
//     vector<int> candidateFloors;
//     vector<int> candidatePeriods;
//     vector<double> candidateScores;
    
//     // Данные для визуализации
//     Mat originalImage;
//     Mat processedImage;
//     Mat buildingImage;
//     vector<float> profile;
//     vector<float> smoothedProfile;
//     vector<float> autocorrelation;
//     vector<int> peaks;
//     Rect buildingRect;
// };

// // ================================
// // 1. ПРЕДОБРАБОТКА ИЗОБРАЖЕНИЯ
// // ================================
// class ImagePreprocessor {
// public:
//     Mat process(const Mat& src, const AlgorithmConfig& config) {
//         Mat result;
        
//         if (src.channels() == 3) {
//             cvtColor(src, result, COLOR_BGR2GRAY);
//         } else {
//             result = src.clone();
//         }
        
//         if (config.gaussianBlurSize > 1) {
//             GaussianBlur(result, result, Size(config.gaussianBlurSize, config.gaussianBlurSize), 0);
//         }
        
//         if (config.useCLAHE) {
//             Ptr<CLAHE> clahe = createCLAHE(config.claheClipLimit, Size(config.claheGridSize, config.claheGridSize));
//             clahe->apply(result, result);
//         } else {
//             equalizeHist(result, result);
//         }
        
//         return result;
//     }
// };

// // ================================
// // 2. ОПРЕДЕЛЕНИЕ ГРАНИЦ ЗДАНИЯ
// // ================================
// class BuildingDetector {
// public:
//     Rect detectBuilding(const Mat& gray, const AlgorithmConfig& config) {
//         if (!config.autoDetectBuilding) {
//             int height = gray.rows;
//             int cropTop = height * config.buildingCropTopPercent / 100;
//             int cropBottom = height * config.buildingCropBottomPercent / 100;
//             return Rect(0, cropTop, gray.cols, height - cropTop - cropBottom);
//         }
        
//         int height = gray.rows;
//         int width = gray.cols;
        
//         vector<int> horizontalProjection(height, 0);
//         for (int y = 0; y < height; ++y) {
//             for (int x = 0; x < width; ++x) {
//                 if (gray.at<uchar>(y, x) > 128) {
//                     horizontalProjection[y]++;
//                 }
//             }
//         }
        
//         int maxProj = *max_element(horizontalProjection.begin(), horizontalProjection.end());
//         int thresholdProj = maxProj * 0.05;
        
//         int topBound = 0;
//         int bottomBound = height - 1;
        
//         for (int y = 0; y < height; ++y) {
//             if (horizontalProjection[y] > thresholdProj) {
//                 topBound = y;
//                 break;
//             }
//         }
        
//         for (int y = height - 1; y >= 0; --y) {
//             if (horizontalProjection[y] > thresholdProj) {
//                 bottomBound = y;
//                 break;
//             }
//         }
        
//         topBound = std::max(0, topBound - height / 20);
//         bottomBound = std::min(height - 1, bottomBound + height / 20);
        
//         return Rect(0, topBound, width, bottomBound - topBound);
//     }
// };

// // ================================
// // 3. ВЕРТИКАЛЬНЫЙ ПРОФИЛЬ
// // ================================
// class VerticalProfileAnalyzer {
// public:
//     vector<float> computeProfile(const Mat& gray, const Rect& buildingRect, const AlgorithmConfig& config) {
//         Mat building = gray(buildingRect);
//         int height = building.rows;
//         int width = building.cols;
        
//         vector<float> profile(height, 0.0f);
        
//         int bandWidth = width * config.stripWidthPercent / 100;
//         bandWidth = std::max(10, std::min(bandWidth, width / 2));
        
//         int numBands = config.numVerticalBands;
//         vector<int> startPositions;
        
//         if (numBands == 1) {
//             startPositions = {(width - bandWidth) / 2};
//         } else {
//             int step = (width - bandWidth) / (numBands - 1);
//             for (int i = 0; i < numBands; ++i) {
//                 startPositions.push_back(i * step);
//             }
//         }
        
//         for (int startX : startPositions) {
//             for (int y = 0; y < height; ++y) {
//                 float sum = 0;
//                 int count = 0;
//                 for (int x = startX; x < startX + bandWidth && x < width; ++x) {
//                     sum += building.at<uchar>(y, x);
//                     count++;
//                 }
//                 if (count > 0) {
//                     profile[y] += sum / count;
//                 }
//             }
//         }
        
//         for (int y = 0; y < height; ++y) {
//             profile[y] /= startPositions.size();
//         }
        
//         if (config.useMultiScale) {
//             vector<float> multiScaleProfile(height, 0.0f);
//             vector<int> scales = {5, 10, 15, 20};
            
//             for (int scale : scales) {
//                 if (height / scale < 10) continue;
                
//                 Mat scaled;
//                 resize(building, scaled, Size(width / scale, height / scale));
//                 resize(scaled, scaled, building.size());
                
//                 for (int y = 0; y < height; ++y) {
//                     float sum = 0;
//                     for (int x = 0; x < width; ++x) {
//                         sum += scaled.at<uchar>(y, x);
//                     }
//                     multiScaleProfile[y] += sum / width;
//                 }
//             }
            
//             if (!multiScaleProfile.empty()) {
//                 for (int y = 0; y < height; ++y) {
//                     profile[y] = (profile[y] + multiScaleProfile[y] / scales.size()) / 2;
//                 }
//             }
//         }
        
//         return profile;
//     }
    
//     vector<float> smoothProfile(const vector<float>& profile, const AlgorithmConfig& config) {
//         vector<float> smoothed(profile.size());
//         int window = config.profileSmoothingWindow;
//         int half = window / 2;
        
//         for (size_t i = 0; i < profile.size(); ++i) {
//             float sum = 0;
//             int count = 0;
//             for (int j = -half; j <= half; ++j) {
//                 int idx = i + j;
//                 if (idx >= 0 && idx < (int)profile.size()) {
//                     sum += profile[idx];
//                     count++;
//                 }
//             }
//             smoothed[i] = sum / count;
//         }
        
//         return smoothed;
//     }
// };

// // ================================
// // 4. СПЕКТРАЛЬНЫЙ АНАЛИЗ
// // ================================
// class SpectralAnalyzer {
// public:
//     vector<float> normalizeSignal(const vector<float>& signal) {
//         float mean = accumulate(signal.begin(), signal.end(), 0.0f) / signal.size();
//         float stddev = 0;
//         for (float v : signal) {
//             stddev += (v - mean) * (v - mean);
//         }
//         stddev = sqrt(stddev / signal.size());
        
//         vector<float> normalized(signal.size());
//         if (stddev > 1e-6f) {
//             for (size_t i = 0; i < signal.size(); ++i) {
//                 normalized[i] = (signal[i] - mean) / stddev;
//             }
//         }
//         return normalized;
//     }
    
//     vector<float> computeAutocorrelation(const vector<float>& signal, int maxLag) {
//         int n = signal.size();
//         vector<float> normSignal = normalizeSignal(signal);
//         vector<float> autocorr(maxLag + 1, 0.0f);
        
//         for (int lag = 1; lag <= maxLag && lag < n; ++lag) {
//             float sum = 0;
//             int count = 0;
//             for (int i = 0; i < n - lag; ++i) {
//                 sum += normSignal[i] * normSignal[i + lag];
//                 count++;
//             }
//             if (count > 0) {
//                 autocorr[lag] = sum / count;
//             }
//         }
        
//         return autocorr;
//     }
    
//     vector<int> findPeaks(const vector<float>& data, int minPeriod, int maxPeriod, 
//                            float threshold, int neighborhood, double& bestValue) {
//         vector<int> peaks;
//         bestValue = 0;
        
//         for (int i = minPeriod; i <= maxPeriod && i < (int)data.size(); ++i) {
//             if (data[i] > threshold) {
//                 bool isPeak = true;
//                 for (int d = -neighborhood; d <= neighborhood; ++d) {
//                     if (d != 0 && i + d >= minPeriod && i + d <= maxPeriod && i + d < (int)data.size()) {
//                         if (data[i + d] >= data[i]) {
//                             isPeak = false;
//                             break;
//                         }
//                     }
//                 }
//                 if (isPeak) {
//                     peaks.push_back(i);
//                     if (data[i] > bestValue) {
//                         bestValue = data[i];
//                     }
//                 }
//             }
//         }
        
//         return peaks;
//     }
// };

// // ================================
// // 5. ВИЗУАЛИЗАЦИЯ
// // ================================
// class Visualizer {
// public:
//     void visualize(const BuildingResult& result, const AlgorithmConfig& config, int imageIndex) {
//         if (!config.showVisualization) return;
        
//         // Окно 1: Оригинальное изображение с выделенным зданием
//         Mat displayOriginal = result.originalImage.clone();
//         rectangle(displayOriginal, result.buildingRect, Scalar(0, 255, 0), 2);
        
//         string info = "Floors: " + to_string(result.detectedFloors) + 
//                       " | Period: " + to_string(result.periodPx) + "px" +
//                       " | Confidence: " + to_string(result.confidence) + "%";
//         putText(displayOriginal, info, Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0, 255, 0), 2);
        
//         // Рисуем линии этажей
//         if (result.periodPx > 0 && result.buildingRect.height > 0) {
//             for (int y = result.buildingRect.y + result.periodPx; 
//                  y < result.buildingRect.y + result.buildingRect.height; 
//                  y += result.periodPx) {
//                 line(displayOriginal, Point(0, y), Point(displayOriginal.cols, y), Scalar(0, 255, 255), 1);
//             }
//         }
        
//         // Окно 2: Вертикальный профиль
//         Mat profilePlot(500, 800, CV_8UC3, Scalar(255, 255, 255));
        
//         // График оригинального профиля
//         if (!result.profile.empty()) {
//             float maxVal = *max_element(result.profile.begin(), result.profile.end());
//             for (size_t y = 0; y < result.profile.size() - 1; ++y) {
//                 int x1 = 50 + result.profile[y] / maxVal * 300;
//                 int x2 = 50 + result.profile[y + 1] / maxVal * 300;
//                 int y1 = 400 - y * 350 / result.profile.size();
//                 int y2 = 400 - (y + 1) * 350 / result.profile.size();
//                 line(profilePlot, Point(x1, y1), Point(x2, y2), Scalar(0, 0, 255), 1);
//             }
//         }
        
//         // График сглаженного профиля
//         if (!result.smoothedProfile.empty()) {
//             float maxVal = *max_element(result.smoothedProfile.begin(), result.smoothedProfile.end());
//             for (size_t y = 0; y < result.smoothedProfile.size() - 1; ++y) {
//                 int x1 = 400 + result.smoothedProfile[y] / maxVal * 300;
//                 int x2 = 400 + result.smoothedProfile[y + 1] / maxVal * 300;
//                 int y1 = 400 - y * 350 / result.smoothedProfile.size();
//                 int y2 = 400 - (y + 1) * 350 / result.smoothedProfile.size();
//                 line(profilePlot, Point(x1, y1), Point(x2, y2), Scalar(255, 0, 0), 2);
//             }
//         }
        
//         putText(profilePlot, "Original Profile (Red) | Smoothed Profile (Blue)", 
//                 Point(50, 30), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 0), 1);
//         putText(profilePlot, "Top (sky)", Point(50, 80), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 0, 255), 1);
//         putText(profilePlot, "Bottom (ground)", Point(50, 470), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 0, 255), 1);
        
//         // Окно 3: Автокорреляция с пиками
//         Mat autocorrPlot(500, 800, CV_8UC3, Scalar(255, 255, 255));
        
//         if (!result.autocorrelation.empty()) {
//             float maxVal = *max_element(result.autocorrelation.begin() + 10, 
//                                         result.autocorrelation.begin() + min(200, (int)result.autocorrelation.size()));
//             if (maxVal > 0) {
//                 for (size_t lag = 10; lag < result.autocorrelation.size() - 1 && lag < 300; ++lag) {
//                     int x1 = 50 + lag * 700 / 300;
//                     int x2 = 50 + (lag + 1) * 700 / 300;
//                     int y1 = 400 - result.autocorrelation[lag] / maxVal * 350;
//                     int y2 = 400 - result.autocorrelation[lag + 1] / maxVal * 350;
//                     line(autocorrPlot, Point(x1, y1), Point(x2, y2), Scalar(0, 0, 255), 2);
//                 }
//             }
            
//             // Отмечаем пики
//             for (int peak : result.peaks) {
//                 if (peak < 300) {
//                     int x = 50 + peak * 700 / 300;
//                     int y = 400 - result.autocorrelation[peak] / maxVal * 350;
//                     circle(autocorrPlot, Point(x, y), 5, Scalar(0, 255, 0), -1);
                    
//                     string text = to_string(peak) + "px";
//                     putText(autocorrPlot, text, Point(x - 20, y - 10), 
//                             FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 255, 0), 1);
//                 }
//             }
//         }
        
//         // Отмечаем выбранный период
//         if (result.periodPx > 0 && result.periodPx < 300) {
//             int x = 50 + result.periodPx * 700 / 300;
//             line(autocorrPlot, Point(x, 50), Point(x, 380), Scalar(255, 0, 0), 2);
//             string text = "Selected: " + to_string(result.periodPx) + "px";
//             putText(autocorrPlot, text, Point(x - 30, 40), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255, 0, 0), 2);
//         }
        
//         putText(autocorrPlot, "Autocorrelation with Peaks", Point(50, 30), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 0), 1);
//         putText(autocorrPlot, "Lag (pixels)", Point(50, 480), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 0, 0), 1);
//         putText(autocorrPlot, "Correlation", Point(20, 200), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 0, 0), 1, 90);
        
//         // Окно 4: Выделенное здание с линиями этажей
//         Mat buildingDisplay;
//         if (!result.buildingImage.empty()) {
//             cvtColor(result.buildingImage, buildingDisplay, COLOR_GRAY2BGR);
            
//             if (result.periodPx > 0) {
//                 for (int y = result.periodPx; y < buildingDisplay.rows; y += result.periodPx) {
//                     line(buildingDisplay, Point(0, y), Point(buildingDisplay.cols, y), Scalar(0, 255, 255), 1);
//                 }
//             }
            
//             string floorText = "Detected: " + to_string(result.detectedFloors) + " floors";
//             putText(buildingDisplay, floorText, Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 0), 2);
//         }
        
//         // Показываем все окна
//         string windowName1 = "Image " + to_string(imageIndex) + " - Building Detection";
//         string windowName2 = "Image " + to_string(imageIndex) + " - Vertical Profile";
//         string windowName3 = "Image " + to_string(imageIndex) + " - Autocorrelation";
//         string windowName4 = "Image " + to_string(imageIndex) + " - Extracted Building";
        
//         imshow(windowName1, displayOriginal);
//         imshow(windowName2, profilePlot);
//         imshow(windowName3, autocorrPlot);
//         if (!buildingDisplay.empty()) {
//             imshow(windowName4, buildingDisplay);
//         }
        
//         // Перемещаем окна для удобства
//         moveWindow(windowName1, 0, 0);
//         moveWindow(windowName2, 900, 0);
//         moveWindow(windowName3, 0, 550);
//         moveWindow(windowName4, 900, 550);
        
//         if (config.saveVisualization) {
//             imwrite("debug_" + to_string(imageIndex) + "_original.png", displayOriginal);
//             imwrite("debug_" + to_string(imageIndex) + "_profile.png", profilePlot);
//             imwrite("debug_" + to_string(imageIndex) + "_autocorr.png", autocorrPlot);
//         }
//     }
// };

// // ================================
// // 6. ОСНОВНОЙ АЛГОРИТМ
// // ================================
// BuildingResult analyzeBuilding(const string& imagePath, const AlgorithmConfig& config, int imageIndex = 0) {
//     BuildingResult result;
//     result.filename = imagePath;
//     result.confidence = 0;
    
//     auto start = chrono::high_resolution_clock::now();
    
//     // Загрузка
//     result.originalImage = imread(imagePath);
//     if (result.originalImage.empty()) {
//         result.detectedFloors = -1;
//         return result;
//     }
    
//     // Предобработка
//     ImagePreprocessor preprocessor;
//     Mat processed = preprocessor.process(result.originalImage, config);
//     result.processedImage = processed.clone();
    
//     // Обнаружение здания
//     BuildingDetector detector;
//     result.buildingRect = detector.detectBuilding(processed, config);
//     result.buildingImage = processed(result.buildingRect).clone();
    
//     // Анализ профиля
//     VerticalProfileAnalyzer profileAnalyzer;
//     result.profile = profileAnalyzer.computeProfile(processed, result.buildingRect, config);
//     result.smoothedProfile = profileAnalyzer.smoothProfile(result.profile, config);
    
//     int buildingHeight = result.buildingRect.height;
//     result.buildingHeightPx = buildingHeight;
    
//     // Спектральный анализ
//     SpectralAnalyzer spectralAnalyzer;
//     int maxLag = buildingHeight * config.maxLagPercent / 100;
//     maxLag = std::min(maxLag, 200);
    
//     result.autocorrelation = spectralAnalyzer.computeAutocorrelation(result.smoothedProfile, maxLag);
    
//     // Поиск пиков
//     double bestPeakValue;
//     result.peaks = spectralAnalyzer.findPeaks(result.autocorrelation, config.minPeriodPx, config.maxPeriodPx,
//                                                config.peakThreshold, config.peakNeighborhood, bestPeakValue);
    
//     // Оценка этажей
//     if (result.peaks.empty()) {
//         result.detectedFloors = -1;
//         result.periodPx = -1;
//         result.peakCorrelation = 0;
//         result.confidence = 0;
//     } else {
//         result.candidatePeriods = result.peaks;
//         result.candidateScores.clear();
        
//         for (int p : result.peaks) {
//             result.candidateScores.push_back(result.autocorrelation[p]);
//         }
        
//         int bestPeriod = result.peaks[0];
//         double bestScore = result.autocorrelation[bestPeriod];
//         for (size_t i = 0; i < result.peaks.size(); ++i) {
//             if (result.autocorrelation[result.peaks[i]] > bestScore) {
//                 bestScore = result.autocorrelation[result.peaks[i]];
//                 bestPeriod = result.peaks[i];
//             }
//         }
        
//         result.periodPx = bestPeriod;
//         result.peakCorrelation = bestScore;
        
//         int rawFloors = buildingHeight / bestPeriod;
//         result.candidateFloors.push_back(rawFloors);
        
//         if (config.harmonicCorrection) {
//             if (rawFloors > config.maxFloors && rawFloors / 2 >= config.minFloors) {
//                 rawFloors = rawFloors / 2;
//                 result.periodPx = bestPeriod * 2;
//                 result.candidateFloors.push_back(rawFloors);
//             }
//             if (rawFloors < config.minFloors && rawFloors * 2 <= config.maxFloors) {
//                 rawFloors = rawFloors * 2;
//                 result.periodPx = bestPeriod / 2;
//                 result.candidateFloors.push_back(rawFloors);
//             }
//         }
        
//         result.detectedFloors = std::max(config.minFloors, std::min(rawFloors, config.maxFloors));
//         result.confidence = (int)(bestScore * 100);
//         result.confidence = std::min(100, std::max(0, result.confidence));
        
//         if (bestScore < config.confidenceMinThreshold) {
//             result.confidence = 0;
//         }
//     }
    
//     auto end = chrono::high_resolution_clock::now();
//     result.timeMs = chrono::duration<double, milli>(end - start).count();
    
//     // Визуализация
//     Visualizer visualizer;
//     visualizer.visualize(result, config, imageIndex);
    
//     return result;
// }

// // ================================
// // 7. ТЕСТИРОВАНИЕ
// // ================================
// void testImage(const string& imagePath, const string& imageName, int imageIndex, int expectedFloors = -1) {
//     cout << "\n" << string(70, '=') << endl;
//     cout << "  📸 ИЗОБРАЖЕНИЕ " << imageIndex << ": " << imageName << endl;
//     cout << string(70, '=') << endl;
    
//     AlgorithmConfig config;
    
//     // Настройка параметров для оптимального результата
//     config.minPeriodPx = 35;
//     config.maxPeriodPx = 100;
//     config.peakThreshold = 0.25f;
//     config.stripWidthPercent = 30;
//     config.profileSmoothingWindow = 7;
//     config.showVisualization = true;
//     config.saveVisualization = false;
//     config.useCLAHE = true;
//     config.autoDetectBuilding = true;
//     config.harmonicCorrection = true;
    
//     cout << "\n  🔧 ПАРАМЕТРЫ:\n";
//     cout << "     minPeriod=" << config.minPeriodPx << "px\n";
//     cout << "     maxPeriod=" << config.maxPeriodPx << "px\n";
//     cout << "     threshold=" << config.peakThreshold << "\n";
//     cout << "     stripWidth=" << config.stripWidthPercent << "%\n";
//     cout << "     smoothing=" << config.profileSmoothingWindow << "\n";
    
//     BuildingResult result = analyzeBuilding(imagePath, config, imageIndex);
    
//     cout << "\n  📊 РЕЗУЛЬТАТЫ АНАЛИЗА:\n";
//     cout << "     Высота здания: " << result.buildingHeightPx << " px\n";
//     cout << "     Найдено пиков: " << result.peaks.size() << "\n";
    
//     if (!result.peaks.empty()) {
//         cout << "     Пики: ";
//         for (int p : result.peaks) cout << p << "px ";
//         cout << "\n";
//         cout << "     Лучший период: " << result.periodPx << " px\n";
//         cout << "     Корреляция: " << result.peakCorrelation << "\n";
//     }
    
//     cout << "\n  ╔══════════════════════════════════════════════════════════════════╗\n";
//     cout << "  ║  🏢 РЕЗУЛЬТАТ: " << result.detectedFloors << " ЭТАЖЕЙ";
//     for (int i = 0; i < 30 - to_string(result.detectedFloors).length(); i++) cout << " ";
//     cout << "║\n";
//     cout << "  ║     Уверенность: " << result.confidence << "%";
//     for (int i = 0; i < 36 - to_string(result.confidence).length(); i++) cout << " ";
//     cout << "║\n";
//     cout << "  ║     Высота этажа: " << result.periodPx << " px";
//     for (int i = 0; i < 34 - to_string(result.periodPx).length(); i++) cout << " ";
//     cout << "║\n";
//     cout << "  ║     Время: " << result.timeMs << " ms";
//     for (int i = 0; i < 38 - to_string(result.timeMs).length(); i++) cout << " ";
//     cout << "║\n";
//     if (expectedFloors > 0) {
//         cout << "  ║     Ожидалось: " << expectedFloors << " этажей";
//         for (int i = 0; i < 36 - to_string(expectedFloors).length(); i++) cout << " ";
//         cout << "║\n";
//     }
//     cout << "  ╚══════════════════════════════════════════════════════════════════╝\n";
// }

// // ================================
// // 8. ГЛАВНАЯ ФУНКЦИЯ
// // ================================
// int main() {
//     cout << "\n";
//     cout << "╔══════════════════════════════════════════════════════════════════════════════════════╗\n";
//     cout << "║                         ПОДСЧЁТ ЭТАЖЕЙ - РАСШИРЕННЫЙ АЛГОРИТМ                       ║\n";
//     cout << "║                              С ВИЗУАЛИЗАЦИЕЙ                                        ║\n";
//     cout << "╚══════════════════════════════════════════════════════════════════════════════════════╝\n";
    
//     cout << "\n  💡 ИНФОРМАЦИЯ:\n";
//     cout << "     Будут открыты 4 окна с визуализацией для каждого изображения:\n";
//     cout << "     1. Оригинальное изображение с выделенным зданием и линиями этажей\n";
//     cout << "     2. Вертикальный профиль яркости (красный - оригинал, синий - сглаженный)\n";
//     cout << "     3. Автокорреляция с отмеченными пиками\n";
//     cout << "     4. Выделенное здание с линиями этажей\n";
//     cout << "\n     Нажмите любую клавишу для перехода к следующему изображению...\n";
    
//     // Список изображений
//     vector<tuple<string, string, int>> testImages = {
//         {"/home/shymilly/FloorCounter/modern_building1.jpg", "modern_building1.jpg", -1},
//         {"/home/shymilly/FloorCounter/modern_building2.jpg", "modern_building2.jpg", -1},
//         {"/home/shymilly/FloorCounter/modern_building3.jpg", "modern_building3.jpg", -1}
//     };
    
//     int imageIndex = 1;
//     for (const auto& img : testImages) {
//         string path = get<0>(img);
//         string name = get<1>(img);
//         int expected = get<2>(img);
        
//         if (!filesystem::exists(path)) {
//             cout << "\n⚠️ Файл не найден: " << path << endl;
//             continue;
//         }
        
//         testImage(path, name, imageIndex++, expected);
        
//         if (imageIndex <= 4) {
//             cout << "\n  Нажмите любую клавишу для продолжения...";
//             waitKey(0);
//             destroyAllWindows();
//         }
//     }
    
//     cout << "\n" << string(70, '=') << endl;
//     cout << "  ✅ ТЕСТИРОВАНИЕ ЗАВЕРШЕНО" << endl;
//     cout << string(70, '=') << endl;
    
//     cout << "\n  Нажмите Enter для выхода...";
//     cin.get();
    
//     return 0;
// }

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <map>

using namespace cv;
using namespace std;

// Структура для хранения горизонтальной линии
struct HorizontalLine {
    int y;
    Point start;
    Point end;
    double length;
    double confidence;
    double thickness;  // Толщина линии
};

// Структура для хранения параметров детекции
struct DetectionParams {
    double angleThreshold = 8.0;
    double minLengthRatio = 0.20;         // Увеличен для отсечения коротких линий
    int cannyThreshold1 = 40;             // Повышен для фильтрации шума
    int cannyThreshold2 = 120;
    int houghThreshold = 70;
    int morphKernelSize = 25;             // Увеличен для соединения линий этажей
    int minLineGap = 15;
    int minLineLength = 60;               // Минимальная длина линии
    double minThickness = 2.0;            // Минимальная толщина линии
    double maxThickness = 15.0;           // Максимальная толщина линии
};

// Определение толщины линии
double getLineThickness(const Mat& edges, Point start, Point end) {
    LineIterator it(edges, start, end);
    int thickness = 0;
    for (int i = 0; i < it.count; ++i, ++it) {
        if (edges.at<uchar>(it.pos()) > 0) {
            thickness++;
        }
    }
    return (double)thickness / it.count * 3.0; // Примерная оценка толщины
}

// Группировка Y-координат в кластеры с ограничением по количеству
int clusterYCoordinates(vector<int>& yCoordinates, vector<double>& weights, double threshold, int maxExpectedFloors = 30) {
    if (yCoordinates.empty()) return 0;
    
    vector<pair<int, double>> weightedPoints;
    for (size_t i = 0; i < yCoordinates.size(); ++i) {
        weightedPoints.push_back({yCoordinates[i], weights[i]});
    }
    
    sort(weightedPoints.begin(), weightedPoints.end());
    
    vector<vector<pair<int, double>>> clusters;
    clusters.push_back(vector<pair<int, double>>{weightedPoints[0]});
    
    for (size_t i = 1; i < weightedPoints.size(); ++i) {
        if (abs(weightedPoints[i].first - clusters.back().back().first) <= threshold) {
            clusters.back().push_back(weightedPoints[i]);
        } else {
            clusters.push_back(vector<pair<int, double>>{weightedPoints[i]});
        }
    }
    
    // Если кластеров слишком много, объединяем их
    while ((int)clusters.size() > maxExpectedFloors && threshold < 100) {
        threshold *= 1.5;
        return clusterYCoordinates(yCoordinates, weights, threshold, maxExpectedFloors);
    }
    
    yCoordinates.clear();
    weights.clear();
    
    for (const auto& cluster : clusters) {
        double weightedSum = 0;
        double totalWeight = 0;
        for (const auto& point : cluster) {
            weightedSum += point.first * point.second;
            totalWeight += point.second;
        }
        yCoordinates.push_back(round(weightedSum / totalWeight));
        weights.push_back(totalWeight / cluster.size());
    }
    
    return clusters.size();
}

// Морфологическая обработка
Mat morphologicalProcessing(const Mat& edges, int kernelSize) {
    Mat morphed;
    Mat horizontalKernel = getStructuringElement(MORPH_RECT, Size(kernelSize, 3));
    morphologyEx(edges, morphed, MORPH_CLOSE, horizontalKernel);
    
    Mat verticalKernel = getStructuringElement(MORPH_RECT, Size(3, kernelSize/2));
    morphologyEx(morphed, morphed, MORPH_OPEN, verticalKernel);
    
    return morphed;
}

// Поиск горизонтальных линий с фильтрацией по толщине
vector<HorizontalLine> findHorizontalLines(const Mat& image, const DetectionParams& params) {
    Mat gray, edges, morphedEdges;
    
    if (image.channels() == 3) {
        cvtColor(image, gray, COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }
    
    GaussianBlur(gray, gray, Size(5, 5), 1.0);
    Canny(gray, edges, params.cannyThreshold1, params.cannyThreshold2, 3);
    morphedEdges = morphologicalProcessing(edges, params.morphKernelSize);
    
    vector<Vec4i> lines;
    HoughLinesP(morphedEdges, lines, 1, CV_PI/180, params.houghThreshold, 
                params.minLineLength, params.minLineGap);
    
    vector<HorizontalLine> horizontalLines;
    double minLength = image.cols * params.minLengthRatio;
    
    for (const auto& ln : lines) {
        Point start(ln[0], ln[1]);
        Point end(ln[2], ln[3]);
        
        double deltaY = end.y - start.y;
        double deltaX = end.x - start.x;
        
        if (deltaX == 0) continue;
        
        double angle = atan(abs(deltaY) / abs(deltaX)) * 180.0 / CV_PI;
        
        if (angle <= params.angleThreshold) {
            double length = sqrt(deltaX * deltaX + deltaY * deltaY);
            if (length >= minLength) {
                HorizontalLine hl;
                hl.start = start;
                hl.end = end;
                hl.length = length;
                hl.y = (start.y + end.y) / 2;
                hl.thickness = getLineThickness(edges, start, end);
                
                // Фильтрация по толщине (отсекаем слишком толстые и слишком тонкие линии)
                if (hl.thickness >= params.minThickness && hl.thickness <= params.maxThickness) {
                    hl.confidence = (length / image.cols) * (1.0 - angle / params.angleThreshold) * 
                                    (1.0 - abs(hl.thickness - 5.0) / 10.0);
                    horizontalLines.push_back(hl);
                }
            }
        }
    }
    
    return horizontalLines;
}

// Автоматическое определение границ здания
Rect autoDetectBuilding(const Mat& gray) {
    int height = gray.rows;
    int width = gray.cols;
    
    vector<int> verticalProjection(width, 0);
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            if (gray.at<uchar>(y, x) < 200) {
                verticalProjection[x]++;
            }
        }
    }
    
    vector<int> smoothed(width, 0);
    for (int x = 5; x < width - 5; ++x) {
        for (int dx = -5; dx <= 5; ++dx) {
            smoothed[x] += verticalProjection[x + dx];
        }
        smoothed[x] /= 11;
    }
    
    int maxProj = *max_element(smoothed.begin(), smoothed.end());
    int threshold = maxProj * 0.25;
    
    int leftBound = 0;
    int rightBound = width - 1;
    
    for (int x = 0; x < width; ++x) {
        if (smoothed[x] > threshold) {
            leftBound = x;
            break;
        }
    }
    
    for (int x = width - 1; x >= 0; --x) {
        if (smoothed[x] > threshold) {
            rightBound = x;
            break;
        }
    }
    
    leftBound = max(0, leftBound - width/25);
    rightBound = min(width - 1, rightBound + width/25);
    
    return Rect(leftBound, 0, rightBound - leftBound, height);
}

// Определение оптимального количества этажей
int determineOptimalFloors(const vector<int>& yCoordinates, int buildingHeight) {
    if (yCoordinates.size() < 2) return yCoordinates.size() + 1;
    
    // Вычисляем расстояния между линиями
    vector<int> gaps;
    for (size_t i = 1; i < yCoordinates.size(); ++i) {
        int gap = yCoordinates[i] - yCoordinates[i-1];
        if (gap > 10 && gap < buildingHeight / 2) {
            gaps.push_back(gap);
        }
    }
    
    if (gaps.empty()) return yCoordinates.size() + 1;
    
    // Находим наиболее часто встречающийся зазор (моду)
    map<int, int> gapFrequency;
    for (int g : gaps) {
        // Округляем до ближайших 5 пикселей
        int roundedGap = round(g / 5.0) * 5;
        gapFrequency[roundedGap]++;
    }
    
    int typicalGap = 50;
    int maxFreq = 0;
    for (const auto& [gap, freq] : gapFrequency) {
        if (freq > maxFreq) {
            maxFreq = freq;
            typicalGap = gap;
        }
    }
    
    // Ожидаемое количество этажей на основе типичного зазора
    int expectedFloorsByGap = buildingHeight / typicalGap;
    
    // Количество уникальных линий + 1
    int floorsByLines = yCoordinates.size() + 1;
    
    // Берём среднее, но не более 25 этажей
    int result = (expectedFloorsByGap + floorsByLines) / 2;
    
    // Коррекция: если разница слишком большая, берём меньшее значение
    if (abs(expectedFloorsByGap - floorsByLines) > 5) {
        result = min(expectedFloorsByGap, floorsByLines);
    }
    
    return min(25, max(1, result));
}

// Основная функция определения этажей
int detectFloors(const Mat& image, vector<int>& floorPositions, Mat& outputImage) {
    DetectionParams params;
    
    Mat gray;
    if (image.channels() == 3) {
        cvtColor(image, gray, COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }
    
    Rect buildingRect = autoDetectBuilding(gray);
    Mat building = gray(buildingRect);
    
    vector<HorizontalLine> horizontalLines = findHorizontalLines(building, params);
    
    if (horizontalLines.empty()) {
        floorPositions.clear();
        outputImage = image.clone();
        putText(outputImage, "Floors: 1 (no lines detected)", Point(10, 30), 
                FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 0, 255), 2);
        return 1;
    }
    
    // Сортируем линии по Y и удаляем дубликаты
    vector<int> yCoordinates;
    vector<double> weights;
    
    for (const auto& hl : horizontalLines) {
        yCoordinates.push_back(hl.y);
        weights.push_back(hl.confidence);
    }
    
    // Первичная кластеризация с малым порогом
    vector<int> tempY = yCoordinates;
    vector<double> tempW = weights;
    int distanceThreshold = max(15, building.rows / 30);
    
    clusterYCoordinates(tempY, tempW, distanceThreshold, 30);
    
    // Оптимальное количество этажей
    int numFloors = determineOptimalFloors(tempY, building.rows);
    
    // Ограничиваем максимальное количество этажей
    numFloors = min(25, max(1, numFloors));
    
    // Генерируем позиции этажей (равномерно распределяем)
    floorPositions.clear();
    if (numFloors > 1) {
        int step = building.rows / numFloors;
        for (int i = 1; i <= numFloors - 1; ++i) {
            floorPositions.push_back(buildingRect.y + i * step);
        }
    }
    
    // Визуализация
    outputImage = image.clone();
    rectangle(outputImage, buildingRect, Scalar(255, 100, 0), 2);
    
    // Рисуем найденные оригинальные линии (для отладки)
    for (int y : tempY) {
        int globalY = y + buildingRect.y;
        line(outputImage, Point(0, globalY), Point(outputImage.cols - 1, globalY), 
             Scalar(0, 100, 255), 1);
    }
    
    // Рисуем итоговые линии этажей
    for (int y : floorPositions) {
        line(outputImage, Point(0, y), Point(outputImage.cols - 1, y), Scalar(0, 0, 255), 2);
    }
    
    string text = "Floors: " + to_string(numFloors);
    putText(outputImage, text, Point(10, 35), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);
    
    string debugText = "Lines: " + to_string(tempY.size()) + " → Floors: " + to_string(numFloors);
    putText(outputImage, debugText, Point(10, 60), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255, 255, 0), 1);
    
    return numFloors;
}

// ГЛАВНАЯ ФУНКЦИЯ
int main(int argc, char* argv[]) {
    if (argc != 2) {
        cout << "Использование: " << argv[0] << " <путь_к_изображению>" << endl;
        cout << "Пример: " << argv[0] << " building1.jpg" << endl;
        return 1;
    }
    
    string imagePath = argv[1];
    
    cout << "========================================" << endl;
    cout << "ПОДСЧЁТ ЭТАЖЕЙ - АЛГОРИТМ (HORIZONTAL LINES)" << endl;
    cout << "========================================" << endl;
    
    Mat image = imread(imagePath);
    if (image.empty()) {
        cout << "Ошибка: не удалось загрузить изображение " << imagePath << endl;
        return 1;
    }
    
    cout << "Обработка: " << imagePath << endl;
    cout << "Размер: " << image.cols << " x " << image.rows << endl;
    
    vector<int> floorPositions;
    Mat outputImage;
    
    auto start = chrono::high_resolution_clock::now();
    int floors = detectFloors(image, floorPositions, outputImage);
    auto end = chrono::high_resolution_clock::now();
    double ms = chrono::duration<double, milli>(end - start).count();
    
    cout << "\n ╔══════════════════════════════════════════════════════════════════╗" << endl;
    cout << " ║ 🏢 РЕЗУЛЬТАТ: " << floors << " ЭТАЖЕЙ";
    for (int i = 0; i < 30 - to_string(floors).length(); i++) cout << " ";
    cout << "║" << endl;
    cout << " ║ Найдено линий: " << floorPositions.size();
    for (int i = 0; i < 36 - to_string(floorPositions.size()).length(); i++) cout << " ";
    cout << "║" << endl;
    cout << " ║ Время: " << ms << " ms";
    for (int i = 0; i < 38 - to_string(ms).length(); i++) cout << " ";
    cout << "║" << endl;
    cout << " ╚══════════════════════════════════════════════════════════════════╝" << endl;
    
    string outFile = "result_" + imagePath.substr(imagePath.find_last_of("/\\") + 1);
    imwrite(outFile, outputImage);
    cout << "\nРезультат сохранён: " << outFile << endl;
    
    return 0;
}