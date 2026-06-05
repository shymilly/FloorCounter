#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iomanip>

using namespace cv;
using namespace std;
namespace fs = std::filesystem;

// ================================
// СТРУКТУРА ДЛЯ РЕЗУЛЬТАТОВ
// ================================

struct ImageResult {
    string filename;
    int detectedFloors;
    double timeMs;
    int peaksFound;
    int buildingHeight;
};

// ================================
// ПОИСК ПИКОВ
// ================================

vector<int> findPeaks(const vector<float>& data, float threshold, int minDist) {
    vector<int> peaks;
    for (size_t i = 1; i < data.size() - 1; i++) {
        if (data[i] > threshold && data[i] >= data[i-1] && data[i] >= data[i+1]) {
            if (peaks.empty() || (i - peaks.back() >= minDist)) {
                peaks.push_back((int)i);
            }
        }
    }
    return peaks;
}

// ================================
// ОБРЕЗКА ЗДАНИЯ
// ================================

Rect findBuildingRect(const Mat& gray) {
    Mat gradY;
    Sobel(gray, gradY, CV_32F, 0, 1, 3);
    gradY = abs(gradY);
    
    vector<float> rowSum(gray.rows, 0);
    for (int y = 0; y < gray.rows; y++) {
        rowSum[y] = (float)sum(gradY.row(y))[0];
    }
    
    float maxVal = *max_element(rowSum.begin(), rowSum.end());
    if (maxVal > 0) {
        for (float& v : rowSum) v /= maxVal;
    }
    
    int top = 0, bottom = gray.rows - 1;
    for (int y = 0; y < gray.rows; y++) {
        if (rowSum[y] > 0.1f) { top = y; break; }
    }
    for (int y = gray.rows - 1; y >= 0; y--) {
        if (rowSum[y] > 0.1f) { bottom = y; break; }
    }
    
    int height = bottom - top + 1;
    int padding = max(5, height / 20);
    top = max(0, top - padding);
    bottom = min(gray.rows - 1, bottom + padding);
    
    return Rect(0, top, gray.cols, bottom - top + 1);
}

// ================================
// АЛГОРИТМ ПОДСЧЁТА ЭТАЖЕЙ
// ================================

int countFloors_B(const Mat& gray, Mat& outVis, int& peaksFound, int& buildingHeight) {
    const int minPeakDist = 35;
    const float peakThreshold = 0.45f;
    const int smoothWindow = 7;
    const float excludeTopPct = 0.10f;
    const float excludeBottomPct = 0.08f;
    
    Rect buildingRect = findBuildingRect(gray);
    buildingHeight = buildingRect.height;
    
    if (buildingRect.height < 50) return 1;
    
    Mat building = gray(buildingRect);
    
    Mat gradY;
    Sobel(building, gradY, CV_32F, 0, 1, 3);
    gradY = abs(gradY);
    
    vector<float> proj(building.rows, 0.0f);
    for (int y = 0; y < building.rows; y++) {
        proj[y] = (float)sum(gradY.row(y))[0];
    }
    
    float maxProj = *max_element(proj.begin(), proj.end());
    if (maxProj > 0) {
        for (float& v : proj) v /= maxProj;
    }
    
    int half = smoothWindow / 2;
    vector<float> smoothed(proj.size());
    for (size_t i = 0; i < proj.size(); i++) {
        float sum = 0;
        int count = 0;
        for (int j = -half; j <= half; j++) {
            int idx = (int)i + j;
            if (idx >= 0 && idx < (int)proj.size()) {
                sum += proj[idx];
                count++;
            }
        }
        smoothed[i] = sum / count;
    }
    
    vector<int> peaks = findPeaks(smoothed, peakThreshold, minPeakDist);
    
    int excludeTop = (int)(building.rows * excludeTopPct);
    int excludeBottom = (int)(building.rows * excludeBottomPct);
    vector<int> validPeaks;
    for (int p : peaks) {
        if (p > excludeTop && p < building.rows - excludeBottom) {
            validPeaks.push_back(p);
        }
    }
    
    if (validPeaks.empty()) validPeaks = peaks;
    
    peaksFound = validPeaks.size();
    
    int floors = (int)validPeaks.size() + 1;
    if (floors < 1) floors = 1;
    if (floors > 30) floors = 30;
    
    // Визуализация
    outVis = Mat(gray.size(), CV_8UC3);
    cvtColor(gray, outVis, COLOR_GRAY2BGR);
    
    rectangle(outVis, buildingRect, Scalar(255, 0, 0), 2);
    for (int yLocal : validPeaks) {
        int yGlobal = yLocal + buildingRect.y;
        line(outVis, Point(0, yGlobal), Point(outVis.cols - 1, yGlobal), Scalar(0, 0, 255), 2);
    }
    
    string text = "Floors: " + to_string(floors);
    putText(outVis, text, Point(10, 35), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);
    
    string debugText = "Peaks: " + to_string(validPeaks.size()) + " | Height: " + to_string(buildingRect.height) + "px";
    putText(outVis, debugText, Point(10, 65), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255, 255, 0), 1);
    
    return floors;
}

