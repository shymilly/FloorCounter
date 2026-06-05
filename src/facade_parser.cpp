#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <map>
#include <filesystem>
#include <iomanip>

using namespace cv;
using namespace std;
namespace fs = std::filesystem;

// ================================
// СТРУКТУРЫ
// ================================

struct HorizontalLine {
    int y;
    Point start;
    Point end;
    double length;
    double confidence;
};

struct DetectionParams {
    string name;
    double angleThreshold;
    double minLengthRatio;
    int cannyThreshold1;
    int cannyThreshold2;
    int houghThreshold;
    int minLineLength;
    int minLineGap;
    int smoothingWindow;
};

struct ImageResult {
    string filename;
    int detectedFloors;
    double timeMs;
    int linesFound;
    int buildingHeight;
    int typicalGap;
};

// ================================
// ТРИ НАБОРА ПАРАМЕТРОВ
// ================================

vector<DetectionParams> getParameterSets() {
    vector<DetectionParams> params;
    
    // НАБОР 1: СТАНДАРТНЫЙ
    params.push_back({
        "STANDARD",
        10.0,       // angleThreshold
        0.20,       // minLengthRatio
        50,         // cannyThreshold1
        150,        // cannyThreshold2
        80,         // houghThreshold
        50,         // minLineLength
        20,         // minLineGap
        7           // smoothingWindow
    });
    
    // НАБОР 2: ЧУВСТВИТЕЛЬНЫЙ
    params.push_back({
        "SENSITIVE",
        12.0,       // angleThreshold
        0.15,       // minLengthRatio
        30,         // cannyThreshold1
        100,        // cannyThreshold2
        60,         // houghThreshold
        40,         // minLineLength
        15,         // minLineGap
        5           // smoothingWindow
    });
    
    // НАБОР 3: КОНСЕРВАТИВНЫЙ
    params.push_back({
        "CONSERVATIVE",
        8.0,        // angleThreshold
        0.30,       // minLengthRatio
        70,         // cannyThreshold1
        200,        // cannyThreshold2
        100,        // houghThreshold
        70,         // minLineLength
        30,         // minLineGap
        9           // smoothingWindow
    });
    
    return params;
}

// ================================
// ОПРЕДЕЛЕНИЕ ВЕРХНЕЙ И НИЖНЕЙ ГРАНИЦЫ ЗДАНИЯ
// ================================

void getBuildingBounds(const Mat& gray, int& topY, int& bottomY) {
    int height = gray.rows;
    int width = gray.cols;
    
    // Вертикальная проекция яркости
    vector<double> rowBrightness(height, 0);
    for (int y = 0; y < height; ++y) {
        double sum = 0;
        for (int x = 0; x < width; ++x) {
            sum += gray.at<uchar>(y, x);
        }
        rowBrightness[y] = sum / width;
    }
    
    // Ищем резкие перепады яркости (границы здания)
    vector<double> gradients(height - 1, 0);
    for (int y = 0; y < height - 1; ++y) {
        gradients[y] = abs(rowBrightness[y + 1] - rowBrightness[y]);
    }
    
    // Верхняя граница: первый резкий перепад сверху (небо -> здание)
    topY = 0;
    for (int y = height / 10; y < height / 2; ++y) {
        if (gradients[y] > 30) {
            topY = y - 10;
            break;
        }
    }
    
    // Нижняя граница: последний резкий перепад снизу (здание -> земля)
    bottomY = height - 1;
    for (int y = height - height / 10; y > height / 2; --y) {
        if (gradients[y] > 30) {
            bottomY = y + 10;
            break;
        }
    }
    
    // Гарантируем минимальную высоту здания
    if (bottomY - topY < height / 3) {
        topY = height / 8;
        bottomY = height - height / 8;
    }
}

// ================================
// ПОИСК ГОРИЗОНТАЛЬНЫХ ЛИНИЙ
// ================================