// ================================
// ПОЛУЧЕНИЕ СПИСКА ИЗОБРАЖЕНИЙ ИЗ ПАПКИ
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
// ВЫВОД ТАБЛИЦЫ РЕЗУЛЬТАТОВ
// ================================

void printResultsTable(const vector<ImageResult>& results, const vector<string>& imageNames) {
    cout << "\n\n";
    cout << "╔══════════════════════════════════════════════════════════════════════════════════════╗\n";
    cout << "║                    РЕЗУЛЬТАТЫ ТЕСТИРОВАНИЯ - ПРОЕКЦИЯ ГОРИЗОНТАЛЬНЫХ ГРАНИЦ         ║\n";
    cout << "╚══════════════════════════════════════════════════════════════════════════════════════╝\n";
    cout << "\n";
    
    cout << "┌─────┬──────────────────────────────┬─────────────┬─────────────┬─────────────┬─────────────┐\n";
    cout << "│  №  │           Файл                │   Этажи     │   Пики      │  Высота(px) │  Время(ms)  │\n";
    cout << "├─────┼──────────────────────────────┼─────────────┼─────────────┼─────────────┼─────────────┤\n";
    
    for (size_t i = 0; i < results.size(); ++i) {
        string filename = results[i].filename;
        if (filename.length() > 28) {
            filename = filename.substr(0, 25) + "...";
        }
        
        cout << "│ " << setw(3) << right << (i+1) << " │ " 
             << setw(28) << left << filename << " │ "
             << setw(11) << right << results[i].detectedFloors << " │ "
             << setw(11) << right << results[i].peaksFound << " │ "
             << setw(11) << right << results[i].buildingHeight << " │ "
             << setw(11) << right << fixed << setprecision(1) << results[i].timeMs << " │\n";
    }
    
    cout << "└─────┴──────────────────────────────┴─────────────┴─────────────┴─────────────┴─────────────┘\n";
    
    // Статистика
    double totalTime = 0;
    int totalFloors = 0;
    int totalPeaks = 0;
    
    for (const auto& r : results) {
        totalTime += r.timeMs;
        totalFloors += r.detectedFloors;
        totalPeaks += r.peaksFound;
    }
    
    cout << "\n";
    cout << "╔══════════════════════════════════════════════════════════════════════════════════════╗\n";
    cout << "║                              СТАТИСТИКА ПО ДАТАСЕТУ                                  ║\n";
    cout << "╚══════════════════════════════════════════════════════════════════════════════════════╝\n";
    cout << "\n";
    cout << "   ┌─────────────────────────────────────────────────────────────────────────────┐\n";
    cout << "   │  Всего изображений: " << results.size() << "                                                       │\n";
    cout << "   │  Среднее время: " << fixed << setprecision(2) << (totalTime / results.size()) << " ms                                                │\n";
    cout << "   │  Среднее количество этажей: " << (totalFloors / results.size()) << "                                              │\n";
    cout << "   │  Среднее количество пиков: " << (totalPeaks / results.size()) << "                                              │\n";
    cout << "   │  Всего этажей: " << totalFloors << "                                                         │\n";
    cout << "   └─────────────────────────────────────────────────────────────────────────────┘\n";
}

// ================================
// ГЛАВНАЯ ФУНКЦИЯ
// ================================

int main() {
    cout << "\n";
    cout << "╔══════════════════════════════════════════════════════════════════════════════════════╗\n";
    cout << "║              ПОДСЧЁТ ЭТАЖЕЙ - ПРОЕКЦИЯ ГОРИЗОНТАЛЬНЫХ ГРАНИЦ (АДЕЛЯ)                ║\n";
    cout << "╚══════════════════════════════════════════════════════════════════════════════════════╝\n";
    
    string datasetPath = "/home/shymilly/FloorCounter/dataset";
    
    if (!fs::exists(datasetPath)) {
        cout << "\n❌ ОШИБКА: Папка с датасетом не найдена: " << datasetPath << endl;
        cout << "   Создайте папку и поместите в неё файлы modern_building1.jpg ... modern_building50.jpg\n";
        return 1;
    }
    
    vector<string> imagePaths = getImageList(datasetPath, 50);
    
    if (imagePaths.empty()) {
        cout << "\n❌ ОШИБКА: Не найдено изображений в папке " << datasetPath << endl;
        cout << "   Файлы должны иметь имена: modern_building1.jpg, modern_building2.jpg, ...\n";
        return 1;
    }
    
    cout << "\n📁 Найдено изображений: " << imagePaths.size() << "\n";
    
    vector<ImageResult> results;
    vector<string> shortNames;
    
    // Обработка каждого изображения
    for (size_t imgIdx = 0; imgIdx < imagePaths.size(); ++imgIdx) {
        string imagePath = imagePaths[imgIdx];
        string shortName = fs::path(imagePath).filename().string();
        shortNames.push_back(shortName);
        
        cout << "\n🖼️  [" << (imgIdx + 1) << "/" << imagePaths.size() << "] " << shortName << "\n";
        
        Mat gray = imread(imagePath, IMREAD_GRAYSCALE);
        
        if (gray.empty()) {
            cout << "   ⚠️ Не удалось загрузить изображение\n";
            continue;
        }
        
        // Масштабирование если слишком большое
        if (gray.rows > 800) {
            double scale = 800.0 / gray.rows;
            resize(gray, gray, Size(), scale, scale);
        }
        
        Mat vis;
        int peaksFound = 0;
        int buildingHeight = 0;
        
        auto start = chrono::high_resolution_clock::now();
        int floors = countFloors_B(gray, vis, peaksFound, buildingHeight);
        auto end = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(end - start).count();
        
        ImageResult result;
        result.filename = shortName;
        result.detectedFloors = floors;
        result.timeMs = ms;
        result.peaksFound = peaksFound;
        result.buildingHeight = buildingHeight;
        results.push_back(result);
        
        cout << "   ✅ Этажей: " << floors << ", Пиков: " << peaksFound 
             << ", Высота: " << buildingHeight << "px, Время: " << ms << " ms\n";
        
        // Сохраняем визуализацию
        string outFile = datasetPath + "/result_ADELE_" + shortName;
        imwrite(outFile, vis);
    }
    
    // Вывод таблицы результатов
    printResultsTable(results, shortNames);
    
    cout << "\n✅ ТЕСТИРОВАНИЕ ЗАВЕРШЕНО\n";
    cout << "   📂 Результаты сохранены в папке: " << datasetPath << "\n";
    cout << "   🖼️  Визуализации: result_ADELE_*.jpg\n";
    
    // Сохранение результатов в файл
    ofstream file(datasetPath + "/results_ADELE.txt");
    file << "=== РЕЗУЛЬТАТЫ ТЕСТИРОВАНИЯ - ПРОЕКЦИЯ ГОРИЗОНТАЛЬНЫХ ГРАНИЦ ===\n\n";
    file << "┌─────┬──────────────────────────────┬─────────────┬─────────────┬─────────────┬─────────────┐\n";
    file << "│  №  │           Файл                │   Этажи     │   Пики      │  Высота(px) │  Время(ms)  │\n";
    file << "├─────┼──────────────────────────────┼─────────────┼─────────────┼─────────────┼─────────────┤\n";
    
    for (size_t i = 0; i < results.size(); ++i) {
        char buffer[200];
        sprintf(buffer, "│ %3d │ %-28s │ %11d │ %11d │ %11d │ %11.1f │\n",
                (int)(i+1), results[i].filename.c_str(), 
                results[i].detectedFloors, results[i].peaksFound,
                results[i].buildingHeight, results[i].timeMs);
        file << buffer;
    }
    
    file << "└─────┴──────────────────────────────┴─────────────┴─────────────┴─────────────┴─────────────┘\n";
    file.close();
    
    cout << "   📄 Текстовый результат: results_ADELE.txt\n";
    
    return 0;
}