vector<HorizontalLine> findHorizontalLines(const Mat& image, const DetectionParams& params) {
    Mat gray, edges;
    
    if (image.channels() == 3) {
        cvtColor(image, gray, COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }
    
    // Сглаживание
    GaussianBlur(gray, gray, Size(params.smoothingWindow, params.smoothingWindow), 1.0);
    
    // Детекция границ
    Canny(gray, edges, params.cannyThreshold1, params.cannyThreshold2, 3);
    
    // Морфологическое усиление горизонтальных линий
    Mat horizontalKernel = getStructuringElement(MORPH_RECT, Size(20, 1));
    Mat horizontalEdges;
    morphologyEx(edges, horizontalEdges, MORPH_CLOSE, horizontalKernel);
    
    // Поиск линий
    vector<Vec4i> lines;
    HoughLinesP(horizontalEdges, lines, 1, CV_PI/180, params.houghThreshold,
                params.minLineLength, params.minLineGap);
    
    vector<HorizontalLine> horizontalLines;
    double minLength = image.cols * params.minLengthRatio;
    
    for (const auto& ln : lines) {
        Point start(ln[0], ln[1]);
        Point end(ln[2], ln[3]);
        
        double deltaY = abs(end.y - start.y);
        double deltaX = abs(end.x - start.x);
        
        if (deltaX == 0) continue;
        
        double angle = atan(deltaY / deltaX) * 180.0 / CV_PI;
        
        // Только почти горизонтальные линии
        if (angle <= params.angleThreshold) {
            double length = sqrt(deltaX * deltaX + deltaY * deltaY);
            if (length >= minLength) {
                HorizontalLine hl;
                hl.start = start;
                hl.end = end;
                hl.length = length;
                hl.y = (start.y + end.y) / 2;
                hl.confidence = length / image.cols;
                horizontalLines.push_back(hl);
            }
        }
    }
    
    return horizontalLines;
}

// ================================
// ГРУППИРОВКА Y-КООРДИНАТ
// ================================

vector<int> clusterYCoordinates(vector<int>& yCoordinates, int threshold) {
    if (yCoordinates.empty()) return {};
    
    sort(yCoordinates.begin(), yCoordinates.end());
    
    vector<vector<int>> clusters;
    clusters.push_back({yCoordinates[0]});
    
    for (size_t i = 1; i < yCoordinates.size(); ++i) {
        if (yCoordinates[i] - clusters.back().back() <= threshold) {
            clusters.back().push_back(yCoordinates[i]);
        } else {
            clusters.push_back({yCoordinates[i]});
        }
    }
    
    vector<int> result;
    for (const auto& cluster : clusters) {
        int sum = 0;
        for (int y : cluster) sum += y;
        result.push_back(sum / cluster.size());
    }
    
    return result;
}

// ================================
// ОПРЕДЕЛЕНИЕ КОЛИЧЕСТВА ЭТАЖЕЙ
// ================================

int detectFloors(const Mat& image, const DetectionParams& params, 
                 vector<int>& floorPositions, Mat& outputImage,
                 int& linesFound, int& buildingHeight, int& typicalGap) {
    
    Mat gray;
    if (image.channels() == 3) {
        cvtColor(image, gray, COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }
    
    // 1. Определяем границы здания (БЕЗ ОБРЕЗКИ, только для информации)
    int topY, bottomY;
    getBuildingBounds(gray, topY, bottomY);
    buildingHeight = bottomY - topY;
    
    // 2. Ищем горизонтальные линии во ВСЁМ изображении
    vector<HorizontalLine> lines = findHorizontalLines(gray, params);
    
    // 3. Фильтруем линии только внутри здания
    vector<int> yPositions;
    for (const auto& line : lines) {
        if (line.y >= topY && line.y <= bottomY && line.confidence > 0.5) {
            yPositions.push_back(line.y);
        }
    }
    
    linesFound = yPositions.size();
    
    // 4. Группируем близкие линии
    int clusterThreshold = max(15, buildingHeight / 30);
    vector<int> clusteredY = clusterYCoordinates(yPositions, clusterThreshold);
    
    // 5. Определяем типичное расстояние между линиями
    typicalGap = 50;
    if (clusteredY.size() >= 2) {
        vector<int> gaps;
        for (size_t i = 1; i < clusteredY.size(); ++i) {
            gaps.push_back(clusteredY[i] - clusteredY[i-1]);
        }
        sort(gaps.begin(), gaps.end());
        typicalGap = gaps[gaps.size() / 2];
    }
    
    // 6. Рассчитываем количество этажей
    int numFloors;
    if (clusteredY.empty()) {
        // Если линий нет, оцениваем по высоте
        numFloors = max(1, buildingHeight / 70);
    } else {
        // Этажи = (высота здания / типичный зазор) + 1
        numFloors = buildingHeight / typicalGap + 1;
    }
    
    // Ограничения
    numFloors = max(1, min(35, numFloors));
    
    // 7. Генерируем позиции этажей для визуализации
    floorPositions.clear();
    if (numFloors > 1) {
        int step = buildingHeight / numFloors;
        for (int i = 1; i <= numFloors - 1; ++i) {
            floorPositions.push_back(topY + i * step);
        }
    }
    
    // 8. Визуализация
    outputImage = image.clone();
    
    // Рисуем границы здания
    rectangle(outputImage, Point(0, topY), Point(outputImage.cols, bottomY), Scalar(255, 100, 0), 2);
    
    // Рисуем найденные линии
    for (int y : clusteredY) {
        line(outputImage, Point(0, y), Point(outputImage.cols - 1, y), Scalar(0, 100, 255), 1);
    }
    
    // Рисуем итоговые этажи
    for (int y : floorPositions) {
        line(outputImage, Point(0, y), Point(outputImage.cols - 1, y), Scalar(0, 0, 255), 2);
    }
    
    // Текст с результатом
    string text = "Floors: " + to_string(numFloors);
    putText(outputImage, text, Point(10, 35), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);
    
    string debugText = "Lines: " + to_string(clusteredY.size()) + " | Gap: " + to_string(typicalGap) + "px | Height: " + to_string(buildingHeight) + "px";
    putText(outputImage, debugText, Point(10, 65), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255, 255, 0), 1);
    
    return numFloors;
}

// ================================
// ПОЛУЧЕНИЕ СПИСКА ИЗОБРАЖЕНИЙ
// ================================

vector<string> getImageList(const string& folderPath, int maxImages = 50) {
    vector<string> images;
    
    for (int i = 1; i <= maxImages; ++i) {
        string filename = folderPath + "/modern_building" + to_string(i) + ".jpg";
        if (fs::exists(filename)) {
            images.push_back(filename);
        } else {
            filename = folderPath + "/modern_building" + to_string(i) + ".png";
            if (fs::exists(filename)) {
                images.push_back(filename);
            }
        }
    }
    
    return images;
}

// ================================
// ВЫВОД ТАБЛИЦЫ
// ================================

void printResultsTable(const vector<vector<ImageResult>>& allResults,
                       const vector<DetectionParams>& paramSets,
                       const vector<string>& imageNames) {
    
    cout << "\n\n";
    cout << "╔══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════╗\n";
    cout << "║                                         РЕЗУЛЬТАТЫ ТЕСТИРОВАНИЯ                                                    ║\n";
    cout << "╚══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════╝\n";
    cout << "\n";
    
    // Заголовок
    cout << "┌─────┬────────────────────────┬──────────────────────────────┬──────────────────────────────┬──────────────────────────────┐\n";
    cout << "│     │                        │         " << setw(26) << left << paramSets[0].name << "│         " << setw(26) << left << paramSets[1].name << "│         " << setw(26) << left << paramSets[2].name << "│\n";
    cout << "├─────┼────────────────────────┼──────────┬──────────┬───────────┼──────────┬──────────┬───────────┼──────────┬──────────┬───────────┤\n";
    cout << "│  №  │        Файл            │  Этажи   │ Линии    │ Время(ms) │  Этажи   │ Линии    │ Время(ms) │  Этажи   │ Линии    │ Время(ms) │\n";
    cout << "├─────┼────────────────────────┼──────────┼──────────┼───────────┼──────────┼──────────┼───────────┼──────────┼──────────┼───────────┤\n";
    
    for (size_t i = 0; i < imageNames.size(); ++i) {
        string shortName = imageNames[i].length() > 22 ? imageNames[i].substr(0, 19) + "..." : imageNames[i];
        cout << "│ " << setw(3) << right << (i+1) << " │ " << setw(22) << left << shortName << " │";
        
        for (int p = 0; p < 3; ++p) {
            if (i < allResults[p].size()) {
                cout << " " << setw(8) << right << allResults[p][i].detectedFloors << " │"
                     << " " << setw(8) << right << allResults[p][i].linesFound << " │"
                     << " " << setw(9) << right << fixed << setprecision(1) << allResults[p][i].timeMs << " │";
            } else {
                cout << " " << setw(8) << right << "-" << " │" << " " << setw(8) << right << "-" << " │" << " " << setw(9) << right << "-" << " │";
            }
        }
        cout << "\n";
    }
    
    cout << "└─────┴────────────────────────┴──────────┴──────────┴───────────┴──────────┴──────────┴───────────┴──────────┴──────────┴───────────┘\n";
}

// ================================
// ГЛАВНАЯ ФУНКЦИЯ
// ================================

int main() {
    cout << "\n";
    cout << "╔══════════════════════════════════════════════════════════════════════════════════════╗\n";
    cout << "║                    ПОДСЧЁТ ЭТАЖЕЙ - МАССОВОЕ ТЕСТИРОВАНИЕ                          ║\n";
    cout << "║                        50 ИЗОБРАЖЕНИЙ, 3 НАБОРА ПАРАМЕТРОВ                          ║\n";
    cout << "╚══════════════════════════════════════════════════════════════════════════════════════╝\n";
    
    string datasetPath = "/home/shymilly/FloorCounter/dataset";
    
    if (!fs::exists(datasetPath)) {
        cout << "\n❌ Папка не найдена: " << datasetPath << endl;
        cout << "   Создайте папку и добавьте изображения\n";
        return 1;
    }
    
    vector<string> imagePaths = getImageList(datasetPath, 50);
    
    if (imagePaths.empty()) {
        cout << "\n❌ Нет изображений в папке " << datasetPath << endl;
        return 1;
    }
    
    cout << "\n📁 Найдено: " << imagePaths.size() << " изображений\n";
    
    vector<DetectionParams> paramSets = getParameterSets();
    vector<vector<ImageResult>> allResults(3);
    vector<string> shortNames;
    
    for (const auto& path : imagePaths) {
        shortNames.push_back(fs::path(path).filename().string());
    }
    
    // Обработка
    for (size_t imgIdx = 0; imgIdx < imagePaths.size(); ++imgIdx) {
        cout << "\n🖼️  [" << (imgIdx + 1) << "/" << imagePaths.size() << "] " << shortNames[imgIdx] << "\n";
        
        Mat image = imread(imagePaths[imgIdx]);
        if (image.empty()) continue;
        
        for (int paramIdx = 0; paramIdx < 3; ++paramIdx) {
            vector<int> floorPositions;
            Mat outputImage;
            int linesFound = 0, buildingHeight = 0, typicalGap = 0;
            
            auto start = chrono::high_resolution_clock::now();
            int floors = detectFloors(image, paramSets[paramIdx], floorPositions, 
                                      outputImage, linesFound, buildingHeight, typicalGap);
            auto end = chrono::high_resolution_clock::now();
            double ms = chrono::duration<double, milli>(end - start).count();
            
            ImageResult result;
            result.filename = shortNames[imgIdx];
            result.detectedFloors = floors;
            result.timeMs = ms;
            result.linesFound = linesFound;
            result.buildingHeight = buildingHeight;
            result.typicalGap = typicalGap;
            allResults[paramIdx].push_back(result);
            
            cout << "   " << paramSets[paramIdx].name << ": " << floors << " эт, "
                 << linesFound << " линий, " << ms << " ms\n";
            
            // Сохраняем визуализацию
            string outFile = datasetPath + "/result_" + paramSets[paramIdx].name + "_" + 
                             fs::path(imagePaths[imgIdx]).filename().string();
            imwrite(outFile, outputImage);
        }
    }
    
    printResultsTable(allResults, paramSets, shortNames);
    
    cout << "\n✅ Готово!\n";
    cout << "   📂 Результаты: " << datasetPath << "\n";
    cout << "   🖼️  Визуализации: result_*.jpg\n";
    
    return 0;
}